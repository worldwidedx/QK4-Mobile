param(
    [ValidateSet("Doctor", "Configure", "Build", "Test")]
    [string] $Action = "Test",
    [string] $BuildDirectory = "",
    [string] $TestRegex = "",
    [ValidateRange(1, 32)]
    [int] $Parallel = 4
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

function Find-LatestQtHost {
    if ($env:QK4_QT_HOST) {
        return Find-ExistingPath "Qt Windows host kit" @($env:QK4_QT_HOST)
    }

    $hostKits = Get-ChildItem -LiteralPath "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            $candidate = Join-Path $_.FullName "mingw_64"
            if (Test-Path -LiteralPath (Join-Path $candidate "lib\cmake\Qt6")) {
                Get-Item -LiteralPath $candidate
            }
        } |
        Sort-Object {
            $versionText = $_.Parent.Name
            try { [version]$versionText } catch { [version]"0.0" }
        } -Descending

    if (-not $hostKits) {
        throw "Unable to locate a Qt Windows MinGW host kit. Set QK4_QT_HOST."
    }
    return $hostKits[0].FullName
}

function Resolve-MingwBin {
    param([string] $Candidate)

    if (-not $Candidate) {
        return ""
    }
    if (Test-Path -LiteralPath (Join-Path $Candidate "g++.exe")) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }
    $binCandidate = Join-Path $Candidate "bin"
    if (Test-Path -LiteralPath (Join-Path $binCandidate "g++.exe")) {
        return (Resolve-Path -LiteralPath $binCandidate).Path
    }
    return ""
}

function Find-MatchingMingwBin {
    param([string] $QtHost)

    if ($env:QK4_MINGW_BIN) {
        $overrideBin = Resolve-MingwBin $env:QK4_MINGW_BIN
        if ($overrideBin) {
            return $overrideBin
        }
        throw "QK4_MINGW_BIN does not contain bin\g++.exe: $env:QK4_MINGW_BIN"
    }

    # Qt's generated environment file records the exact compiler kit selected
    # by the Maintenance Tool. Prefer that mapping over guessing by version.
    $qtEnvironment = Join-Path $QtHost "bin\qtenv2.bat"
    if (Test-Path -LiteralPath $qtEnvironment) {
        foreach ($line in Get-Content -LiteralPath $qtEnvironment) {
            foreach ($segment in ($line -split ';')) {
                if ($segment -match '(?i)([A-Z]:\\[^=]*\\mingw[^=]*\\bin)') {
                    $mappedBin = Resolve-MingwBin $Matches[1]
                    if ($mappedBin) {
                        return $mappedBin
                    }
                }
            }
        }
    }

    $toolchains = Get-ChildItem -LiteralPath "C:\Qt\Tools" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^mingw(\d+)_64$' } |
        Sort-Object { [int]([regex]::Match($_.Name, '\d+').Value) } -Descending
    foreach ($toolchain in $toolchains) {
        $toolchainBin = Resolve-MingwBin $toolchain.FullName
        if ($toolchainBin) {
            return $toolchainBin
        }
    }
    throw "Unable to locate the Qt MinGW compiler runtime. Set QK4_MINGW_BIN."
}

function Invoke-Checked {
    param(
        [string] $Description,
        [string] $Executable,
        [string[]] $Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

$projectDirectory = $PSScriptRoot
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectDirectory "build-tests"
}
$testBuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$testSourceDirectory = Join-Path $projectDirectory "tests"
$qtHost = Find-LatestQtHost
$qtBin = Join-Path $qtHost "bin"
$mingwBin = Find-MatchingMingwBin $qtHost
$compiler = Find-ExistingPath "MinGW C++ compiler" @((Join-Path $mingwBin "g++.exe"))
$cmake = Find-CommandPath "CMake" $env:QK4_CMAKE "cmake.exe" @(
    "C:\Qt\Tools\CMake_64\bin\cmake.exe"
)
$ninja = Find-CommandPath "Ninja" $env:QK4_NINJA "ninja.exe" @(
    "C:\Qt\Tools\Ninja\ninja.exe"
)
$ctest = Find-ExistingPath "CTest" @((Join-Path (Split-Path -Parent $cmake) "ctest.exe"))

# Limit the environment correction to this runner. This is the critical piece:
# g++.exe launches cc1plus.exe from libexec, and that child process needs the
# MinGW runtime DLLs in mingwBin. A full compiler path in CMakeCache.txt alone
# does not make those DLLs discoverable.
$previousProcessPath = $env:Path
try {
    $env:Path = "$qtBin;$mingwBin;$previousProcessPath"

    $compilerBackend = (& $compiler -print-prog-name=cc1plus).Trim()
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $compilerBackend)) {
        throw "MinGW could not locate cc1plus.exe. Reinstall the matching Qt MinGW toolchain."
    }
    & $compilerBackend --version | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cc1plus.exe could not start. Verify the MinGW runtime DLLs in $mingwBin."
    }

    Write-Host "QK4 Windows test environment:"
    Write-Host "  Qt host: $qtHost"
    Write-Host "  MinGW runtime: $mingwBin"
    Write-Host "  Compiler: $compiler"
    Write-Host "  CMake: $cmake"
    Write-Host "  Ninja: $ninja"
    Write-Host "  Test build: $testBuildDirectory"

    if ($Action -eq "Doctor") {
        Write-Host "Desktop compiler backend and runtime DLL search path are ready."
        return
    }

    $configureArguments = @(
        "-S", $testSourceDirectory,
        "-B", $testBuildDirectory,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_PREFIX_PATH=$qtHost",
        "-DCMAKE_CXX_COMPILER=$compiler",
        "-DCMAKE_MAKE_PROGRAM=$ninja"
    )
    Invoke-Checked "Windows test configuration" $cmake $configureArguments
    if ($Action -eq "Configure") {
        return
    }

    Invoke-Checked "Windows test build" $cmake @(
        "--build", $testBuildDirectory,
        "--parallel", $Parallel.ToString()
    )
    if ($Action -eq "Build") {
        return
    }

    $testArguments = @(
        "--test-dir", $testBuildDirectory,
        "--output-on-failure",
        "--parallel", $Parallel.ToString()
    )
    if ($TestRegex) {
        $testArguments += @("-R", $TestRegex)
    }
    Invoke-Checked "Windows tests" $ctest $testArguments
} finally {
    $env:Path = $previousProcessPath
}
