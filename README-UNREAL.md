# UEGT

UEGT is an original, non-profit global strategy and tactical defense game for Unreal Engine 5.8.2 or newer. Its target remains a complete game—not a prototype or vertical slice—with campaign strategy, base management, research, manufacturing, interception, turn-based tactical combat, progression, modding, saving, localization, and release-quality presentation.

OpenXcom is retained in this worktree as a GPL reference catalog for system breadth. UEGT is intentionally not an exact reproduction: production code, data formats, rules, names, narrative, interface, art, audio, and audiovisual content are original. The Unreal targets do not compile, include, link, or load the OpenXcom source or bundled data.

## Project layout

The active project is `UEGT.uproject`.

- `Source/UEGTCore/`: deterministic campaign, strategic and tactical rules, content loading, save storage, and domain automation.
- `Source/UEGTGame/`: Unreal runtime, native Slate/UMG interface, localization, audio, input, and presentation automation.
- `Content/`: original gameplay packages and the interface catalog.
- `Samples/Mods/AuroraRelay/`: an example original content package.
- `src/`, `bin/`, and the root CMake project: retained OpenXcom reference code and resources.

The original Unreal code under `Source/` is [MIT licensed](Source/LICENSE.txt). OpenXcom reference files retain their existing license terms. Original X-COM resources are not required by the Unreal targets.

## Requirements

- Unreal Engine 5.8.2 or newer.
- A Visual Studio toolchain supported by that Unreal installation.
- Windows PowerShell 5.1 or PowerShell 7.5+ and `rg` (ripgrep) on `PATH`. The preflight preserves JSON string types, including date-shaped UI text, and uses `rg` to scan source diagnostics.

The scripts default to `C:\Program Files\Epic Games\UE_5.8`. Pass `-EngineRoot` to use another installation.

## Build and test

Run from the repository root:

```powershell
./scripts/Build-Unreal.ps1
./scripts/Test-Unreal.ps1
```

Build before testing: the default test runner uses the compiled editor modules. The default `UEGT.Core` filter includes both domain and game/presentation tests. Every run first tests the localization validator, then checks the catalog in the worktree for the required schema and field types, exact field casing, unique dotted keys, five cultures, exact English/source equality, bounded placeholder indexes and multiplicity, and source-diagnostic coverage. Unreal must discover tests and report successful completion for the script to pass. Automation disables user-mod discovery; tests that exercise mods load their own fixtures.

Each invocation prints a unique Editor or Runtime log path under `Saved/Logs/Automation` and surfaces automation errors in the console. Logs from previous sessions cannot satisfy the current run's completion check, and later automation sessions retain earlier logs.

For a focused regression run:

```powershell
./scripts/Test-Unreal.ps1 -TestFilter UEGT.Core.CampaignSaveStore
```

To verify behavior that differs outside the Editor, build and stage a Development game, then pass its `Windows` directory to the runner:

```powershell
./scripts/Build-Unreal.ps1 -Target UEGT -Configuration Development
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  "-project=$PWD/UEGT.uproject" -nop4 -platform=Win64 -clientconfig=Development `
  -skipbuild -cook -stage -pak -iostore -unattended -utf8output `
  "-stagingdirectory=$PWD/Saved/RuntimeAutomation"
./scripts/Test-Unreal.ps1 -RuntimeRoot './Saved/RuntimeAutomation/Windows'
```

Rebuild and restage after source or content changes. Runtime mode launches the staged `UEGT.exe` and runs only tests marked for `ClientContext`. These cover non-editor identifier casing, campaign seed input, PCM streaming, and audio playback completion. The playback test requires a running game world and an Unreal audio device. Keep the full Editor suite as a separate check. Shipping builds exclude development automation tests.

The localization checks can also run without an Unreal installation:

```powershell
./scripts/Test-LocalizationCatalog.Tests.ps1
./scripts/Test-LocalizationCatalog.ps1
```

For the production target:

```powershell
./scripts/Build-Unreal.ps1 -Target UEGT -Configuration Shipping
```

## Run locally

After building the editor target:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900
```

The native shell provides campaign setup, strategy, base management, and tactical play. [Runtime fixtures](docs/UNREAL-RUNTIME-FIXTURES.md) give repeatable commands for specific interface states and screenshots in non-shipping builds.

Campaign seeds accept whole decimal integers from `-9223372036854775808` through `9223372036854775807`, with an optional sign, surrounding whitespace, and leading zeros. Fractions, exponent notation, trailing text, embedded null characters, and out-of-range values are rejected before starting a campaign.

## Saves and mods

Campaign slots live under the runtime project's `Saved/SaveGames`. Each slot uses `.uegtsave`, `.uegtsave.tmp`, and `.uegtsave.bak` candidates. Writes verify the temporary file before promotion. Loading validates available candidates against the requested catalog and selects the newest valid timestamp, preferring primary, temporary, then backup on ties.

Resaving retains the newest compatible prior candidate as the backup. If no candidate matches the new catalog, the newest valid candidate from another catalog is retained instead. A recovered temporary file is moved to the backup before its path is reused; a corrupt, stale, or incompatible primary cannot replace the selected recovery copy. Save timestamps never move behind campaign creation or the retained candidate, so a backward system-clock adjustment cannot make a successful save load older progress.

Save readers enforce JSON scalar types before checksum validation. Signed 64-bit values use canonical decimal strings to retain full precision; ordinary integer fields use JSON numbers. Numbers and booleans cannot stand in for text or ID strings, and quoted numbers cannot stand in for integer fields. Identifier spelling is validated before Unreal name construction, so uppercase aliases, oversized IDs, and embedded null characters return diagnostics in both Editor and standalone builds.

User packages are discovered under `Saved/Mods`. Save compatibility depends on the loaded package IDs and versions. See [content authoring](docs/CONTENT-AUTHORING.md) and the [Aurora Relay sample](Samples/Mods/AuroraRelay/README.md) for installation, reload, replacement, and validation rules.

## Status and further documentation

The game has substantial strategic and tactical systems, but release completion is still open. Authored tactical presentation, broader usability and performance evidence, and packaged release validation remain in the [implementation status and release gates](docs/PORTING.md).

[Development history](docs/UNREAL-DEVELOPMENT-HISTORY.md) retains earlier feature descriptions and verification checkpoints. Its historical test and catalog totals should not be used as evidence for the current worktree; run the build and automation commands above.
