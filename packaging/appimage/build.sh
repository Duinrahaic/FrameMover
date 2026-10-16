#!/bin/sh
# Builds the installer AppImage for the Steam Frame (Linux aarch64): ./packaging/appimage/build.sh [OUTPUT]
# Must run on an aarch64 machine (the Frame itself works): the program links against the aarch64 SteamVR
# runtime, so it cannot be cross-built from an x86-64 host. Needs cmake, a C++ toolchain, the build dependencies
# from the README, and network access the first time (downloads appimagetool into the build directory unless
# it is already on PATH).
set -eu

ARCH=aarch64
if [ "$(uname -m)" != "$ARCH" ]; then
	echo "this must be built on $ARCH (the Steam Frame), not $(uname -m)" >&2
	exit 1
fi

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${BUILD_DIR:-$ROOT/build}
OUT=${1:-$BUILD/FrameMover-installer-$ARCH.AppImage}
APPDIR=$BUILD/AppDir

cmake -S "$ROOT" -B "$BUILD" >/dev/null
cmake --build "$BUILD" -j --target frame-mover

# The program finds its data directory at <exe>/../data, so usr/bin and usr/data sit side by side.
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
cp "$BUILD/frame-mover" "$APPDIR/usr/bin/frame-mover"
cp -r "$ROOT/data" "$APPDIR/usr/data"
cp "$ROOT/packaging/appimage/AppRun" "$ROOT/packaging/appimage/frame-mover.desktop" "$ROOT/packaging/appimage/frame-mover.svg" "$APPDIR/"
chmod +x "$APPDIR/AppRun"

TOOL=$(command -v appimagetool || true)
if [ -z "$TOOL" ]; then
	TOOL=$BUILD/appimagetool-$ARCH.AppImage
	if [ ! -x "$TOOL" ]; then
		curl -fL -o "$TOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"
		chmod +x "$TOOL"
	fi
fi

# Extract-and-run so building does not need FUSE.
ARCH=$ARCH APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" "$APPDIR" "$OUT"

# Refuse to hand out an image that would not start on the Frame.
for f in "$APPDIR/usr/bin/frame-mover" "$OUT"; do
	if ! file "$f" | grep -q "ARM aarch64"; then
		echo "$f is not an aarch64 ELF" >&2
		exit 1
	fi
done
echo "built $OUT"
