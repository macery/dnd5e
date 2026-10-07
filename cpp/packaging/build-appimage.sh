#!/usr/bin/env bash
# Builds a self-contained DnD_Engine-x86_64.AppImage (Qt and other libraries
# bundled) into cpp/packaging/out/.
#
# An AppImage runs on distros whose glibc is the same age or newer than the
# one it was built on, so this is meant to run inside an Ubuntu container
# rather than on a newer host. From the repo root:
#
#   podman run --rm -v "$PWD":/src:Z -w /src ubuntu:24.04 cpp/packaging/build-appimage.sh
#
# Rootless podman maps the container's root to your own user, so the output
# is already yours. With (rootful) docker, add
#   -e HOST_UID=$(id -u) -e HOST_GID=$(id -g)
# so the script hands the files back to you. It can also run directly on an
# Ubuntu 24.04 machine with sudo.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
packaging="$repo_root/cpp/packaging"
work="$packaging/work"
out="$packaging/out"

sudo_cmd=""
[ "$(id -u)" -ne 0 ] && sudo_cmd="sudo"

export DEBIAN_FRONTEND=noninteractive
$sudo_cmd apt-get update
$sudo_cmd apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates wget file \
    libsqlite3-dev qt6-base-dev qt6-qpa-plugins qt6-wayland libgl-dev

# Separate build dir so this never clobbers a host build in cpp/build.
cmake -S "$repo_root/cpp" -B "$work/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$work/build" --parallel
ctest --test-dir "$work/build" --output-on-failure

mkdir -p "$work/tools" "$out"
cd "$work/tools"
for tool in linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage; do
    if [ ! -x "$tool" ]; then
        repo="${tool%-x86_64.AppImage}"
        wget -q "https://github.com/linuxdeploy/$repo/releases/download/continuous/$tool"
        chmod +x "$tool"
    fi
done

# Containers have no FUSE, so run the tool AppImages by extracting them.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/lib/qt6/bin/qmake
export LDAI_OUTPUT="$out/DnD_Engine-x86_64.AppImage"
# Both display backends: Qt picks native Wayland on a Wayland session and X11
# (xcb, always bundled) everywhere else, or when Wayland can't start.
# QT_QPA_PLATFORM=xcb forces X11. "waylandcompositor" brings the client-side
# shell, decoration, and graphics plugins the Wayland platform plugin needs.
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so"
export EXTRA_QT_MODULES="waylandcompositor"

rm -rf "$work/AppDir"
./linuxdeploy-x86_64.AppImage \
    --appdir "$work/AppDir" \
    --executable "$work/build/app/dnd_app" \
    --desktop-file "$packaging/dnd-engine.desktop" \
    --icon-file "$packaging/dnd-engine.svg" \
    --plugin qt \
    --output appimage

# Rootful docker only (see top): hand root-owned output back to the host user.
if [ -n "${HOST_UID:-}" ]; then chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" "$work" "$out"; fi

echo "Built $LDAI_OUTPUT"
