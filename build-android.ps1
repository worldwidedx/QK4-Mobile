param(
    [ValidateSet("Doctor", "Configure", "Build", "Apk", "Install")]
    [string] $Action = "Build",
    [ValidateSet("Debug", "Release")]
    [string] $DeploymentType = "Debug",
    [string] $DeviceSerial = "",
    [string] $BuildDirectory = ""
)

$ErrorActionPreference = "Stop"

function Find-ExistingPath {
    param(
        [string] $Description,
        [string[]] $Candidates
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Unable to locate $Description. Checked: $($Candidates -join ', ')"
}

function Find-CommandPath {
    param(
        [string] $Description,
        [string] $Override,
        [string] $CommandName,
        [string[]] $Candidates
    )

    if ($Override) {
        return Find-ExistingPath $Description @($Override)
    }

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    return Find-ExistingPath $Description $Candidates
}

function Find-LatestQtAndroid {
    if ($env:QK4_QT_ANDROID) {
        return Find-ExistingPath "Qt Android ARM64 kit" @($env:QK4_QT_ANDROID)
    }

    $kits = Get-ChildItem -LiteralPath "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            $candidate = Join-Path $_.FullName "android_arm64_v8a"
            if (Test-Path -LiteralPath $candidate) {
                Get-Item -LiteralPath $candidate
            }
        } |
        Sort-Object { [version]$_.Parent.Name } -Descending

    if (-not $kits) {
        throw "Unable to locate a Qt Android ARM64 kit. Set QK4_QT_ANDROID."
    }

    return $kits[0].FullName
}

function Find-LatestNdk {
    param([string] $AndroidSdk)

    if ($env:ANDROID_NDK_ROOT) {
        return Find-ExistingPath "Android NDK" @($env:ANDROID_NDK_ROOT)
    }

    $ndkRoot = Join-Path $AndroidSdk "ndk"
    $ndks = Get-ChildItem -LiteralPath $ndkRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]$_.Name } -Descending

    if (-not $ndks) {
        throw "Unable to locate an Android NDK below $ndkRoot. Set ANDROID_NDK_ROOT."
    }

    return $ndks[0].FullName
}

$projectDir = $PSScriptRoot
if (-not $BuildDirectory) {
    $defaultBuildDirectory = if ($DeploymentType -eq "Release") {
        "build-android-arm64-release"
    } else {
        "build-android-arm64"
    }
    $BuildDirectory = Join-Path $projectDir $defaultBuildDirectory
}
$buildDir = [System.IO.Path]::GetFullPath($BuildDirectory)

$androidSdk = Find-ExistingPath "Android SDK" @(
    $env:ANDROID_SDK_ROOT,
    $env:ANDROID_HOME,
    (Join-Path $env:LOCALAPPDATA "Android\Sdk")
)
$androidNdk = Find-LatestNdk $androidSdk
$qtAndroid = Find-LatestQtAndroid

$qtVersionRoot = Split-Path -Parent $qtAndroid
$qtHost = Find-ExistingPath "matching Qt Windows host kit" @(
    $env:QK4_QT_HOST,
    (Join-Path $qtVersionRoot "mingw_64")
)

$cmake = Find-CommandPath "CMake" $env:QK4_CMAKE "cmake.exe" @(
    "C:\Qt\Tools\CMake_64\bin\cmake.exe"
)
$ninja = Find-CommandPath "Ninja" $env:QK4_NINJA "ninja.exe" @(
    "C:\Qt\Tools\Ninja\ninja.exe"
)
$javaHome = Find-ExistingPath "Java/JDK" @(
    $env:QK4_JAVA_HOME,
    "C:\Program Files\Android\Android Studio\jbr",
    $env:JAVA_HOME
)
$adb = Find-ExistingPath "Android Debug Bridge" @(
    (Join-Path $androidSdk "platform-tools\adb.exe")
)
$androidBuildTools = Find-ExistingPath "Android build tools" @(
    (Join-Path $androidSdk "build-tools\36.0.0")
)
$zipalign = Find-ExistingPath "Android zipalign" @(
    (Join-Path $androidBuildTools "zipalign.exe")
)
$apksigner = Find-ExistingPath "Android APK signer" @(
    (Join-Path $androidBuildTools "apksigner.bat")
)
$androidDeployQt = Find-ExistingPath "androiddeployqt" @(
    (Join-Path $qtHost "bin\androiddeployqt.exe")
)
$opusRoot = Find-ExistingPath "Android ARM64 Opus development files" @(
    $env:QK4_OPUS_ROOT,
    (Join-Path $projectDir "third_party\android\opus"),
    (Join-Path (Split-Path -Parent $projectDir) "qk4-android-deps\opus")
)
$opusHeader = Find-ExistingPath "Opus header" @(
    (Join-Path $opusRoot "include\opus\opus.h")
)
$opusLibrary = Find-ExistingPath "Android ARM64 Opus library" @(
    (Join-Path $opusRoot "lib\libopus.a")
)

