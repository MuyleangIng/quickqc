param(
    [string]$Repo = "MuyleangIng/quickqc"
)

$ErrorActionPreference = "Stop"

$isArm = $env:PROCESSOR_ARCHITECTURE -eq "ARM64"
$assets = if ($isArm) { @("quickqc-windows-arm64.zip", "quickqc-windows-amd64.zip") } else { @("quickqc-windows-amd64.zip") }

$tmp = New-Item -ItemType Directory -Path (Join-Path $env:TEMP ("quickqc-install-" + [guid]::NewGuid().ToString())) -Force

$zipPath = $null
foreach ($asset in $assets) {
    $url = "https://github.com/$Repo/releases/latest/download/$asset"
    $candidateZip = Join-Path $tmp.FullName $asset
    try {
        Invoke-WebRequest -Uri $url -OutFile $candidateZip
        $zipPath = $candidateZip
        break
    } catch {
    }
}

if (-not $zipPath) {
    throw "Could not download a Windows release asset for this machine."
}

Expand-Archive -Path $zipPath -DestinationPath $tmp.FullName -Force

$candidate1 = Join-Path $tmp.FullName "quickqc.exe"
$candidate2 = Join-Path $tmp.FullName "Release\\quickqc.exe"

if (Test-Path $candidate1) {
    $exePath = $candidate1
} elseif (Test-Path $candidate2) {
    $exePath = $candidate2
} else {
    throw "quickqc.exe not found in downloaded archive."
}

$installDir = Join-Path $env:LOCALAPPDATA "QuickQC"
New-Item -Path $installDir -ItemType Directory -Force | Out-Null
$target = Join-Path $installDir "quickqc.exe"
Copy-Item -Path $exePath -Destination $target -Force

try {
    Unblock-File -Path $target
} catch {
}

Write-Host "QuickQC installed at $target"
Write-Host "Run with: $target"
