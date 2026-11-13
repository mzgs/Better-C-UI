#!/usr/bin/env python3
"""Run the native smoke test against two local pages, without Internet access."""
import http.server
import subprocess
import sys
import tempfile
import threading


class HistoryPages(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path not in ("/first", "/second"):
            self.send_error(404)
            return
        body = ("<!doctype html><meta charset='utf-8'><title>History test</title>"
                f"<h1>{self.path[1:]}</h1><input aria-label='History page input'>").encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


with http.server.ThreadingHTTPServer(("127.0.0.1", 0), HistoryPages) as server:
    threading.Thread(target=server.serve_forever, daemon=True).start()
    with tempfile.TemporaryDirectory(prefix="cui-history-") as scratch:
        result = subprocess.run([sys.argv[1], scratch + "/snapshot.png",
                                 f"http://127.0.0.1:{server.server_port}"], timeout=90)
    server.shutdown()
    sys.exit(result.returncode)
