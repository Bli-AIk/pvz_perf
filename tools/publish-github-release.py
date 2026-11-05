"""Publish a source-only release or verified assets using the Git credential helper."""

import argparse
import hashlib
import json
from pathlib import Path
import os
import subprocess
import tempfile
import urllib.error
import urllib.parse
import urllib.request


def resolve_release(request, root, tag, commit, notes):
    try:
        return request("GET", root + "/releases/tags/" + urllib.parse.quote(tag))
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
        error.close()
    # GitHub's by-tag endpoint does not expose draft releases. Recover an
    # interrupted upload through the authenticated release list before creating.
    matches = []
    page = 1
    while True:
        releases = request("GET", root + f"/releases?per_page=100&page={page}")
        matches.extend(release for release in releases if release["tag_name"] == tag)
        if len(releases) < 100:
            break
        page += 1
    if matches:
        if any(release["target_commitish"] != commit for release in matches):
            raise RuntimeError("Existing draft targets a different source commit")
        return max(matches, key=lambda release: (
            sum(asset.get("state") == "uploaded" for asset in release.get("assets", [])),
            release["id"]))
    return request("POST", root + "/releases", {
        "tag_name": tag, "target_commitish": commit, "name": tag,
        "body": notes, "draft": True, "prerelease": False})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", default="YuHao2006/pvz_perf")
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--proxy", help="Explicit HTTP proxy; uses system curl TLS transport")
    parser.add_argument("--tag")
    parser.add_argument("--commit")
    parser.add_argument("--notes", type=Path)
    parser.add_argument("--asset", type=Path, action="append", default=[])
    parser.add_argument("--replace-asset", action="append", default=[],
                        help="Explicitly replace a changed attachment with this exact name")
    parser.add_argument("--source-only", action="store_true",
                        help="Publish tag source archives without uploading attachments")
    args = parser.parse_args()
    if not set(args.replace_asset) <= {path.name for path in args.asset}:
        parser.error("Every --replace-asset must also be supplied through --asset")
    if args.source_only and args.asset:
        parser.error("--source-only cannot be combined with --asset")
    environment = dict(os.environ, GIT_TERMINAL_PROMPT="0", GCM_INTERACTIVE="never")
    credential = subprocess.run(["git", "credential", "fill"],
        input="protocol=https\nhost=github.com\n\n", text=True, capture_output=True,
        env=environment, check=True)
    values = dict(line.split("=", 1) for line in credential.stdout.splitlines() if "=" in line)
    token = values["password"]
    headers = {"Authorization": "Bearer " + token,
               "Accept": "application/vnd.github+json", "User-Agent": "pvz-perf-release",
               "X-GitHub-Api-Version": "2022-11-28"}
    root = "https://api.github.com/repos/" + args.repository

    def request(method, url, data=None, content_type="application/json"):
        body = json.dumps(data).encode() if data is not None and not isinstance(data, bytes) else data
        request_headers = dict(headers)
        if body is not None:
            request_headers["Content-Type"] = content_type
        if args.proxy:
            # Credentials go through stdin, never command arguments or log files.
            config = "".join("header = " + json.dumps(k + ": " + v) + "\n"
                             for k, v in request_headers.items())
            with tempfile.TemporaryDirectory(prefix="pvz-release-http-") as tmp:
                result_file = Path(tmp) / "response.json"
                command = ["curl.exe", "--silent", "--show-error", "--http1.1", "--tls-max", "1.2", "--retry", "4", "--retry-all-errors", "--retry-delay", "2", "--proxy", args.proxy,
                           "--config", "-", "--request", method, "--max-time", "300",
                           "--output", str(result_file), "--write-out", "%{http_code}", url]
                if body is not None:
                    payload = Path(tmp) / "payload"
                    payload.write_bytes(body)
                    command += ["--data-binary", "@" + str(payload)]
                result = subprocess.run(command, input=config, capture_output=True, text=True, check=True)
                status = int(result.stdout)
                if status >= 400:
                    raise urllib.error.HTTPError(url, status, "GitHub API request failed", {}, None)
                return None if status == 204 else json.loads(result_file.read_text(encoding="utf-8"))
        req = urllib.request.Request(url, data=body, headers=request_headers, method=method)
        with urllib.request.urlopen(req, timeout=300) as response:
            return None if response.status == 204 else json.load(response)

    repository = request("GET", root)
    if not repository.get("permissions", {}).get("push"):
        raise RuntimeError("The configured account cannot publish to this repository")
    if args.inspect:
        print(json.dumps({"repository": repository["full_name"],
                          "default_branch": repository["default_branch"],
                          "push": True}, indent=2))
        return
    if not args.tag or not args.commit or args.notes is None or (not args.asset and not args.source_only):
        parser.error("Publishing needs --tag, --commit, --notes and either --asset or --source-only")
    commit = subprocess.check_output(["git", "rev-parse", args.commit], text=True).strip()
    if subprocess.check_output(["git", "rev-parse", args.tag + "^{commit}"], text=True).strip() != commit:
        raise RuntimeError("Release tag and source commit differ")
    for path in args.asset:
        if not path.is_file():
            raise FileNotFoundError(path)
    notes = args.notes.read_text(encoding="utf-8")
    release = resolve_release(request, root, args.tag, commit, notes)
    upload_root = release["upload_url"].split("{", 1)[0]
    existing = {asset["name"]: asset for asset in release["assets"]}
    if args.source_only and existing:
        raise RuntimeError("Source-only publishing refuses a release with existing attachments")
    uploaded = []
    for path in args.asset:
        contents = path.read_bytes()
        digest = "sha256:" + hashlib.sha256(contents).hexdigest()
        asset = existing.get(path.name)
        if asset is not None and asset.get("digest") != digest:
            if path.name not in args.replace_asset:
                raise RuntimeError("A different release asset already exists: " + path.name)
            request("DELETE", root + "/releases/assets/" + str(asset["id"]))
            asset = None
        if asset is None:
            content_type = ("application/vnd.android.package-archive"
                            if path.suffix.lower() == ".apk" else "application/zip")
            asset = request("POST", upload_root + "?name=" + urllib.parse.quote(path.name),
                            contents, content_type)
        if asset["size"] != len(contents) or asset.get("digest") not in (None, digest):
            raise RuntimeError("Uploaded asset verification failed: " + path.name)
        uploaded.append({"name": asset["name"], "bytes": asset["size"]})
        print("Verified upload: " + path.name, flush=True)
    published = request("PATCH", root + "/releases/" + str(release["id"]),
                        {"body": notes, "name": args.tag, "target_commitish": commit,
                         "draft": False, "prerelease": False})
    print(json.dumps({"release": published["html_url"], "commit": commit,
                      "assets": uploaded}, indent=2))


if __name__ == "__main__":
    main()
