[CmdletBinding()]
param(
	[ValidateSet("UEGT", "UEGTEditor")]
	[string] $Target = "UEGTEditor",

	[ValidateSet("DebugGame", "Development", "Shipping")]
	[string] $Configuration = "Development",

	[string] $EngineRoot = "C:\Program Files\Epic Games\UE_5.8"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot "UEGT.uproject"
$BuildScript = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"

if (-not (Test-Path -LiteralPath $BuildScript -PathType Leaf))
{
	throw "Unreal build script was not found at '$BuildScript'. Pass -EngineRoot for the installed engine."
}

& $BuildScript $Target Win64 $Configuration "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
if ($LASTEXITCODE -ne 0)
{
	exit $LASTEXITCODE
}
