"""
PlatformIO extra script to configure SDL2 include and library paths
for the LVGL desktop simulator build.

Requires SDL2 development libraries:
  Windows (MSYS2): pacman -S mingw-w64-x86_64-SDL2
  macOS:           brew install sdl2
  Linux:           sudo apt install libsdl2-dev
"""
import subprocess
import os
import sys

Import("env")


def configure_sdl2_msys2(env):
    """Configure SDL2 from known MSYS2 paths (Windows)."""
    for prefix in [
        r"C:\msys64\mingw64",
        r"C:\msys64\ucrt64",
        r"C:\msys64\mingw32",
        r"C:\msys64\clang64",
    ]:
        include_dir = os.path.join(prefix, "include", "SDL2")
        include_parent = os.path.join(prefix, "include")
        lib_dir = os.path.join(prefix, "lib")
        if os.path.isdir(include_dir) and os.path.isdir(lib_dir):
            env.Append(CPPPATH=[include_dir, include_parent])
            env.Append(LIBPATH=[lib_dir])
            env.Append(LIBS=["mingw32", "SDL2main", "SDL2", "ws2_32"])
            env.Append(LINKFLAGS=["-mconsole"])
            return True
    return False


def configure_sdl2_unix(env):
    """Configure SDL2 using sdl2-config (macOS/Linux)."""
    try:
        cflags = subprocess.check_output(
            ["sdl2-config", "--cflags"], text=True
        ).strip().split()
        libs = subprocess.check_output(
            ["sdl2-config", "--libs"], text=True
        ).strip().split()
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return False

    for flag in cflags:
        if flag.startswith("-I"):
            sdl_path = flag[2:]
            env.Append(CPPPATH=[sdl_path])
            parent = os.path.dirname(sdl_path)
            if parent and parent != sdl_path:
                env.Append(CPPPATH=[parent])
        elif flag.startswith("-D"):
            env.Append(CPPDEFINES=[flag[2:]])

    for flag in libs:
        if flag.startswith("-L"):
            env.Append(LIBPATH=[flag[2:]])
        elif flag.startswith("-l"):
            env.Append(LIBS=[flag[2:]])
        else:
            env.Append(LINKFLAGS=[flag])

    return True


def configure_sdl2(env):
    if sys.platform == "win32":
        if configure_sdl2_msys2(env):
            return
    else:
        if configure_sdl2_unix(env):
            return

    sys.stderr.write(
        "\nError: SDL2 not found. Install SDL2 development libraries:\n"
        "  Windows (MSYS2): pacman -S mingw-w64-x86_64-SDL2\n"
        "  macOS:           brew install sdl2\n"
        "  Linux:           sudo apt install libsdl2-dev\n\n"
    )
    env.Exit(1)


configure_sdl2(env)
