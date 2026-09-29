#!/usr/bin/env bash
# QK4 Android build helper for macOS/Linux (mirrors build-android.ps1).
#
# Usage:
#   ./build-android.sh doctor                 # print detected toolchain, verify it
#   ./build-android.sh configure [Debug|Release]
#   ./build-android.sh build     [Debug|Release]   # compile the app .so
#   ./build-android.sh apk       [Debug|Release]   # build + package an APK
#   ./build-android.sh install   [Debug|Release] [adb-serial]   # apk + adb install
#
# Debug is the default and needs no keystore (uses the Android debug key).
# Release needs QT_ANDROID_KEYSTORE_{PATH,ALIAS,STORE_PASS,KEY_PASS}.
#
# Override any autodetected path with an env var:
#   QK4_QT_ANDROID, QK4_QT_HOST, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT,
#   QK4_OPUS_ROOT, QK4_CMAKE, QK4_NINJA, QK4_JAVA_HOME
set -euo pipefail

ACTION="${1:-build}"
DEPLOY="${2:-Debug}"
DEVICE_SERIAL="${3:-}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

die() { echo "error: $*" >&2; exit 1; }
first_dir() { for d in "$@"; do [ -n "${d:-}" ] && [ -d "$d" ] && { echo "$d"; return; }; done; return 1; }
latest_subdir() { ls -1 "$1" 2>/dev/null | sort -V | tail -1; }

# --- Qt Android arm64 kit + matching macOS host kit ---
QT_ANDROID="${QK4_QT_ANDROID:-}"
if [ -z "$QT_ANDROID" ]; then
    for base in "$HOME/Qt" /opt/Qt; do
        [ -d "$base" ] || continue
        ver="$(ls -1 "$base" 2>/dev/null | grep -E '^[0-9]' | sort -V | tail -1)"
        [ -n "$ver" ] && [ -d "$base/$ver/android_arm64_v8a" ] && { QT_ANDROID="$base/$ver/android_arm64_v8a"; break; }
    done
fi
[ -n "$QT_ANDROID" ] && [ -d "$QT_ANDROID" ] || die "Qt Android arm64 kit not found; set QK4_QT_ANDROID"
QT_VER_ROOT="$(dirname "$QT_ANDROID")"
QT_HOST="$(first_dir "${QK4_QT_HOST:-}" "$QT_VER_ROOT/macos" "$QT_VER_ROOT/gcc_64" "$QT_VER_ROOT/clang_64")" \
    || die "Qt host kit (macos/gcc_64) not found next to $QT_ANDROID; set QK4_QT_HOST"

# --- Android SDK + NDK ---
SDK="$(first_dir "${ANDROID_SDK_ROOT:-}" "${ANDROID_HOME:-}" /usr/local/share/android-commandlinetools "$HOME/Library/Android/sdk")" \
    || die "Android SDK not found; set ANDROID_SDK_ROOT"
NDK="${ANDROID_NDK_ROOT:-$SDK/ndk/$(latest_subdir "$SDK/ndk")}"
[ -d "$NDK" ] || die "Android NDK not found under $SDK/ndk; set ANDROID_NDK_ROOT"
BUILD_TOOLS="$SDK/build-tools/$(latest_subdir "$SDK/build-tools")"

# --- host tools ---
CMAKE="${QK4_CMAKE:-$(command -v cmake || true)}"; [ -x "$CMAKE" ] || die "cmake not found"
NINJA="${QK4_NINJA:-$(command -v ninja || true)}"; [ -x "$NINJA" ] || die "ninja not found"
ADB="$(first_dir "$SDK/platform-tools" >/dev/null 2>&1 && echo "$SDK/platform-tools/adb" || command -v adb)"
ANDROIDDEPLOYQT="$QT_HOST/bin/androiddeployqt"
JAVA_HOME_DIR="${QK4_JAVA_HOME:-${JAVA_HOME:-$(/usr/libexec/java_home 2>/dev/null || true)}}"
[ -n "$JAVA_HOME_DIR" ] && [ -d "$JAVA_HOME_DIR" ] || die "JDK not found; set QK4_JAVA_HOME"

# --- Opus (Android arm64 prebuilt) ---
OPUS_ROOT="$(first_dir "${QK4_OPUS_ROOT:-}" "$PROJECT_DIR/third_party/android/opus" "$(dirname "$PROJECT_DIR")/qk4-android-deps/opus")" \
    || die "Android arm64 Opus not found; set QK4_OPUS_ROOT"
OPUS_INCLUDE="$OPUS_ROOT/include"
OPUS_LIBRARY="$OPUS_ROOT/lib/libopus.a"
[ -f "$OPUS_LIBRARY" ] || die "libopus.a not found at $OPUS_LIBRARY"

case "$DEPLOY" in Debug|Release) ;; *) die "deployment type must be Debug or Release" ;; esac
BUILD_DIR="$PROJECT_DIR/build-android-arm64"
if [ "$DEPLOY" = Release ]; then BUILD_DIR="$BUILD_DIR-release"; fi

