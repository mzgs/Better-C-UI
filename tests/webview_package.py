#!/usr/bin/env python3
"""Install an existing build and check core/add-on consumer boundaries."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*command):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        result.check_returncode()


build = Path(sys.argv[1]).resolve()
enabled = "CUI_WEBVIEW:BOOL=ON" in (build / "CMakeCache.txt").read_text()
with tempfile.TemporaryDirectory(prefix="cui-webview-package-") as scratch:
    scratch = Path(scratch)
    prefix = scratch / "install"
    run("cmake", "--install", str(build), "--prefix", str(prefix), "--config", "Debug")
    for mode in ("core", "optional", "required"):
        source = scratch / mode
        source.mkdir()
        component = {"core": "", "optional": "OPTIONAL_COMPONENTS webview",
                     "required": "COMPONENTS webview"}[mode]
        use_addon = mode != "core" and enabled
        (source / "main.c").write_text(
            "#include <cui/cui_webview.h>\nint main(void) { return cui_webview_get_status(0) != CUI_WEBVIEW_UNAVAILABLE; }\n"
            if use_addon else
            "#include <cui/cui_app.h>\nint main(void) { cui_ctx *ui = cui_create(0); if (!ui) return 1; cui_destroy(ui); return 0; }\n")
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14)\nproject(consumer C)\n"
            f"find_package(cui REQUIRED CONFIG {component})\n"
            "get_target_property(core_links cui::cui INTERFACE_LINK_LIBRARIES)\n"
            "if(core_links MATCHES \"WebKit|WebView2|CUI_WEBKIT|cui_webview|cui::webview\")\n"
            "  message(FATAL_ERROR \"Browser dependency leaked into the core\")\nendif()\n"
            + ("if(NOT TARGET cui::webview)\n  message(FATAL_ERROR \"Missing add-on target\")\nendif()\n"
               if use_addon else
               "if(TARGET cui::webview)\n  message(FATAL_ERROR \"Unexpected add-on target\")\nendif()\n")
            + f"add_executable(consumer main.c)\ntarget_link_libraries(consumer PRIVATE {'cui::webview' if use_addon else 'cui::cui'})\n")
        command = ["cmake", "-S", str(source), "-B", str(source / "build"),
                   f"-DCMAKE_PREFIX_PATH={prefix}", "-DCMAKE_BUILD_TYPE=Debug"]
        if mode == "required" and not enabled:
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            assert result.returncode and "WebView add-on" in result.stdout, result.stdout
        else:
            run(*command)
            run("cmake", "--build", str(source / "build"), "--config", "Debug")
    core_pc = next(prefix.rglob("cui.pc")).read_text()
    assert not any(name in core_pc for name in ("WebKit", "WebView2", "webkit2gtk", "cui_webview"))
print(f"webview package: core isolation, component discovery and consumer linking passed (add-on {'enabled' if enabled else 'disabled'})")
