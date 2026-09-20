[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $DeviceSerial,

    [string] $AdbPath = "",

    [switch] $RemoveOldPackage
)

$ErrorActionPreference = "Stop"

$oldPackage = "com.ai5qk.qk4phone"
$newPackage = "com.w9wdx.qk4phone"

if (-not $AdbPath) {
    $AdbPath = Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe"
}
if (-not (Test-Path -LiteralPath $AdbPath)) {
    throw "ADB was not found at '$AdbPath'. Supply -AdbPath explicitly."
}
$AdbPath = (Resolve-Path -LiteralPath $AdbPath).Path

function Invoke-AdbText {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments,

        [switch] $AllowFailure
    )

    $output = & $AdbPath -s $DeviceSerial @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        $detail = ($output | Out-String).Trim()
        throw "ADB failed with exit code $exitCode while running '$($Arguments -join ' ')'. $detail"
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = @($output | ForEach-Object { $_.ToString() })
    }
}

function Test-PackageInstalled {
    param([string] $PackageName)

    $result = Invoke-AdbText -Arguments @("shell", "pm", "path", $PackageName) -AllowFailure
    return $result.ExitCode -eq 0 -and ($result.Output -join "`n") -match "package:"
}

function Test-AppPath {
    param(
        [string] $PackageName,
        [string] $Path,
        [ValidateSet("e", "d", "f")]
        [string] $Kind = "e"
    )

    $result = Invoke-AdbText -Arguments @(
        "shell", "run-as", $PackageName, "test", "-$Kind", $Path
    ) -AllowFailure
    return $result.ExitCode -eq 0
}

function New-AdbProcessStartInfo {
    param(
        [string[]] $Arguments,
        [bool] $RedirectInput,
        [bool] $RedirectOutput
    )

    # Every argument used here is a package, serial, command, or private path
    # constrained below to characters that do not require shell quoting.
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $AdbPath
    $info.Arguments = (@("-s", $DeviceSerial) + $Arguments) -join " "
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $RedirectInput
    $info.RedirectStandardOutput = $RedirectOutput
    $info.RedirectStandardError = $true
    return $info
}

function Read-AppFileChunk {
    param(
        [string] $Path,
        [int] $BlockSize,
        [int] $BlockIndex
    )

    $arguments = @(
        "exec-out", "run-as", $oldPackage, "dd", "if=$Path",
        "bs=$BlockSize", "skip=$BlockIndex", "count=1", "status=none"
    )
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = New-AdbProcessStartInfo $arguments $false $true
    if (-not $process.Start()) {
        throw "Could not start ADB while reading '$Path'."
    }
    try {
        $memory = [System.IO.MemoryStream]::new()
        try {
            $process.StandardOutput.BaseStream.CopyTo($memory)
        } finally {
            $bytes = $memory.ToArray()
            $memory.Dispose()
        }
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Could not read '$Path' from the old package. $errorText"
        }
        return ,$bytes
    } finally {
        $process.Dispose()
    }
}

function Write-AppFileChunk {
    param(
        [string] $Path,
        [int] $BlockSize,
        [int] $BlockIndex,
        [byte[]] $Bytes
    )

    $arguments = @(
        "exec-in", "run-as", $newPackage, "dd", "of=$Path",
        "bs=$BlockSize", "seek=$BlockIndex", "conv=notrunc", "status=none"
    )
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = New-AdbProcessStartInfo $arguments $true $true
    if (-not $process.Start()) {
        throw "Could not start ADB while writing '$Path'."
    }
    try {
        try {
            $process.StandardInput.BaseStream.Write($Bytes, 0, $Bytes.Length)
            $process.StandardInput.BaseStream.Flush()
            $process.StandardInput.Close()
        } catch {
            $process.StandardInput.Close()
            throw
        }
        $outputText = $process.StandardOutput.ReadToEnd()
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Could not write '$Path' into the new package. $outputText $errorText"
        }
    } finally {
        $process.Dispose()
    }
}

function Copy-AppFile {
    param([string] $Path)

    $sizeResult = Invoke-AdbText -Arguments @(
        "shell", "run-as", $oldPackage, "stat", "-c", "%s", $Path
    )
    $sizeText = ($sizeResult.Output -join "").Trim()
    [long] $fileSize = 0
    if (-not [long]::TryParse($sizeText, [ref] $fileSize)) {
        throw "Could not determine the size of '$Path'."
    }

    $lastSlash = $Path.LastIndexOf('/')
    if ($lastSlash -le 0) {
        throw "Refusing a path without an app-private parent: '$Path'."
    }
    $parent = $Path.Substring(0, $lastSlash)
    Invoke-AdbText -Arguments @(
        "shell", "run-as", $newPackage, "mkdir", "-p", $parent
    ) | Out-Null
    Invoke-AdbText -Arguments @(
        "shell", "run-as", $newPackage, "rm", "-f", $Path
    ) | Out-Null
    Invoke-AdbText -Arguments @(
        "shell", "run-as", $newPackage, "touch", $Path
    ) | Out-Null

    $blockSize = 262144
    $blockCount = [int] [Math]::Ceiling($fileSize / [double] $blockSize)
    for ($blockIndex = 0; $blockIndex -lt $blockCount; ++$blockIndex) {
        [byte[]] $bytes = Read-AppFileChunk $Path $blockSize $blockIndex
        if ($bytes.Length -le 0) {
            throw "The old package returned an empty block for '$Path'."
        }
        Write-AppFileChunk $Path $blockSize $blockIndex $bytes
    }

    $newSizeResult = Invoke-AdbText -Arguments @(
        "shell", "run-as", $newPackage, "stat", "-c", "%s", $Path
    )
    [long] $newSize = 0
    if (-not [long]::TryParse(($newSizeResult.Output -join "").Trim(), [ref] $newSize) -or
        $newSize -ne $fileSize) {
        throw "Size verification failed for '$Path'."
    }
}

