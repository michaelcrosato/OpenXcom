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

function Get-CatalogField($Object, [string] $Name)
{
	# JSON field names are case-sensitive, unlike PowerShell property access.
	foreach ($Property in $Object.PSObject.Properties)
	{
		if ($Property.Name -ceq $Name) { return ,$Property.Value }
	}
	return $null
}

function ConvertTo-CatalogCulture([string] $Name)
{
	$Name = $Name.Trim().ToLowerInvariant().Replace("_", "-")
	if ($Name.Length -gt 2 -and $Name[2] -eq '-') { return $Name.Substring(0, 2) }
	return $Name
}

function Get-IndexedPlaceholders([string] $Value, [string] $Context)
{
	$Indices = [System.Collections.Generic.List[int]]::new()
	foreach ($Match in [regex]::Matches($Value, '\{([0-9]+)(\})?'))
	{
		$Index = 0
		if (-not [int]::TryParse($Match.Groups[1].Value, [System.Globalization.NumberStyles]::None,
			[System.Globalization.CultureInfo]::InvariantCulture, [ref]$Index))
		{
			throw "$Context contains an indexed placeholder outside the 32-bit range."
		}
		if ($Match.Groups[2].Success) { $Indices.Add($Index) }
	}
	$Indices.Sort()
	return $Indices.ToArray()
}

$Json = Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8
if (-not $Json.TrimStart().StartsWith("{")) { throw "Localization catalog must be a JSON object." }
$JsonParameters = @{ InputObject = $Json }
if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey("DateKind"))
{
	# Preserve date-shaped UI text instead of coercing it to System.DateTime.
	$JsonParameters.DateKind = "String"
}
elseif ($PSVersionTable.PSEdition -eq "Core")
{
	throw "Localization validation requires Windows PowerShell 5.1 or PowerShell 7.5+ to preserve JSON string types."
}
$Catalog = ConvertFrom-Json @JsonParameters
$SchemaVersion = Get-CatalogField $Catalog "schemaVersion"
if (($SchemaVersion -isnot [int] -and $SchemaVersion -isnot [long] -and $SchemaVersion -isnot [double]) -or $SchemaVersion -ne 1)
{
	throw "Localization catalog requires numeric schemaVersion 1."
}
$CatalogId = Get-CatalogField $Catalog "catalogId"
# The runtime stores catalogId as an FName (NAME_SIZE - 1 characters maximum).
if ($CatalogId -isnot [string] -or $CatalogId.Length -gt 1023 -or $CatalogId -cnotmatch '\A[a-z][a-z0-9.-]*\z')
{
	throw "Localization catalogId must be a lowercase id of at most 1023 characters."
}
$SourceCulture = Get-CatalogField $Catalog "sourceCulture"
if ($SourceCulture -isnot [string] -or (ConvertTo-CatalogCulture $SourceCulture) -cne "en")
{
	throw "Localization sourceCulture must be English (en)."
}
$Cultures = Get-CatalogField $Catalog "cultures"
if ($Cultures -isnot [array] -or $Cultures.Count -ne $RequiredCultures.Count)
{
	throw "Localization cultures must be an array containing exactly en, fr, de, es, and ja."
}
$SeenCultures = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($Culture in $Cultures)
{
	if ($Culture -isnot [string]) { throw "Localization cultures must be strings." }
	$NormalizedCulture = ConvertTo-CatalogCulture $Culture
	if ($RequiredCultures -cnotcontains $NormalizedCulture -or -not $SeenCultures.Add($NormalizedCulture))
	{
		throw "Localization cultures must contain en, fr, de, es, and ja exactly once."
	}
}
$Entries = Get-CatalogField $Catalog "entries"
if ($Entries -isnot [array] -or $Entries.Count -eq 0)
{
	throw "Localization entries must be a non-empty array."
}
$SeenKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$ExactDiagnosticKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

foreach ($Entry in $Entries)
{
	if ($Entry -isnot [pscustomobject]) { throw "Localization entry must be an object." }
	$Key = Get-CatalogField $Entry "key"
	if ($Key -isnot [string] -or $Key -cnotmatch '\A[a-z][a-z0-9.-]*\z' -or
		-not $Key.Contains(".") -or $Key.EndsWith(".") -or $Key.Contains(".."))
	{
		throw "Localization entry key must be a lowercase dotted id."
	}

	if (-not $SeenKeys.Add($Key))
	{
		throw "Localization key '$Key' is duplicated."
	}

	$Source = Get-CatalogField $Entry "source"
	if ($Source -isnot [string] -or [string]::IsNullOrWhiteSpace($Source))
	{
		throw "Localization key '$Key' requires a non-empty source string."
	}

	$Translations = Get-CatalogField $Entry "translations"
	if ($Translations -isnot [pscustomobject]) { throw "Localization key '$Key' requires a translations object." }
	if (@($Translations.PSObject.Properties).Count -ne $RequiredCultures.Count)
	{
		throw "Localization key '$Key' must contain exactly five translations."
	}
	$SourcePlaceholders = @(Get-IndexedPlaceholders $Source "Localization key '$Key' source")
	foreach ($Culture in $RequiredCultures)
	{
		$Translation = Get-CatalogField $Translations $Culture
		if ($Translation -isnot [string] -or [string]::IsNullOrWhiteSpace($Translation))
		{
			throw "Localization key '$Key' requires a non-empty '$Culture' translation string."
		}
		if ($Culture -ceq "en" -and $Translation -cne $Source)
		{
			throw "Localization key '$Key' English translation must equal its source."
		}

		$TranslationPlaceholders = @(Get-IndexedPlaceholders $Translation "Localization key '$Key' translation '$Culture'")
		if (Compare-Object $SourcePlaceholders $TranslationPlaceholders)
		{
			throw "Localization key '$Key' changes placeholders in '$Culture'."
		}
	}

	if ($Key -like "diagnostic.*" -and $Key -notlike "diagnostic.generic-*")
	{
		[void] $ExactDiagnosticKeys.Add($Key)
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
