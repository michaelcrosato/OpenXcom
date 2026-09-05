[CmdletBinding()]
param(
	[ValidateNotNullOrEmpty()]
	[string] $TestFilter = "UEGT.Core",

	[string] $EngineRoot = "C:\Program Files\Epic Games\UE_5.8",

	[ValidateNotNullOrEmpty()]
	[string] $RuntimeRoot
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "UEGT.uproject"
$EditorCommand = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$RuntimeMode = $PSBoundParameters.ContainsKey("RuntimeRoot")
$RunKind = if ($RuntimeMode) { "Runtime" } else { "Editor" }
# Unreal clears Saved/Automation/Logs when a test session starts.
$LogDirectory = Join-Path $ProjectRoot "Saved\Logs\Automation"
$LogFile = Join-Path $LogDirectory ("UEGT-{0}-{1}-{2}.log" -f $RunKind, (Get-Date -Format "yyyyMMdd-HHmmss"), [guid]::NewGuid().ToString("N"))
$LocalizationAudit = Join-Path $PSScriptRoot "Test-LocalizationCatalog.ps1"
$LocalizationValidatorTests = Join-Path $PSScriptRoot "Test-LocalizationCatalog.Tests.ps1"

if ($RuntimeMode)
{
	$RuntimeCommand = Join-Path $RuntimeRoot "UEGT\Binaries\Win64\UEGT.exe"
	if (-not (Test-Path -LiteralPath $RuntimeCommand -PathType Leaf))
	{
		throw "UEGT Development runtime was not found at '$RuntimeCommand'. Pass -RuntimeRoot for a staged Development build."
	}
	$RuntimeCommand = (Resolve-Path -LiteralPath $RuntimeCommand).Path
}
elseif (-not (Test-Path -LiteralPath $EditorCommand -PathType Leaf))
{
	throw "Unreal command-line editor was not found at '$EditorCommand'. Pass -EngineRoot for the installed engine."
}

& $LocalizationValidatorTests
& $LocalizationAudit -ProjectRoot $ProjectRoot

[void] (New-Item -ItemType Directory -Path $LogDirectory -Force)
Write-Host "Automation log: $LogFile"
if ($RuntimeMode)
{
	# The packaged GUI executable must be waited on explicitly. Keep paths and
	# console-command arguments quoted when Start-Process joins the arguments.
	$RuntimeArguments = @("-Unattended", "-NoSplash", "-NoP4", "-NullRHI", "-UEGTNoUserMods",
		('-ExecCmds="Automation RunTests ' + $TestFilter + ';Quit"'),
		'-TestExit="Automation Test Queue Empty"', ('-AbsLog="' + $LogFile + '"'), "-Log")
	$RuntimeProcess = Start-Process -FilePath $RuntimeCommand -WorkingDirectory (Split-Path -Parent $RuntimeCommand) `
		-ArgumentList $RuntimeArguments -WindowStyle Hidden -PassThru
	Write-Host "Runtime automation PID: $($RuntimeProcess.Id)"
	$RuntimeProcess.WaitForExit()
	$TestExitCode = $RuntimeProcess.ExitCode
}
else
{
	& $EditorCommand $ProjectFile -Unattended -NoSplash -NoP4 -NullRHI -UEGTNoUserMods "-ExecCmds=Automation RunTests $TestFilter;Quit" "-TestExit=Automation Test Queue Empty" "-AbsLog=$LogFile" -Log
	$TestExitCode = $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $LogFile -PathType Leaf))
{
	throw "Unreal exited with code $TestExitCode but did not produce the expected automation log '$LogFile'."
}

# Surface the useful failure details even when Unreal only logs them to disk.
# Each invocation reads its own log, never a prior Editor or runtime session.
Select-String -LiteralPath $LogFile -Pattern "LogAutomationController: Error:|LogAutomationCommandLine: Error:|No automation tests matched|Fatal error:|Assertion failed:" |
	ForEach-Object { Write-Host $_.Line }

if ($TestExitCode -ne 0)
{
	Write-Host "Unreal automation failed with exit code $TestExitCode. Inspect '$LogFile'."
	exit $TestExitCode
}

$Discovery = Select-String -LiteralPath $LogFile -Pattern "Found [1-9][0-9]* automation tests based on" | Select-Object -Last 1
$Completion = Select-String -LiteralPath $LogFile -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" | Select-Object -Last 1
if ($null -eq $Discovery -or $null -eq $Completion)
{
	throw "Unreal exited without a successful automation completion marker. Inspect '$LogFile'."
}

Write-Host $Discovery.Line
Write-Host $Completion.Line
