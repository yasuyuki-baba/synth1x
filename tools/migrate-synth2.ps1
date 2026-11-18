# Copy Synth2 user data on Windows. Preview is the default.
param([switch]$Apply)

$ErrorActionPreference = 'Stop'
if (-not $env:APPDATA -or -not $env:LOCALAPPDATA) {
    throw 'APPDATA and LOCALAPPDATA are required.'
}

$dataFrom = Join-Path $env:APPDATA 'Synth2'
$dataTo = Join-Path $env:APPDATA 'Synth1X'

function Get-MigrationItem([string]$Path) {
    try {
        Get-Item -LiteralPath $Path -Force
    } catch [System.Management.Automation.ItemNotFoundException] {
        return
    }
}

function Test-MigrationLink($Item) {
    $Item -and ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint)
}

function Copy-MigrationItem([string]$Source, [string]$Destination) {
    $sourceItem = Get-MigrationItem $Source
    if (-not $sourceItem) { return }
    $targetItem = Get-MigrationItem $Destination
    if ((Test-MigrationLink $sourceItem) -or (Test-MigrationLink $targetItem)) {
        Write-Host "Skip symbolic link or junction: $Source -> $Destination"
        return
    }

    if ($sourceItem.PSIsContainer) {
        if ($targetItem -and -not $targetItem.PSIsContainer) {
            Write-Host "Keep existing: $Destination"
            return
        }
        foreach ($child in Get-ChildItem -LiteralPath $Source -Force) {
            if ($child.Name -like '.synth2-write-*' -or $child.Name -like '.patch-write-*' -or
                $child.Name -in @('.synth2-tags.cache', '.tags.cache')) { continue }
            if ($Source -eq $dataFrom -and $child.Name -eq 'preset-directory') {
                Write-Host 'Custom library: copy the folder and select its copy in Synth1X (see MIGRATION.md).'
                continue
            }
            Copy-MigrationItem $child.FullName (Join-Path $Destination $child.Name)
        }
        return
    }

    if ($targetItem) {
        Write-Host "Keep existing: $Destination"
        return
    }
    Write-Host "Copy: $Source -> $Destination"
    if ($Apply) {
        $parent = Split-Path -LiteralPath $Destination
        [IO.Directory]::CreateDirectory($parent) | Out-Null
        $stage = Join-Path $parent ('.migration.' + [Guid]::NewGuid().ToString('N'))
        try {
            [IO.File]::Copy($Source, $stage)
            # This overload fails if the destination already exists.
            [IO.File]::Move($stage, $Destination)
        } catch [System.IO.IOException] {
            if (-not (Get-MigrationItem $Destination)) { throw }
            Write-Host "Keep existing: $Destination"
        } finally {
            if ([IO.File]::Exists($stage)) {
                [IO.File]::SetAttributes($stage, [IO.FileAttributes]::Normal)
                [IO.File]::Delete($stage)
            }
        }
    }
}

Write-Host "User data: $dataFrom -> $dataTo"
$sourceFolder = Get-MigrationItem $dataFrom
if ($sourceFolder.PSIsContainer -or (Test-MigrationLink $sourceFolder)) {
    Copy-MigrationItem $dataFrom $dataTo
} else {
    Write-Host 'No Synth2 user data folder found.'
}
$configFrom = Join-Path $env:LOCALAPPDATA 'Synth2'
$configTo = Join-Path $env:LOCALAPPDATA 'Synth1X'
if ($configFrom -ne $dataFrom) {
    $settingsFrom = Join-Path $configFrom 'settings.ini'
    $settingsTo = Join-Path $configTo 'settings.ini'
    Write-Host "Standalone settings: $settingsFrom -> $settingsTo"
    if ((Test-MigrationLink (Get-MigrationItem $configFrom)) -or
        (Test-MigrationLink (Get-MigrationItem $configTo))) {
        Write-Host 'Skip standalone settings in symbolic-link or junction folder.'
    } elseif ((Get-MigrationItem $settingsFrom) -is [IO.FileInfo]) {
        Copy-MigrationItem $settingsFrom $settingsTo
    }
}
if (-not $Apply) {
    Write-Host 'Preview only. Close your DAW and both apps, then rerun with -Apply to copy.'
}
