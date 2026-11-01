#!/usr/bin/env bash
set -euo pipefail

# Generate a signing-enabled Xcode project for local physical-device testing.
# Xcode manages the developer certificate and provisioning profile; none are
# read from the shell or stored in the repository.
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${QK4_QT_IOS:?Set QK4_QT_IOS to the matching Qt iOS kit}"
: "${QK4_QT_HOST:?Set QK4_QT_HOST to the matching Qt macOS host kit}"

test "$(uname -s)" = Darwin
test -x "$QK4_QT_IOS/bin/qt-cmake"
test -f "$project_dir/src/ios/iosaudiosession.mm"

build_dir="${QK4_IOS_BUILD_DIR:-$project_dir/build-ios-device}"
cmake_args=(
  -U CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED
  -U CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE
  -S "$project_dir"
  -B "$build_dir"
  -G Xcode
  -DCMAKE_SYSTEM_NAME=iOS
  -DCMAKE_OSX_SYSROOT=iphoneos
  -DCMAKE_OSX_ARCHITECTURES=arm64
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
  "-DQT_HOST_PATH=$QK4_QT_HOST"
  -DBUILD_TESTING=OFF
  # Release needs -g or Xcode produces no dSYM; without one the archive upload
  # warns "Upload Symbols Failed" and TestFlight crashes cannot be symbolicated.
  "-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -g"
  "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -g"
  # CMake redirects each target's output to <build>/Release-iphoneos, so the
  # dSYM would land there instead of in the archive. Use Xcode's own location,
  # which the archive step collects.
  '-DCMAKE_XCODE_ATTRIBUTE_DWARF_DSYM_FOLDER_PATH=$(BUILD_DIR)/$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)'
)

if [[ -n "${QK4_IOS_BUNDLE_ID:-}" ]]; then
  cmake_args+=("-DQK4_IOS_BUNDLE_ID=$QK4_IOS_BUNDLE_ID")
fi

"$QK4_QT_IOS/bin/qt-cmake" "${cmake_args[@]}"
printf '\nGenerated signing-enabled Xcode project: %s/QK4.xcodeproj\n' "$build_dir"
