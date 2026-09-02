[CmdletBinding()]
param(
	[string] $ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$CatalogPath = Join-Path $ProjectRoot "Content\Localization\uegt.ui.json"
$ContentSourceRoot = Join-Path $ProjectRoot "Source\UEGTCore\Private\Content"
$TacticalSourceRoot = Join-Path $ProjectRoot "Source\UEGTCore\Private\Tactical"
$GameSourceRoot = Join-Path $ProjectRoot "Source\UEGTGame\Private"
$RequiredCultures = @("en", "fr", "de", "es", "ja")

if (-not (Test-Path -LiteralPath $CatalogPath -PathType Leaf))
{
	throw "Localization catalog was not found at '$CatalogPath'."
}

if (-not (Test-Path -LiteralPath $ContentSourceRoot -PathType Container))
{
	throw "Content-loader source was not found at '$ContentSourceRoot'."
}

if (-not (Test-Path -LiteralPath $TacticalSourceRoot -PathType Container))
{
	throw "Tactical runtime source was not found at '$TacticalSourceRoot'."
}

if (-not (Test-Path -LiteralPath $GameSourceRoot -PathType Container))
{
	throw "Game-layer source was not found at '$GameSourceRoot'."
}

$Catalog = Get-Content -LiteralPath $CatalogPath -Raw | ConvertFrom-Json
$SeenKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$ExactDiagnosticKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$PlaceholderPattern = [regex]::new("\{[0-9]+\}")

foreach ($Entry in $Catalog.entries)
{
	if ([string]::IsNullOrWhiteSpace([string] $Entry.key))
	{
		throw "Localization catalog contains an entry without a key."
	}

	if (-not $SeenKeys.Add([string] $Entry.key))
	{
		throw "Localization key '$($Entry.key)' is duplicated."
	}

	if ([string]::IsNullOrWhiteSpace([string] $Entry.source))
	{
		throw "Localization key '$($Entry.key)' has an empty source string."
	}

	$SourcePlaceholders = @($PlaceholderPattern.Matches([string] $Entry.source) | ForEach-Object Value | Sort-Object)
	foreach ($Culture in $RequiredCultures)
	{
		$Translation = [string] $Entry.translations.$Culture
		if ([string]::IsNullOrWhiteSpace($Translation))
		{
			throw "Localization key '$($Entry.key)' has no '$Culture' translation."
		}

		$TranslationPlaceholders = @($PlaceholderPattern.Matches($Translation) | ForEach-Object Value | Sort-Object)
		if (Compare-Object $SourcePlaceholders $TranslationPlaceholders)
		{
			throw "Localization key '$($Entry.key)' changes placeholders in '$Culture'."
		}
	}

	if ($Entry.key -like "diagnostic.*" -and $Entry.key -notlike "diagnostic.generic-*")
	{
		[void] $ExactDiagnosticKeys.Add([string] $Entry.key)
	}
}

$ContentDiagnosticCodes = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$DiagnosticPattern = [regex]::new(
	'(?:AddError|AddDiagnostic)\s*\([\s\S]{0,240}?TEXT\("([a-z0-9_]+)"\)',
	[System.Text.RegularExpressions.RegexOptions]::Singleline)
$SourceFiles = @(& rg --files $ContentSourceRoot -g "*.cpp")
if ($LASTEXITCODE -ne 0 -or $SourceFiles.Count -eq 0)
{
	throw "Could not enumerate content-loader source files with rg."
}

foreach ($SourceFile in $SourceFiles)
{
	$SourceText = Get-Content -LiteralPath $SourceFile -Raw
	foreach ($Match in $DiagnosticPattern.Matches($SourceText))
	{
		[void] $ContentDiagnosticCodes.Add($Match.Groups[1].Value)
	}
}

$MissingDiagnostics = @()
foreach ($Code in $ContentDiagnosticCodes)
{
	$NormalizedCode = $Code.Replace("_", "-")
	$Key = "diagnostic.$NormalizedCode"
	if (-not $ExactDiagnosticKeys.Contains($Key))
	{
		$MissingDiagnostics += $Code
	}
}

if ($MissingDiagnostics.Count -gt 0)
{
	$MissingDiagnostics = @($MissingDiagnostics | Sort-Object)
	throw "Player-facing content diagnostics lack exact localization entries: $($MissingDiagnostics -join ', ')."
}

$RuntimeDiagnosticCodes = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$TacticalSourceFiles = @(& rg --files $TacticalSourceRoot -g "*.cpp")
if ($LASTEXITCODE -ne 0 -or $TacticalSourceFiles.Count -eq 0)
{
	throw "Could not enumerate tactical runtime source files with rg."
}

$TacticalFallbackPattern = [regex]::new('FName\(TEXT\("([a-z0-9_]+)"\)\)')
foreach ($SourceFile in $TacticalSourceFiles)
{
	$SourceText = Get-Content -LiteralPath $SourceFile -Raw
	foreach ($Match in $DiagnosticPattern.Matches($SourceText))
	{
		[void] $RuntimeDiagnosticCodes.Add($Match.Groups[1].Value)
	}

	foreach ($Match in $TacticalFallbackPattern.Matches($SourceText))
	{
		if ($Match.Groups[1].Value -ne "armor")
		{
			[void] $RuntimeDiagnosticCodes.Add($Match.Groups[1].Value)
		}
	}
}

$GameDiagnosticPattern = [regex]::new(
	'(?:Diagnostic\.Code|UnavailableReasonCode)\s*=\s*TEXT\("([a-z0-9_]+)"\)')
$GameSourceFiles = @(& rg --files $GameSourceRoot -g "*.cpp")
if ($LASTEXITCODE -ne 0 -or $GameSourceFiles.Count -eq 0)
{
	throw "Could not enumerate game-layer source files with rg."
}

foreach ($SourceFile in $GameSourceFiles)
{
	if ($SourceFile -match '[\\/]Tests[\\/]')
	{
		continue
	}

	$SourceText = Get-Content -LiteralPath $SourceFile -Raw
	foreach ($Match in $GameDiagnosticPattern.Matches($SourceText))
	{
		[void] $RuntimeDiagnosticCodes.Add($Match.Groups[1].Value)
	}
}

$MissingRuntimeDiagnostics = @()
foreach ($Code in $RuntimeDiagnosticCodes)
{
	$NormalizedCode = $Code.Replace("_", "-")
	$Key = "diagnostic.$NormalizedCode"
	if (-not $ExactDiagnosticKeys.Contains($Key))
	{
		$MissingRuntimeDiagnostics += $Code
	}
}

if ($MissingRuntimeDiagnostics.Count -gt 0)
{
	$MissingRuntimeDiagnostics = @($MissingRuntimeDiagnostics | Sort-Object)
	throw "Player-facing tactical or game-layer diagnostics lack exact localization entries: $($MissingRuntimeDiagnostics -join ', ')."
}

Write-Host "Localization audit passed: $($Catalog.entries.Count) unique entries, $($ExactDiagnosticKeys.Count) exact diagnostics, $($ContentDiagnosticCodes.Count) content-loader codes, and $($RuntimeDiagnosticCodes.Count) tactical/game-layer codes covered across $($RequiredCultures.Count) cultures."