function Get-AppFileHash {
    param(
        [string] $PackageName,
        [string] $Path
    )

    $result = Invoke-AdbText -Arguments @(
        "shell", "run-as", $PackageName, "sha256sum", $Path
    )
    $text = ($result.Output -join "`n").Trim()
    if ($text -notmatch "^([0-9a-fA-F]{64})\s") {
        throw "Could not verify '$Path' in $PackageName."
    }
    return $Matches[1].ToLowerInvariant()
}

if (-not (Test-PackageInstalled $oldPackage)) {
    throw "The old package '$oldPackage' is not installed on $DeviceSerial."
}
if (-not (Test-PackageInstalled $newPackage)) {
    throw "Install the new package '$newPackage' before running this migration."
}

# run-as is intentionally required: it keeps radio profiles and their
# obfuscated passwords inside app-private storage and off shared storage.
Invoke-AdbText -Arguments @("shell", "run-as", $oldPackage, "id") | Out-Null
Invoke-AdbText -Arguments @("shell", "run-as", $newPackage, "id") | Out-Null

$marker = "files/.qk4-package-migration-complete"
if (Test-AppPath $newPackage $marker "f") {
    throw "The new package already contains the completed-migration marker. Refusing to run twice."
}
if ((Test-AppPath $newPackage "files/settings" "e") -or
    (Test-AppPath $newPackage "files/sstv" "e")) {
    throw "The new package already contains QK4 settings or SSTV data. Clear/reinstall it before migration to avoid overwriting data."
}
if (-not (Test-AppPath $oldPackage "files/settings/QK4/QK4.conf" "f")) {
    throw "The old package does not contain the expected QK4 settings file."
}

$migrationPaths = @("files/settings")
if (Test-AppPath $oldPackage "files/sstv" "d") {
    $migrationPaths += "files/sstv"
}

# Query each selected tree separately so an optional SSTV directory never
# turns a settings migration into a failing find invocation.
$filesToVerify = @()
foreach ($path in $migrationPaths) {
    $result = Invoke-AdbText -Arguments @(
        "shell", "run-as", $oldPackage, "find", $path, "-type", "f"
    )
    $filesToVerify += $result.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
}
$filesToVerify = @($filesToVerify | Sort-Object -Unique)
if (-not $filesToVerify) {
    throw "No private settings files were found to migrate."
}
foreach ($path in $filesToVerify) {
    if ($path -notmatch "^files/(settings|sstv)/[A-Za-z0-9._/-]+$") {
        throw "Refusing an unexpected private-data path: '$path'."
    }
}

Invoke-AdbText -Arguments @("shell", "am", "force-stop", $oldPackage) | Out-Null
Invoke-AdbText -Arguments @("shell", "am", "force-stop", $newPackage) | Out-Null

Write-Host "Copying $($filesToVerify.Count) private QK4 data file(s)..."
foreach ($path in $filesToVerify) {
    Copy-AppFile $path
}

Write-Host "Verifying every migrated file..."
foreach ($path in $filesToVerify) {
    $oldHash = Get-AppFileHash $oldPackage $path
    $newHash = Get-AppFileHash $newPackage $path
    if ($oldHash -ne $newHash) {
        throw "Verification failed for '$path'. The old package has not been removed."
    }
}
Invoke-AdbText -Arguments @(
    "shell", "run-as", $newPackage, "touch", $marker
) | Out-Null

if ($RemoveOldPackage) {
    Write-Host "Verification passed. Removing $oldPackage..."
    Invoke-AdbText -Arguments @("uninstall", $oldPackage) | Out-Null
    if (Test-PackageInstalled $oldPackage) {
        throw "Android reported success, but the old package is still installed."
    }
}

if (-not (Test-PackageInstalled $newPackage)) {
    throw "The new package is unexpectedly missing after migration."
}

Write-Host "Migration complete: $($filesToVerify.Count) file(s) verified in $newPackage."
if (-not $RemoveOldPackage) {
    Write-Host "The old package was retained. Re-run is blocked by the migration marker; remove it manually after validation."
}
