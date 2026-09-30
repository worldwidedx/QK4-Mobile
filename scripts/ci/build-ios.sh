#!/usr/bin/env bash
set -euo pipefail

# Compile only: no signing credentials, installation, or distribution.
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
: "${QK4_QT_IOS:?Set QK4_QT_IOS to the matching Qt iOS kit}"
: "${QK4_QT_HOST:?Set QK4_QT_HOST to the matching Qt macOS host kit}"
test "$(uname -s)" = Darwin
test -f "$project_dir/src/ios/iosaudiosession.mm"
test -x "$QK4_QT_IOS/bin/qt-cmake"

"$QK4_QT_IOS/bin/qt-cmake" -S "$project_dir" -B "$project_dir/build-ios-ci" \
  -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DQT_HOST_PATH="$QK4_QT_HOST" -DBUILD_TESTING=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$project_dir/build-ios-ci" --config Release --parallel 3 -- \
  -sdk iphoneos CODE_SIGNING_ALLOWED=NO
