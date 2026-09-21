$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $projectDirectory
try {
    $documents = @(
        'AGENTS.md', 'CONTRIBUTING.md', 'README.md',
        'docs/ORIENTATION_POLICY.md', 'docs/PHONE_UX_CONTRACT.md',
        'docs/CODE_BOUNDARIES.md', 'docs/VALIDATION.md',
        'docs/RELEASE_PROCESS.md', 'docs/MOBILE_MIGRATION.md',
        'docs/PROJECT_STATUS.md', '.github/pull_request_template.md'
    )
    $linkCount = 0
    foreach ($document in $documents) {
        $content = Get-Content -LiteralPath $document -Raw
        $parent = Split-Path -Parent (Join-Path $projectDirectory $document)
        foreach ($match in [regex]::Matches($content, '\]\(([^)]+)\)')) {
            $target = $match.Groups[1].Value
            if ($target -match '^(https?://|mailto:|#)') { continue }
            $path = ($target -split '#')[0]
            if (-not (Test-Path -LiteralPath (Join-Path $parent $path))) {
                throw "Missing local link in ${document}: $target"
            }
            $linkCount++
        }
    }
    if ((Get-Content docs/PROJECT_STATUS.md -Raw) -notmatch '(?m)^## Device and orientation validation\r?$') {
        throw 'Missing device-validation heading used by policy links.'
    }
    git diff --check HEAD^ HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Commit whitespace check failed.' }
    Write-Host "Policy documents present; $linkCount local links checked."
} finally {
    Pop-Location
}