function Show-AndroidEnvironment {
    Write-Host "QK4 Android build environment is ready:"
    Write-Host "  Project: $projectDir"
    Write-Host "  Qt Android: $qtAndroid"
    Write-Host "  Qt host: $qtHost"
    Write-Host "  Android SDK: $androidSdk"
    Write-Host "  Android NDK: $androidNdk"
    Write-Host "  Java: $javaHome"
    Write-Host "  CMake: $cmake"
    Write-Host "  Ninja: $ninja"
    Write-Host "  ADB: $adb"
    Write-Host "  Android build tools: $androidBuildTools"
    Write-Host "  androiddeployqt: $androidDeployQt"
    Write-Host "  Opus: $opusRoot"
}

function Configure-AndroidBuild {
    $opusInclude = Split-Path -Parent (Split-Path -Parent $opusHeader)
    if ($DeploymentType -eq "Release") {
        $requiredSigningVariables = @(
            "QT_ANDROID_KEYSTORE_PATH",
            "QT_ANDROID_KEYSTORE_ALIAS",
            "QT_ANDROID_KEYSTORE_STORE_PASS",
            "QT_ANDROID_KEYSTORE_KEY_PASS"
        )
        $missingSigningVariables = $requiredSigningVariables | Where-Object {
            [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($_))
        }
        if ($missingSigningVariables) {
            throw "Release signing requires these environment variables: $($missingSigningVariables -join ', ')"
        }
    }

    & $cmake `
        -S $projectDir `
        -B $buildDir `
        -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_TOOLCHAIN_FILE=$qtAndroid\lib\cmake\Qt6\qt.toolchain.cmake" `
        "-DQT_HOST_PATH=$qtHost" `
        "-DANDROID_SDK_ROOT=$androidSdk" `
        "-DANDROID_NDK_ROOT=$androidNdk" `
        "-DQT_CHAINLOAD_TOOLCHAIN_FILE=$androidNdk\build\cmake\android.toolchain.cmake" `
        -DANDROID_ABI=arm64-v8a `
        -DANDROID_PLATFORM=android-26 `
        "-DQK4_ANDROID_DEPLOYMENT_TYPE=$($DeploymentType.ToUpperInvariant())" `
        "-DQK4_OPUS_INCLUDE_DIR=$opusInclude" `
        "-DQK4_OPUS_LIBRARY=$opusLibrary"

    if ($LASTEXITCODE -ne 0) {
        throw "Android configuration failed with exit code $LASTEXITCODE."
    }
}

if ($Action -eq "Doctor") {
    Show-AndroidEnvironment
    exit 0
}

if ($Action -eq "Configure") {
    Configure-AndroidBuild
    Write-Host "Configured Android build in $buildDir"
    exit 0
}

if (-not (Test-Path -LiteralPath (Join-Path $buildDir "CMakeCache.txt"))) {
    Configure-AndroidBuild
}

if ($Action -eq "Build") {
    & $cmake --build $buildDir --target QK4 --parallel 4
    if ($LASTEXITCODE -ne 0) {
        throw "Android build failed with exit code $LASTEXITCODE."
    }
    exit 0
}

