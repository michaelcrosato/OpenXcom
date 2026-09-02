# Aurora Relay sample mod

Aurora Relay is a small, original schema-1 example for UEGT. It depends on `uegt.base` and adds one research-gated sensor item without replacing built-in rules.

To install it into the default user-mod location from the repository root:

```powershell
New-Item -ItemType Directory -Force -Path '.\Saved\Mods\AuroraRelay'
Copy-Item -LiteralPath '.\Samples\Mods\AuroraRelay\aurora-relay.uegt.json' `
  -Destination '.\Saved\Mods\AuroraRelay\aurora-relay.uegt.json'
```

Then open the main menu and select **RELOAD CONTENT + MODS**. `sample.aurora-relay 1.0.0` should appear under **CONTENT STATUS**. Remove the copied folder to uninstall it; restore it before loading any campaign saved with the sample enabled.

See [content package authoring](../../../docs/CONTENT-AUTHORING.md) for ordering, replacement, validation, command-line, and save-compatibility rules.

The sample package is original UEGT material released under the adjacent MIT license. It contains no OpenXcom code, data, names, narrative, or audiovisual assets.
