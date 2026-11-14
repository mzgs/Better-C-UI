#!/usr/bin/env python3
"""Verify relocation, signatures and launch while Homebrew reads are denied."""
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile


def check_bundle(bundle):
    bundle = Path(bundle).resolve(strict=True)
    with (bundle / "Contents/Info.plist").open("rb") as file:
        name = plistlib.load(file)["CFBundleExecutable"]
    executable = bundle / "Contents/MacOS" / name
    libraries = list((bundle / "Contents/Frameworks").glob("*.dylib"))
    assert any(path.name.startswith("libmpv.") for path in libraries), "libmpv was not embedded"
    assert (bundle / "Contents/Resources/licenses/cui/LICENSE").is_file()
    subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
    for binary in [executable, *libraries]:
        output = subprocess.check_output(["/usr/bin/otool", "-L", str(binary)], text=True)
        for line in output.splitlines()[1:]:
            dependency = line.strip().split(" (compatibility version", 1)[0]
            if dependency.startswith(("/System/", "/usr/lib/")):
                continue
            if dependency.startswith("@executable_path/"):
                resolved = executable.parent / dependency[len("@executable_path/"):]
            elif dependency.startswith("@loader_path/"):
                resolved = binary.parent / dependency[len("@loader_path/"):]
            else:
                raise AssertionError(f"External dependency in {binary.name}: {dependency}")
            assert bundle in resolved.resolve(strict=True).parents, f"Dependency escapes bundle: {resolved}"

    with tempfile.TemporaryDirectory(prefix="cui video relocation ") as directory:
        temporary = Path(directory)
        relocated = temporary / "Player with spaces.app"
        shutil.copytree(bundle, relocated, symlinks=True)
        media = temporary / "sample.y4m"
        with media.open("wb") as file:
            file.write(b"YUV4MPEG2 W32 H18 F24:1 Ip A1:1 C420jpeg\n")
            for _ in range(96):
                file.write(b"FRAME\n" + bytes([81]) * 576 + bytes([90]) * 144 + bytes([240]) * 144)
        # Block both Homebrew prefixes without moving or changing the installed SDK.
        profile = '(version 1)(allow default)(deny file-read* (subpath "/opt/homebrew") (subpath "/usr/local"))'
        environment = {key: value for key, value in os.environ.items() if not key.startswith("DYLD_")}
        for renderer in ("opengl", "software"):
            screenshot = temporary / f"{renderer}.png"
            environment.update(CUI_RENDERER=renderer, CUI_SCREENSHOT=str(screenshot), CUI_SCREENSHOT_FRAMES="45")
            result = subprocess.run(["/usr/bin/sandbox-exec", "-p", profile,
                str(relocated / "Contents/MacOS" / name), str(media)],
                env=environment, capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, f"{renderer} launch failed:\n{result.stdout}\n{result.stderr}"
            assert screenshot.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", f"{renderer} screenshot missing"
    print(f"video bundle: {len(libraries)} libraries; relocated OpenGL/software launches passed with Homebrew access denied")


if __name__ == "__main__":
    check_bundle(sys.argv[1])
