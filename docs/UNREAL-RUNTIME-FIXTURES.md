# UEGT runtime fixtures

These fixtures are available in non-shipping builds. Run the commands from the repository root after an editor build; change the engine path for your installation. `-UEGTCulture` selects the interface culture, and `-UEGTRuntimeCapture` requests a screenshot.

See the [build and test guide](../README-UNREAL.md) for prerequisites.

The non-shipping base-defense supply fixture runs through real campaign commands, manufactures two capacitors, waits for a perimeter assault, focuses the alert, and optionally captures its doctrine-specific allocation choices:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTBaseDefenseSupplyDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping coalition-counterplay fixture ratifies the Horizon Compact, prepares one active and one withdrawn member, launches the authored stage-four Ashen Accord Severance contact, focuses its detected plan card, and optionally captures the exact escape-strain and thwart-recovery projections:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 `
  -UEGTCoalitionCounterplayDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping service-history fixture focuses a concise active roster and memorial roll, deriving both careers from their mission counts so the exact next milestone can be verified in any supported interface culture:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 `
  -UEGTServiceHistoryDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Watchkeeper fixture uses the shared evaluator to place one Legacy Anchor and one Field Proven agent aboard a grounded transport, focuses the fleet card, and exposes the exact active +10 morale projection:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 `
  -UEGTWatchkeeperDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Legacy Relay fixture uses the same assigned team with a maxed Clear Sight doctrine, focuses the fleet card, and exposes the deterministic temporary +2 accuracy relay without changing persisted personnel health or doctrine state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTLegacyRelayDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Flight-Deck Rotation fixture focuses two craft sharing one operational maintenance lane. Relay 12 is active with a one-hour turnaround; Relay 27 is first in queue with a one-hour wait and an exact three-hour ready estimate:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 `
  -UEGTCraftServiceDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Return Path fixture focuses one newly injured field agent and derives all three recovery choices from the shared policy: a 60-hour no-cost Measured Return, 30-hour Surge Care for 20,000 funds, and 90-hour Reflection Cycle with +1 Resolve on completion. It also exposes the mandatory decision pause before strategic time can advance:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTRecoveryPlanDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Stewardship Rotation fixture focuses one experienced field agent serving a 30-day Training Cadre tour. Twelve days remain; training begun at that base is 25% faster; one completed tour is retained; and the current tour still awards +1 Resolve on completion:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTStewardshipDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Works Cadre fixture uses authoritative campaign commands to reserve three engineers, begin a 180-hour Secure Storage build with exactly 126 hours committed, and damage the Operations Hub so its 20-hour repair previews at 14 hours. The localized base card exposes the 3/3 staffing tradeoff, 30% front-load, active construction, and repair projection:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTWorksCadreDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Works Charter fixture extends that base card with three deterministic future-work policies. It selects Assembly Cadence and then Restoration Cadence through authoritative commands, exposing the exact 15% construction / 45% repair projection in French while preserving committed clocks, funds, and random-draw state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTWorksCharterDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Signal Watch fixture preserves the two-base Threadline choices, then assigns one of two available scientists to the source base's operational Operations Hub. That duty expands its Relay Weave from one facility channel to two total channels, immediately promoting the held FIFO convoy without changing cargo, route exposure, inbound-storage commitments, or random-draw state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTSignalWatchDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Threadline Retune fixture focuses an escorted Veiled Chain convoy held behind one active relay job. Its selected route remains disabled while Open Relay and Rapid Thread expose exact alternative duration, exposure, wait, and arrival projections; cargo, inbound storage, paid escort, funds, identity, and FIFO order stay committed:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTThreadlineRetuneDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Signal Surety fixture focuses an unescorted Rapid Thread convoy held behind one active relay job. It exposes the current configured escort cost and an exact before/after arrival projection: commissioning the escort recovers the pending deterministic interdiction delay without changing cargo, destination storage, route, convoy identity, dispatch order, source inventory, or random-draw state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTSignalSuretyDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Relief Priority fixture exposes a three-convoy Relay Weave: one active commitment, one earlier held commitment, and a never-departed target. Elevating the target rotates it to the front of the held line, previews every exact queue consequence, and leaves active work, cargo, storage, routes, escorts, funds, inventory, and random-draw state untouched:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTReliefPriorityDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Relief Stand-Down fixture exposes one active relay commitment, one escorted Rapid Thread convoy held before departure, and one later Open Relay commitment. Standing down the eligible convoy returns its exact cargo to the source, releases its destination-storage reservation, advances the later convoy by 48 hours, and makes the already-paid Signal Escort cost visibly non-refundable:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTReliefStandDownDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Relief Diversion fixture establishes a third base and focuses one Open Relay convoy held behind active work. Redirecting it from the high-pressure Patagonia route to the North Atlantic atomically moves four storage units, removes the pending 24-hour interdiction forecast, advances one later commitment by the same amount, and preserves cargo, identity, relay order, funds, active work, and random state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTReliefDiversionDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Relay Waypoint fixture keeps that third base as an intermediate stop instead of a new destination. A held Open Relay convoy reaches the North Atlantic waypoint after the active job, then takes a Rapid Thread onward leg to Patagonia; the screen exposes direct restoration plus all three onward doctrines, both exposure values, exact waypoint/final arrivals, and every follow-on queue shift while the source channel and final storage remain reserved end-to-end:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTRelayWaypointDemo -UEGTCulture=fr -UEGTRuntimeCapture
```

The non-shipping Balanced Handoff fixture focuses a six-unit convoy on that two-leg route and exposes the two stable cargo plans. The active plan reserves three units at the North Atlantic waypoint and keeps three committed to Patagonia; switching to Through Cargo would atomically restore all six to the final destination without changing relay timing, source-channel occupancy, escort, funds, or random state:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -windowed -ResX=1600 -ResY=900 -ForceRes -RenderOffscreen `
  -UEGTBalancedHandoffDemo -UEGTCulture=fr -UEGTRuntimeCapture
```
