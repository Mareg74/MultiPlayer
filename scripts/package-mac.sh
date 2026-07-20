#!/usr/bin/env bash
# Build + package MultiPlayer.app for macOS release (branch: mac).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="${MP_VERSION:-$(grep -E '^project\(MultiPlayer VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
OUT_DIR="${OUT_DIR:-$ROOT/dist/mac}"
APP="$BUILD_DIR/MultiPlayer.app"
ZIP="$OUT_DIR/MultiPlayer-${VERSION}-macOS.zip"

QT_PREFIX="${CMAKE_PREFIX_PATH:-$(brew --prefix qt 2>/dev/null || brew --prefix qt@6 2>/dev/null || true)}"
if [[ -z "${QT_PREFIX}" || ! -d "${QT_PREFIX}" ]]; then
  echo "Qt prefix not found. Set CMAKE_PREFIX_PATH." >&2
  exit 1
fi

MACDEPLOYQT="${MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}"
if [[ ! -x "$MACDEPLOYQT" ]]; then
  # Homebrew sometimes puts it in libexec
  MACDEPLOYQT="$(find "$QT_PREFIX" -name macdeployqt -type f 2>/dev/null | head -1 || true)"
fi
if [[ -z "$MACDEPLOYQT" || ! -x "$MACDEPLOYQT" ]]; then
  echo "macdeployqt not found under $QT_PREFIX" >&2
  exit 1
fi

echo "==> Configure ($VERSION)"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PREFIX"

echo "==> Build"
cmake --build "$BUILD_DIR" --config Release -j "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [[ ! -d "$APP" ]]; then
  echo "Missing $APP" >&2
  exit 1
fi

echo "==> macdeployqt"
set +e
"$MACDEPLOYQT" "$APP" -always-overwrite
MACDEPLOY_STATUS=$?
set -e
if [[ $MACDEPLOY_STATUS -ne 0 ]]; then
  echo "WARNING: macdeployqt exited $MACDEPLOY_STATUS (continuing; Homebrew Qt is often noisy)" >&2
fi

# Ad-hoc sign so Gatekeeper is less angry on local/CI unsigned builds
if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - "$APP" 2>/dev/null || true
fi

FRAMEWORKS="$APP/Contents/Frameworks"
MACOS_DIR="$APP/Contents/MacOS"
mkdir -p "$FRAMEWORKS"

NDI_SRC=""
for cand in \
  "$ROOT/third_party/NDI/runtime/libndi.dylib" \
  "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib" \
  "$HOME/NDI SDK for Apple/lib/macOS/libndi.dylib"
do
  if [[ -f "$cand" ]]; then
    NDI_SRC="$cand"
    break
  fi
done

if [[ -n "$NDI_SRC" ]]; then
  echo "==> Bundle NDI: $NDI_SRC"
  cp -f "$NDI_SRC" "$FRAMEWORKS/libndi.dylib"
  chmod +w "$FRAMEWORKS/libndi.dylib"
  # Also next to the binary for dlopen(NDILIB_LIBRARY_NAME)
  cp -f "$FRAMEWORKS/libndi.dylib" "$MACOS_DIR/libndi.dylib"
else
  echo "WARNING: libndi.dylib not found — NDI will need a system install" >&2
fi

mkdir -p "$OUT_DIR"
rm -f "$ZIP"
echo "==> Zip $ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"

echo "PACK_OK $ZIP"
ls -lh "$ZIP"
