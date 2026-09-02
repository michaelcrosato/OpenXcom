[CmdletBinding()]
param(
	[string] $TestFilter = "UEGT.Core",

	[string] $EngineRoot = "C:\Program Files\Epic Games\UE_5.8"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "UEGT.uproject"
$EditorCommand = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$LogFile = Join-Path $ProjectRoot "Saved\Logs\UEGT.log"
$LocalizationAudit = Join-Path $PSScriptRoot "Test-LocalizationCatalog.ps1"

if (-not (Test-Path -LiteralPath $EditorCommand -PathType Leaf))
{
	throw "Unreal command-line editor was not found at '$EditorCommand'. Pass -EngineRoot for the installed engine."
}

& $LocalizationAudit -ProjectRoot $ProjectRoot

& $EditorCommand $ProjectFile -Unattended -NoSplash -NoP4 -NullRHI "-ExecCmds=Automation RunTests $TestFilter;Quit" "-TestExit=Automation Test Queue Empty" -Log
if ($LASTEXITCODE -ne 0)
{
	exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $LogFile -PathType Leaf))
{
	throw "Unreal exited successfully but did not produce the expected automation log '$LogFile'."
}

$Discovery = Select-String -LiteralPath $LogFile -Pattern "Found [0-9]+ automation tests" | Select-Object -Last 1
$Completion = Select-String -LiteralPath $LogFile -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" | Select-Object -Last 1
if ($null -eq $Discovery -or $null -eq $Completion)
{
	throw "Unreal exited without a successful automation completion marker. Inspect '$LogFile'."
}

Write-Host $Discovery.Line
Write-Host $Completion.Line