# Qt Shader Tools (qsb): required by qt6_add_shaders. Some Qt kits ship without
# it (e.g. this Qt 6.11.1 Android/macOS kit). When the target kit lacks it,
# point find_package at a SAME-VERSION copy — a Homebrew qt or the iOS kit for
# the target module, and Homebrew qt (which bundles the qsb host binary) for
# the host tools.
SHADER_ARGS=()
SHADERTOOLS_NOTE=""
if [ ! -d "$QT_ANDROID/lib/cmake/Qt6ShaderTools" ]; then
    st_dir=""; stt_dir=""
    for c in /usr/local/opt/qt /opt/homebrew/opt/qt "$QT_VER_ROOT/ios"; do
        if [ -d "$c/lib/cmake/Qt6ShaderTools" ]; then st_dir="$c/lib/cmake/Qt6ShaderTools"; break; fi
    done
    for c in /usr/local/opt/qt /opt/homebrew/opt/qt; do
        if [ -d "$c/lib/cmake/Qt6ShaderToolsTools" ]; then stt_dir="$c/lib/cmake/Qt6ShaderToolsTools"; break; fi
    done
    [ -n "$st_dir" ] || die "Qt6ShaderTools not in the Android kit and no fallback found (install Qt Shader Tools, or brew install qt)"
    [ -n "$stt_dir" ] || die "Qt6ShaderToolsTools (qsb host tool) not found; install Qt Shader Tools or brew install qt"
    SHADER_ARGS+=("-DQt6ShaderTools_DIR=$st_dir" "-DQt6ShaderToolsTools_DIR=$stt_dir")
    SHADERTOOLS_NOTE="  ShaderTools:  $st_dir (fallback)
  qsb host:     $stt_dir (fallback)"
fi

show_env() {
    cat <<EOF
QK4 Android build environment:
  Project:      $PROJECT_DIR
  Qt Android:   $QT_ANDROID
  Qt host:      $QT_HOST
  Android SDK:  $SDK
  Android NDK:  $NDK
  Build tools:  $BUILD_TOOLS
  JDK:          $JAVA_HOME_DIR
  CMake:        $CMAKE
  Ninja:        $NINJA
  adb:          $ADB
  androiddeployqt: $ANDROIDDEPLOYQT
  Opus:         $OPUS_ROOT
${SHADERTOOLS_NOTE:+$SHADERTOOLS_NOTE
}  Build dir:    $BUILD_DIR  ($DEPLOY)
EOF
}

configure() {
    if [ "$DEPLOY" = Release ]; then
        for v in QT_ANDROID_KEYSTORE_PATH QT_ANDROID_KEYSTORE_ALIAS QT_ANDROID_KEYSTORE_STORE_PASS QT_ANDROID_KEYSTORE_KEY_PASS; do
            [ -n "${!v:-}" ] || die "Release signing needs env var $v"
        done
    fi
    "$CMAKE" -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja \
        "-DCMAKE_MAKE_PROGRAM=$NINJA" \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_TOOLCHAIN_FILE=$QT_ANDROID/lib/cmake/Qt6/qt.toolchain.cmake" \
        "-DQT_HOST_PATH=$QT_HOST" \
        "-DANDROID_SDK_ROOT=$SDK" \
        "-DANDROID_NDK_ROOT=$NDK" \
        "-DQT_CHAINLOAD_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-26 \
        "-DQK4_ANDROID_DEPLOYMENT_TYPE=$(echo "$DEPLOY" | tr '[:lower:]' '[:upper:]')" \
        "-DQK4_OPUS_INCLUDE_DIR=$OPUS_INCLUDE" \
        "-DQK4_OPUS_LIBRARY=$OPUS_LIBRARY" \
        ${SHADER_ARGS[@]+"${SHADER_ARGS[@]}"}
}

build() {
    [ -f "$BUILD_DIR/CMakeCache.txt" ] || configure
    "$CMAKE" --build "$BUILD_DIR" --target QK4 --parallel 4
}

package() {
    build
    export JAVA_HOME="$JAVA_HOME_DIR"; export PATH="$JAVA_HOME_DIR/bin:$PATH"
    local pkg="$BUILD_DIR/android-build"
    local settings="$BUILD_DIR/android-QK4-deployment-settings.json"
    mkdir -p "$pkg/libs/arm64-v8a"
    cp -f "$BUILD_DIR/libQK4_arm64-v8a.so" "$pkg/libs/arm64-v8a/"
    local args=(--input "$settings" --output "$pkg")
    if [ "$DEPLOY" = Release ]; then args+=(--release); fi
    "$ANDROIDDEPLOYQT" "${args[@]}"
    if [ "$DEPLOY" = Release ]; then
        local unsigned; unsigned="$(find "$pkg/build/outputs/apk" -name '*-release-unsigned.apk' 2>/dev/null | head -1)"
        [ -n "$unsigned" ] || die "no unsigned release APK produced"
        local aligned="$pkg/QK4-release-aligned.apk" signed="$pkg/QK4-release.apk"
        "$BUILD_TOOLS/zipalign" -f -p 4 "$unsigned" "$aligned"
        "$BUILD_TOOLS/apksigner" sign --ks "$QT_ANDROID_KEYSTORE_PATH" \
            --ks-key-alias "$QT_ANDROID_KEYSTORE_ALIAS" \
            --ks-pass "env:QT_ANDROID_KEYSTORE_STORE_PASS" \
            --key-pass "env:QT_ANDROID_KEYSTORE_KEY_PASS" \
            --out "$signed" "$aligned"
        APK="$signed"
    else
        APK="$(find "$pkg/build/outputs/apk" -name '*.apk' ! -name '*-unsigned.apk' ! -name '*-aligned.apk' 2>/dev/null | sort | tail -1)"
    fi
    [ -n "${APK:-}" ] || die "no APK produced under $pkg"
    echo "APK: $APK"
}

case "$(echo "$ACTION" | tr '[:upper:]' '[:lower:]')" in
    doctor)    show_env ;;
    configure) show_env; configure ;;
    build)     show_env; build ;;
    apk)       show_env; package ;;
    install)
        package
        local_args=()
        if [ -n "$DEVICE_SERIAL" ]; then local_args+=(-s "$DEVICE_SERIAL"); fi
        "$ADB" ${local_args[@]+"${local_args[@]}"} install -r "$APK"
        echo "Installed $APK"
        ;;
    *) die "unknown action '$ACTION' (doctor|configure|build|apk|install)" ;;
esac
