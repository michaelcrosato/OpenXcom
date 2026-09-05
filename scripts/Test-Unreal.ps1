[CmdletBinding()]
param(
	[ValidateNotNullOrEmpty()]
	[string] $TestFilter = "UEGT.Core",

	[string] $EngineRoot = "C:\Program Files\Epic Games\UE_5.8"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "UEGT.uproject"
$EditorCommand = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
# Unreal clears Saved/Automation/Logs when a test session starts.
$LogDirectory = Join-Path $ProjectRoot "Saved\Logs\Automation"
$LogFile = Join-Path $LogDirectory ("UEGT-{0}-{1}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss"), [guid]::NewGuid().ToString("N"))
$LocalizationAudit = Join-Path $PSScriptRoot "Test-LocalizationCatalog.ps1"

if (-not (Test-Path -LiteralPath $EditorCommand -PathType Leaf))
{
	throw "Unreal command-line editor was not found at '$EditorCommand'. Pass -EngineRoot for the installed engine."
}

& $LocalizationAudit -ProjectRoot $ProjectRoot

[void] (New-Item -ItemType Directory -Path $LogDirectory -Force)
Write-Host "Automation log: $LogFile"
& $EditorCommand $ProjectFile -Unattended -NoSplash -NoP4 -NullRHI "-ExecCmds=Automation RunTests $TestFilter;Quit" "-TestExit=Automation Test Queue Empty" "-AbsLog=$LogFile" -Log
$EditorExitCode = $LASTEXITCODE

if (-not (Test-Path -LiteralPath $LogFile -PathType Leaf))
{
	throw "Unreal exited with code $EditorExitCode but did not produce the expected automation log '$LogFile'."
}

# Surface the useful failure details even when the command-line editor only logs
# them to disk. Each invocation reads its own log, never a prior editor session.
Select-String -LiteralPath $LogFile -Pattern "LogAutomationController: Error:|LogAutomationCommandLine: Error:|No automation tests matched|Fatal error:|Assertion failed:" |
	ForEach-Object { Write-Host $_.Line }

if ($EditorExitCode -ne 0)
{
	Write-Host "Unreal automation failed with exit code $EditorExitCode. Inspect '$LogFile'."
	exit $EditorExitCode
}

$Discovery = Select-String -LiteralPath $LogFile -Pattern "Found [1-9][0-9]* automation tests based on" | Select-Object -Last 1
$Completion = Select-String -LiteralPath $LogFile -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" | Select-Object -Last 1
if ($null -eq $Discovery -or $null -eq $Completion)
{
	throw "Unreal exited without a successful automation completion marker. Inspect '$LogFile'."
}

Write-Host $Discovery.Line
Write-Host $Completion.Line
