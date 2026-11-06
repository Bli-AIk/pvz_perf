#!/usr/bin/env python3
"""Read-only path spelling audit; does not configure CMake or preprocess C++."""

import argparse
import json
import os
import posixpath
import re
import subprocess
from collections import defaultdict
from pathlib import Path


SOURCE_ROOTS = ("pvzPerf",)
SCAN_SKIP_DIRS = {"tests", "unused", "third_party"}
WALK_SKIP_DIRS = {".dxc-check", "__pycache__", "out", "build"}
CPP_SUFFIXES = {".cpp", ".h", ".hpp", ".c", ".inl", ".inc"}
PATH_SUFFIXES = CPP_SUFFIXES | {".cmake", ".rc", ".cur", ".hlsl", ".hlsli",
                              ".frag", ".comp", ".vert"}
INCLUDE_ROOTS = ("", "pvzPerf", "pvzPerf/SexyAppFramework", "pvzPerf/Sexy.TodLib",
                 "pvzPerf/SexyAppFramework/platform", "pvzPerf/SexyAppFramework/graphics/vulkan",
                 "pvzPerf/SexyAppFramework/graphics/d3d12", "pvzPerf/SexyAppFramework/effect")
CPP_LEXEMES = re.compile(
    r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',
    re.DOTALL,
)
CMAKE_TOKENS = re.compile(
    r'\#\[(?P<equals>=*)\[.*?\](?P=equals)\]|\#[^\n]*'
    r'|"(?:\\.|[^"\\])*"|\[(?P<bracket>=*)\[.*?\](?P=bracket)\]'
    r'|[()]|[^\s()"]+',
    re.DOTALL,
)


def uncomment_cpp(text):
    def replace(match):
        token = match.group()
        if token.startswith(("//", "/*")):
            return re.sub(r"[^\r\n]", " ", token)
        return token
    return CPP_LEXEMES.sub(replace, text)


def files_below(root, directory):
    # scandir/os.walk names preserve directory-entry spelling on NTFS/DrvFS.
    for current, dirs, names in os.walk(root / directory):
        dirs[:] = sorted(d for d in dirs if d not in WALK_SKIP_DIRS and not d.startswith("."))
        for name in sorted(names):
            yield (Path(current) / name).relative_to(root).as_posix()


def inventory(root):
    files = []
    for name in os.listdir(root):
        if (root / name).is_file():
            files.append(name)
    for directory in (*SOURCE_ROOTS, "CMake", "tools", "effects"):
        files.extend(files_below(root, directory))
    return sorted(set(files))


def normalized(base, spelling):
    return posixpath.normpath(posixpath.join(base, spelling.replace("\\", "/")))


