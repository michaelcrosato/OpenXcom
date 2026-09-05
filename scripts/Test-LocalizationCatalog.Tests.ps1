[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Validator = Join-Path $PSScriptRoot "Test-LocalizationCatalog.ps1"
$FixtureParent = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot "Saved\Automation\LocalizationCatalog"))
$FixtureRoot = Join-Path $FixtureParent ([guid]::NewGuid().ToString("N"))
$CatalogPath = Join-Path $FixtureRoot "Content\Localization\uegt.ui.json"
$Utf8 = [System.Text.UTF8Encoding]::new($false)
$script:CaseCount = 0

function Invoke-CatalogCase($Name, $Json, $ExpectedError)
{
	[System.IO.File]::WriteAllText($CatalogPath, $Json, $Utf8)
	$Failure = $null
	try { & $Validator -ProjectRoot $FixtureRoot *> $null }
	catch { $Failure = $_.Exception.Message }
	if ($ExpectedError)
	{
		if ($null -eq $Failure -or -not $Failure.Contains($ExpectedError))
		{
			throw "${Name}: expected rejection containing '$ExpectedError'; got '$Failure'."
		}
	}
	elseif ($null -ne $Failure)
	{
		throw "${Name}: expected success; got '$Failure'."
	}
	$script:CaseCount++
}

