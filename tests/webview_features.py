#!/usr/bin/env python3
"""Exercise browser APIs with loopback-only pages and no external requests."""
import http.server
import json
import os
from pathlib import Path
import plistlib
import shutil
import socket
import subprocess
import sys
import threading
import tempfile
import uuid


class FeaturePages(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/broken":
            self.connection.shutdown(socket.SHUT_RDWR)
            self.connection.close()
            return
        if self.path.split("?")[0] != "/features":
            self.send_error(404)
            return
        headers_ok = (self.headers.get("X-CUI-Test") == "cui-headers" and
                      self.headers.get("User-Agent") == "CUI-WebView-Test/1")
        body = ("<!doctype html><meta charset='utf-8'><title>WebView features</title>"
                "<h1>WebView features</h1><script>window.testHeaders=" + json.dumps(headers_ok) + ";"
                "window.cui.postMessage('from-page');</script>"
                "<iframe srcdoc=\"<script>"
                "try{window.webkit.messageHandlers.cui.postMessage('from-iframe')}catch(e){};"
                "try{window.chrome.webview.postMessage('from-iframe')}catch(e){};"
                "window.top.postMessage({__cui:'from-iframe'},'*');"
                "</script>\"></iframe>").encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Set-Cookie", "cui_test=present; Path=/; SameSite=Lax")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


with tempfile.TemporaryDirectory(prefix="cui-webview-features-") as scratch, \
        http.server.ThreadingHTTPServer(("127.0.0.1", 0), FeaturePages) as server:
    executable = Path(sys.argv[1]).resolve()
    env = dict(os.environ, XDG_DATA_HOME=str(Path(scratch) / "data"),
               XDG_CACHE_HOME=str(Path(scratch) / "cache"))
    # Cookie clearing must use a test-owned store, including native defaults.
    if sys.platform == "darwin":
        contents = Path(scratch) / "Features.app" / "Contents"
        (contents / "MacOS").mkdir(parents=True)
        target = contents / "MacOS" / executable.name
        shutil.copy2(executable, target)
        with (contents / "Info.plist").open("wb") as output:
            plistlib.dump({"CFBundleExecutable": executable.name, "CFBundlePackageType": "APPL",
                          "CFBundleIdentifier": "org.better-c-ui.features." + uuid.uuid4().hex}, output)
        executable = target
    elif sys.platform == "win32":
        target = Path(scratch) / executable.name
        shutil.copy2(executable, target)
        for library in executable.parent.glob("*.dll"):
            shutil.copy2(library, Path(scratch) / library.name)
        executable = target
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        result = subprocess.run([str(executable), f"http://127.0.0.1:{server.server_port}"], env=env, timeout=120)
    finally:
        server.shutdown()
    sys.exit(result.returncode)