def run(root, runtime_literals):
    files = inventory(root)
    exact = set(files)
    directories = {posixpath.dirname(p) for p in files}
    for directory in list(directories):
        while "/" in directory:
            directory = posixpath.dirname(directory)
            directories.add(directory)
    all_paths = exact | directories
    folded = defaultdict(list)
    for path in sorted(all_paths):
        folded[path.casefold()].append(path)
    findings, unresolved, runtime = [], [], []
    counts = {"cpp_files": 0, "includes": 0, "cmake_files": 0, "cmake_paths": 0}

    def check(kind, source, line, spelling, bases, optional=False):
        normalized_candidates = [normalized(base, spelling) for base in bases]
        resolved = next((p for p in normalized_candidates if p in all_paths), None)
        if resolved is not None:
            if "\\" in spelling:
                findings.append(dict(kind=kind, file=source, line=line,
                                     path=spelling, issue="backslash", actual=[resolved]))
            return
        matches = []
        for candidate in normalized_candidates:
            matches.extend(folded.get(candidate.casefold(), []))
        matches = sorted(set(matches))
        if matches:
            findings.append(dict(kind=kind, file=source, line=line, path=spelling,
                                 issue="case-mismatch", actual=matches))
        elif not optional:
            unresolved.append(dict(kind=kind, file=source, line=line, path=spelling))

    for source in files:
        suffix = posixpath.splitext(source)[1]
        parts = source.split("/")
        if (suffix not in CPP_SUFFIXES
                or (len(parts) > 1 and not any(source.startswith(prefix + "/")
                                             for prefix in SOURCE_ROOTS))
                or SCAN_SKIP_DIRS.intersection(parts[:-1])):
            continue
        counts["cpp_files"] += 1
        text = (root / source).read_text(encoding="utf-8-sig", errors="replace")
        clean = uncomment_cpp(text)
        for match in re.finditer(
            r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"\r\n]+)[>"]', clean, re.MULTILINE
        ):
            counts["includes"] += 1
            spelling = match[2]
            bases = list(INCLUDE_ROOTS)
            if match[1] == '"':
                bases.insert(0, posixpath.dirname(source))
            check("include", source, clean.count("\n", 0, match.start()) + 1,
                  spelling, bases, optional=match[1] == "<")
        for match in re.finditer(
            r'^[ \t]*#[ \t]*include[ \t]+([A-Za-z_]\w*)[ \t]*$',
            clean, re.MULTILINE
        ):
            unresolved.append(dict(kind="include-macro", file=source,
                                   line=clean.count("\n", 0, match.start()) + 1,
                                   path=match[1]))
        if runtime_literals:
            for match in re.finditer(r'"(?:\\.|[^"\\])*"', clean):
                value = match.group()[1:-1]
                prefix = clean[clean.rfind("\n", 0, match.start()) + 1:match.start()]
                if re.match(r"\s*#\s*include\b", prefix):
                    continue
                if (re.search(r"\.(exe|dll)\b", value, re.IGNORECASE)
                        or ("%" not in value and
                            re.search(r"[A-Za-z0-9_.]\\\\[A-Za-z0-9_.]", value))):
                    runtime.append(dict(file=source,
                                        line=clean.count("\n", 0, match.start()) + 1,
                                        literal=value))

    for source in files:
        if posixpath.basename(source) != "CMakeLists.txt" and not source.endswith(".cmake"):
            continue
        counts["cmake_files"] += 1
        text = (root / source).read_text(encoding="utf-8-sig", errors="replace")
        base = posixpath.dirname(source)
        command, args, depth = "", [], 0
        previous = None

        def inspect(name, tokens):
            for index, (value, line) in enumerate(tokens):
                value = value.strip('"')
                # A literal extension path is checked in any command, including
                # set(source_list ...), target_sources and set_property(SOURCE).
                is_path = posixpath.splitext(value)[1] in PATH_SUFFIXES
                if name in {"include", "add_subdirectory"} and index == 0:
                    if "/" in value or "\\" in value or value.endswith(".cmake"):
                        is_path = True
                    elif name == "add_subdirectory":
                        is_path = True
                if not is_path:
                    continue
                for variable, replacement in (
                    ("CMAKE_CURRENT_LIST_DIR", base or "."),
                    ("CMAKE_CURRENT_SOURCE_DIR", base or "."),
                    ("CMAKE_SOURCE_DIR", "."),
                    ("PVZ_GAME_SOURCE_DIR", "pvzPerf"),
                    ("PROJECT_SOURCE_DIR", "."),
                ):
                    value = value.replace("${" + variable + "}", replacement)
                # These variables are root-relative after substitution.
                expanded = any("${" + v + "}" in tokens[index][0] for v in (
                    "CMAKE_CURRENT_LIST_DIR", "CMAKE_CURRENT_SOURCE_DIR",
                    "CMAKE_SOURCE_DIR", "PROJECT_SOURCE_DIR"))
                if "$" in value or "*" in value or ";" in value:
                    unresolved.append(dict(kind="cmake-dynamic", file=source,
                                           line=line, path=tokens[index][0].strip('"')))
                    continue
                if re.match(r"^(?:[A-Za-z]:|/)", value):
                    unresolved.append(dict(kind="cmake-external", file=source,
                                           line=line, path=value))
                    continue
                counts["cmake_paths"] += 1
                check("cmake", source, line, value, [""] if expanded else [base],
                      optional=name == "include" and any(v == "OPTIONAL" for v, _ in tokens))

        for match in CMAKE_TOKENS.finditer(text):
            value = match.group()
            if value.startswith("#"):
                continue
            line = text.count("\n", 0, match.start()) + 1
            if value == "(":
                if depth == 0:
                    command = previous.lower() if previous else ""
                    args = []
                depth += 1
            elif value == ")":
                depth -= 1
                if depth == 0:
                    inspect(command, args)
            elif depth:
                args.append((value, line))
            previous = value

    index_spelling = []
    git_note = None
    try:
        tracked = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True, check=True,
        ).stdout.decode("utf-8").split("\0")
        for path in tracked:
            if path and path not in all_paths and path.casefold() in folded:
                index_spelling.append(dict(path=path, actual=folded[path.casefold()]))
    except (OSError, subprocess.CalledProcessError, UnicodeError) as error:
        git_note = str(error)
    return dict(
        root=str(root), counts=counts, findings=findings, unresolved=unresolved,
        case_collisions=[v for v in folded.values() if len(v) > 1],
        git_index_spelling=index_spelling, git_note=git_note,
        runtime_literals=runtime,
        limitations=[
            "Static superset of production directories; no C++/CMake conditional evaluation.",
            "Quoted includes unresolved in repository roots may be generated or external.",
            "Include roots are a documented repository search superset, not compiler flags.",
            "CMake variables/genex/generated outputs are not fully evaluated; see unresolved.",
            "Runtime literals are candidates only; platform guards and I/O normalization need review.",
            "No game build, runtime asset validation, or native ext4 execution is performed.",
        ],
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).absolute().parent.parent)
    parser.add_argument("--json", action="store_true", help="Print all results as JSON.")
    parser.add_argument("--runtime-literals", action="store_true",
                        help="Also list Windows suffix/backslash string candidates.")
    parser.add_argument("--fail-on-findings", action="store_true",
                        help="Exit 1 for spelling findings/collisions; unresolved is separate.")
    args = parser.parse_args()
    result = run(args.root.absolute(), args.runtime_literals)
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=True))
    else:
        print(json.dumps(result["counts"], sort_keys=True))
        for item in result["findings"]:
            print(f'{item["file"]}:{item["line"]}: {item["kind"]} {item["issue"]}: '
                  f'{item["path"]} -> {", ".join(item["actual"])}')
        for item in result["unresolved"]:
            print(f'{item["file"]}:{item["line"]}: unresolved {item["kind"]}: {item["path"]}')
        for paths in result["case_collisions"]:
            print("case-collision: " + ", ".join(paths))
        for item in result["git_index_spelling"]:
            print(f'git-index-spelling: {item["path"]} -> {", ".join(item["actual"])}')
        if result["git_note"]:
            print("Git index comparison unavailable: " + result["git_note"])
        for item in result["runtime_literals"]:
            print(f'{item["file"]}:{item["line"]}: runtime-candidate: {item["literal"]}')
        print(f'Findings={len(result["findings"])}; unresolved={len(result["unresolved"])}; '
              f'case-collisions={len(result["case_collisions"])}; '
              f'git-index-spelling={len(result["git_index_spelling"])}')
        for limitation in result["limitations"]:
            print("LIMIT: " + limitation)
    return int(args.fail_on_findings and bool(
        result["findings"] or result["case_collisions"] or result["git_index_spelling"]))


if __name__ == "__main__":
    raise SystemExit(main())
