#!/usr/bin/env python3
"""
BloxBot Build Script
Creates a standalone executable using PyInstaller.
"""

import subprocess
import sys
import os


def build():
    print("=" * 60)
    print("  BloxBot Build Script")
    print("=" * 60)
    print()

    print("[1/4] Checking dependencies...")
    try:
        import PyInstaller
        print(f"  PyInstaller {PyInstaller.__version__} found")
    except ImportError:
        print("  PyInstaller not found. Installing...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyinstaller"])

    print("[2/4] Cleaning previous builds...")
    for d in ["build", "dist"]:
        if os.path.exists(d):
            import shutil
            shutil.rmtree(d)
            print(f"  Removed {d}/")

    print("[3/4] Building executable...")
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--name=BloxBot",
        "--onefile",
        "--windowed",
        "--icon=assets/icon.ico" if os.path.exists("assets/icon.ico") else "",
        "--add-data=src;src",
        "--hidden-import=PyQt6",
        "--hidden-import=PyOpenGL",
        "--hidden-import=OpenGL",
        "--hidden-import=OpenGL.GL",
        "--hidden-import=OpenGL.GLU",
        "--hidden-import=numpy",
        "--hidden-import=PIL",
        "--collect-all=PyQt6",
        "--collect-all=OpenGL",
        "main.py"
    ]
    cmd = [c for c in cmd if c]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("  Build failed!")
        print(result.stderr)
        return False

    print("[4/4] Build complete!")
    exe_path = os.path.join("dist", "BloxBot.exe")
    if os.path.exists(exe_path):
        size_mb = os.path.getsize(exe_path) / (1024 * 1024)
        print(f"  Executable: {exe_path}")
        print(f"  Size: {size_mb:.1f} MB")
    else:
        print(f"  Executable built in dist/ folder")

    print()
    print("=" * 60)
    print("  Build Successful!")
    print("=" * 60)
    return True


if __name__ == "__main__":
    build()