$packageDir = Join-Path $buildDir "android-build"
$deploymentSettings = Join-Path $buildDir "android-QK4-deployment-settings.json"
$applicationLibrary = Join-Path $buildDir "libQK4_arm64-v8a.so"
$packageLibraryDir = Join-Path $packageDir "libs\arm64-v8a"
$packageLibrary = Join-Path $packageLibraryDir "libQK4_arm64-v8a.so"

$env:JAVA_HOME = $javaHome
$env:Path = "$javaHome\bin;$env:Path"

& $cmake --build $buildDir --target QK4 --parallel 4
if ($LASTEXITCODE -ne 0) {
    throw "Android build failed with exit code $LASTEXITCODE."
}

New-Item -ItemType Directory -Force -Path $packageLibraryDir | Out-Null
Copy-Item -LiteralPath $applicationLibrary -Destination $packageLibrary -Force

$deployArguments = @(
    "--input", $deploymentSettings,
    "--output", $packageDir
)
if ($DeploymentType -eq "Release") {
    # Qt 6.11 occasionally calculates an incorrect filename after Gradle has
    # built the release APK. Ask it only for the release variant, then align
    # and sign the generated APK below using the Android SDK tools.
    $deployArguments += "--release"
}

& $androidDeployQt @deployArguments
if ($LASTEXITCODE -ne 0 -and $DeploymentType -ne "Release") {
    throw "Android packaging failed with exit code $LASTEXITCODE."
}

if ($DeploymentType -eq "Release") {
    # Limit discovery to Gradle's stable APK output tree. Recursing through the
    # entire package directory can race transient desugar/intermediate folders
    # that Gradle removes immediately after assembleRelease completes.
    $releaseApkOutput = Join-Path $packageDir "build\outputs\apk"
    $unsignedApk = Get-ChildItem -LiteralPath $releaseApkOutput `
        -Filter "*-release-unsigned.apk" -File -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $unsignedApk) {
        throw "Android release packaging failed and did not produce an unsigned release APK."
    }

    $alignedApk = Join-Path $packageDir "QK4-release-aligned.apk"
    $signedApk = Join-Path $packageDir "QK4-release.apk"
    Remove-Item -LiteralPath $alignedApk,$signedApk -Force -ErrorAction SilentlyContinue

    & $zipalign -f -p 4 $unsignedApk.FullName $alignedApk
    if ($LASTEXITCODE -ne 0) {
        throw "zipalign failed with exit code $LASTEXITCODE."
    }

    & $apksigner sign --ks $env:QT_ANDROID_KEYSTORE_PATH `
        --ks-key-alias $env:QT_ANDROID_KEYSTORE_ALIAS `
        --ks-pass "env:QT_ANDROID_KEYSTORE_STORE_PASS" `
        --key-pass "env:QT_ANDROID_KEYSTORE_KEY_PASS" `
        --out $signedApk $alignedApk
    if ($LASTEXITCODE -ne 0) {
        throw "apksigner failed with exit code $LASTEXITCODE."
    }

    & $apksigner verify --verbose --print-certs $signedApk
    if ($LASTEXITCODE -ne 0) {
        throw "Signed APK verification failed with exit code $LASTEXITCODE."
    }
}

$apk = if ($DeploymentType -eq "Release") {
    Get-Item -LiteralPath $signedApk -ErrorAction SilentlyContinue
} else {
    # Gradle removes transient desugar directories while packaging. Search its
    # stable APK output tree instead of recursing through every intermediate.
    $debugApkOutput = Join-Path $packageDir "build\outputs\apk"
    Get-ChildItem -LiteralPath $debugApkOutput -Filter "*.apk" -File -Recurse `
        -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notlike "*-unsigned.apk" -and $_.Name -notlike "*-aligned.apk" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

if (-not $apk) {
    throw "The APK target completed but no APK was produced under $packageDir."
}

Write-Host "APK: $($apk.FullName)"

if ($Action -eq "Install") {
    $adbArgs = @()
    if ($DeviceSerial) {
        $adbArgs += @("-s", $DeviceSerial)
    }
    $adbArgs += @("install", "-r", $apk.FullName)

    & $adb @adbArgs
    if ($LASTEXITCODE -ne 0) {
        throw "APK installation failed with exit code $LASTEXITCODE."
    }
}