try
{
	$SourceFiles = @{
		"Source\UEGTCore\Private\Content\Probe.cpp" = 'AddError(Result, TEXT("content_probe"), TEXT("probe"));'
		"Source\UEGTCore\Private\Tactical\Probe.cpp" = 'AddDiagnostic(Diagnostics, TEXT("tactical_probe"), TEXT("probe"));'
		"Source\UEGTGame\Private\Probe.cpp" = 'Diagnostic.Code = TEXT("game_probe");'
		"Source\UEGTGame\Private\Tests\Probe.cpp" = 'Diagnostic.Code = TEXT("test_only_probe");'
	}
	foreach ($RelativePath in $SourceFiles.Keys)
	{
		$Path = Join-Path $FixtureRoot $RelativePath
		[void] (New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force)
		[System.IO.File]::WriteAllText($Path, $SourceFiles[$RelativePath], $Utf8)
	}
	[void] (New-Item -ItemType Directory -Path (Split-Path -Parent $CatalogPath) -Force)
	$Entries = @(foreach ($Key in @("test.label", "diagnostic.content-probe", "diagnostic.tactical-probe", "diagnostic.game-probe"))
	{
		[ordered]@{ key = $Key; source = "State {0}"; translations = [ordered]@{
			en = "State {0}"; fr = "Etat {0}"; de = "Status {0}"; es = "Estado {0}"; ja = "State {0}"
		} }
	})
	$BaseJson = [ordered]@{
		schemaVersion = 1; catalogId = "uegt.test"; sourceCulture = "en"
		cultures = @("en", "fr", "de", "es", "ja"); entries = $Entries
	} | ConvertTo-Json -Depth 10 -Compress
	Invoke-CatalogCase "Valid catalog and test-only diagnostic exclusion" $BaseJson
	$Cases = @(
		@{ Name = "English case mismatch"; Error = "English"; Change = { param($C) $C.entries[0].translations.en = "state {0}" } }
		@{ Name = "Unsupported schema"; Error = "schemaVersion"; Change = { param($C) $C.schemaVersion = 2 } }
		@{ Name = "String schema"; Error = "schemaVersion"; Change = { param($C) $C.schemaVersion = "1" } }
		@{ Name = "Boolean schema"; Error = "schemaVersion"; Change = { param($C) $C.schemaVersion = $true } }
		@{ Name = "Missing schema"; Error = "schemaVersion"; Change = { param($C) $C.PSObject.Properties.Remove("schemaVersion") } }
		@{ Name = "Invalid catalog ID"; Error = "catalogId"; Change = { param($C) $C.catalogId = "Bad.ID" } }
		@{ Name = "Oversized catalog ID"; Error = "catalogId"; Change = { param($C) $C.catalogId = "a" * 1024 } }
		@{ Name = "Longest supported catalog ID"; Change = { param($C) $C.catalogId = "a" * 1023 } }
		@{ Name = "Malformed source culture"; Error = "sourceCulture"; Change = { param($C) $C.sourceCulture = "english" } }
		@{ Name = "Cultures must be an array"; Error = "cultures"; Change = { param($C) $C.cultures = "en,fr,de,es,ja" } }
		@{ Name = "Duplicate culture"; Error = "cultures"; Change = { param($C) $C.cultures[4] = "fr" } }
		@{ Name = "Unsupported culture"; Error = "cultures"; Change = { param($C) $C.cultures[4] = "pt" } }
		@{ Name = "Malformed culture"; Error = "cultures"; Change = { param($C) $C.cultures[1] = "french" } }
		@{ Name = "Entries must be an array"; Error = "entries"; Change = { param($C) $C.entries = $C.entries[0] } }
		@{ Name = "Empty entries"; Error = "entries"; Change = { param($C) $C.entries = @() } }
		@{ Name = "Null entry"; Error = "entry"; Change = { param($C) $C.entries[0] = $null } }
		@{ Name = "Invalid key"; Error = "key"; Change = { param($C) $C.entries[0].key = "test..label" } }
		@{ Name = "Duplicate key"; Error = "duplicated"; Change = { param($C) $C.entries += $C.entries[0] } }
		@{ Name = "Empty source"; Error = "source"; Change = { param($C) $C.entries[0].source = " " } }
		@{ Name = "Numeric source"; Error = "source"; Change = { param($C) $C.entries[0].source = 42 } }
		@{ Name = "Incorrect field casing"; Error = "source"; Change = { param($C) $C.entries[0].PSObject.Properties.Remove("source"); $C.entries[0] | Add-Member -NotePropertyName Source -NotePropertyValue "State {0}" } }
		@{ Name = "Non-object translations"; Error = "translations"; Change = { param($C) $C.entries[0].translations = @("en", "fr", "de", "es", "ja") } }
		@{ Name = "Extra translation"; Error = "five"; Change = { param($C) $C.entries[0].translations | Add-Member -NotePropertyName pt -NotePropertyValue "State {0}" } }
		@{ Name = "Missing translation"; Error = "five"; Change = { param($C) $C.entries[0].translations.PSObject.Properties.Remove("ja") } }
		@{ Name = "Numeric translation"; Error = "translation"; Change = { param($C) $C.entries[0].translations.fr = 42 } }
		@{ Name = "Incorrect translation casing"; Error = "translation"; Change = { param($C) $C.entries[0].translations.PSObject.Properties.Remove("en"); $C.entries[0].translations | Add-Member -NotePropertyName EN -NotePropertyValue "State {0}" } }
		@{ Name = "Missing placeholder"; Error = "placeholders"; Change = { param($C) $C.entries[0].translations.fr = "Etat" } }
		@{ Name = "Duplicate placeholder"; Error = "placeholders"; Change = { param($C) $C.entries[0].translations.fr = "{0} {0}" } }
		@{ Name = "Source placeholder overflow"; Error = "32-bit"; Change = { param($C) $C.entries[0].source = "{2147483648}" } }
		@{ Name = "Unclosed placeholder overflow"; Error = "32-bit"; Change = { param($C) $C.entries[0].source = "{2147483648" } }
		@{ Name = "Translation placeholder overflow"; Error = "32-bit"; Change = { param($C) $C.entries[0].translations.fr = "{2147483648}" } }
		@{ Name = "Missing content diagnostic"; Error = "content diagnostics"; Change = { param($C) $C.entries = @($C.entries | Where-Object key -ne "diagnostic.content-probe") } }
		@{ Name = "Missing game diagnostic"; Error = "game-layer diagnostics"; Change = { param($C) $C.entries = @($C.entries | Where-Object key -ne "diagnostic.game-probe") } }
		@{ Name = "Regional cultures"; Change = { param($C) $C.sourceCulture = " EN-us "; $C.cultures[1] = "FR_ca" } }
		@{ Name = "Equivalent numeric placeholders"; Change = { param($C) $C.entries[0].translations.fr = "Etat {00}" } }
		@{ Name = "Date-shaped text remains a string"; Change = { param($C) $C.entries[0].source = "2026-09-05T00:00:00Z"; foreach ($Property in $C.entries[0].translations.PSObject.Properties) { $Property.Value = $C.entries[0].source } } }
	)
	foreach ($Case in $Cases)
	{
		$Catalog = $BaseJson | ConvertFrom-Json
		& $Case.Change $Catalog
		Invoke-CatalogCase $Case.Name ($Catalog | ConvertTo-Json -Depth 10 -Compress) $Case.Error
	}
	Invoke-CatalogCase "Array root" ("[" + $BaseJson + "]") "object"
	Write-Host "Localization validator regression tests passed: $script:CaseCount cases."
}
finally
{
	if (Test-Path -LiteralPath $FixtureRoot)
	{
		$Resolved = (Resolve-Path -LiteralPath $FixtureRoot).Path
		if ($Resolved -ne [System.IO.Path]::GetFullPath($FixtureRoot) -or
			-not $Resolved.StartsWith($FixtureParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase))
		{
			throw "Refusing to remove a fixture directory outside '$FixtureParent'."
		}
		Remove-Item -LiteralPath $Resolved -Recurse -Force
	}
}
