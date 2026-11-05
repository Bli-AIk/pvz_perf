"""Serve the bundled game locally; no third-party Python packages required."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import webbrowser


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=18088)
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()
    handler = partial(SimpleHTTPRequestHandler, directory=str(Path(__file__).resolve().parent))
    try:
        server = ThreadingHTTPServer((args.host, args.port), handler)
    except OSError as error:
        raise SystemExit(f'Cannot start local server: {error}. Try --port 18089.') from error
    url = f'http://127.0.0.1:{server.server_port}/'
    print(f'pvz_perf: {url}\nKeep this window open. Ctrl+C stops the server.', flush=True)
    if not args.no_browser:
        threading.Timer(.2, webbrowser.open, args=(url,)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
