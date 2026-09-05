# UEGT content package authoring

UEGT loads original JSON content packages through schema version 1. The format is intentionally independent of OpenXcom and other legacy formats: do not copy legacy rules, identifiers, names, prose, artwork, or audiovisual assets into a UEGT package.

The runtime always loads the built-in `Content/Rules` catalog first. Before a campaign starts, it also discovers user packages recursively under `Saved/Mods`. Every `*.json` file in an enabled root is treated as one package.

## Install and reload

Place a package anywhere below the default user directory:

```text
Saved/
└── Mods/
    └── YourPackage/
        └── your-package.uegt.json
```

The default directory is optional. If it does not exist, the built-in catalog loads normally. On the main menu, use **RELOAD CONTENT + MODS** after adding or changing files. Reloading is deliberately unavailable once a campaign is active so the authoritative rule set cannot change underneath live state.

Two development command-line switches control discovery:

- `-UEGTModsDir=<directory>` replaces `Saved/Mods` with an explicit directory. A relative path is resolved from the project directory. An explicit missing or empty directory is an error, including `-UEGTModsDir=""` and whitespace-only values.
- `-UEGTNoUserMods` loads only the built-in catalog.

For example, launch the included sample directly from the repository:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  '.\UEGT.uproject' -game -UEGTModsDir='.\Samples\Mods'
```

## Package envelope

Each JSON file has one package envelope:

```json
{
  "schemaVersion": 1,
  "packageId": "example.signal-kit",
  "displayName": "Signal Kit",
  "version": "1.0.0",
  "priority": 100,
  "dependencies": ["uegt.base"],
  "loadAfter": ["uegt.base"],
  "rules": {
    "items": []
  }
}
```

| Field | Contract |
| --- | --- |
| `schemaVersion` | Required integer. Schema 1 is the only supported version. |
| `packageId` | Required stable ID: 3–64 lowercase ASCII characters, starting with a letter; digits, `.`, `_`, and `-` are allowed. The ID `none` is reserved. |
| `displayName` | Required non-empty author-facing package name. |
| `version` | Required non-empty compatibility string. Change it whenever save compatibility changes. |
| `priority` | Optional integer, default `0`. Lower values load first among packages whose ordering prerequisites are already satisfied. |
| `dependencies` | Optional required-package IDs. Missing dependencies reject the entire catalog. |
| `loadAfter` | Optional ordering hints. They apply only when the named package is present. |
| `rules` | Optional object containing typed rule arrays. |

JSON types are enforced before conversion. Integer fields require unquoted whole numbers in the signed 32-bit range; strings and booleans are rejected even when they could convert to a number. Names, versions, and IDs require strings, including every element of an ID array. Boolean fields require `true` or `false` without quotes. Invalid types produce an `invalid_field_type` diagnostic and reject the package.

Identifier spelling is validated before reaching Unreal's name storage. Uppercase aliases, oversized identifiers, and identifiers containing null characters reject the package with `invalid_field_value` diagnostics, even if an equivalent lowercase name has already loaded. Rejected IDs do not change how later valid packages load. Ordinary display text and prose retain their existing field-specific limits.

Dependency and `loadAfter` cycles are errors. Otherwise ties are resolved lexically by `packageId`, so discovery order, folder names, and filesystem enumeration never affect the result.

## Rule families

Schema 1 accepts these arrays under `rules`:

- `items`, `research`, `archiveEntries`, and `facilities`
- `personnelRoles`, `personnelDoctrines`, and `personnelCommendations`
- `craft`, `regions`, `contacts`, `adversaryPlans`, and `adversaryMissions`
- `tacticalTerrains`, `tacticalUnits`, and `tacticalMissions`

Every rule needs a stable `id` using the same lowercase ID grammar as package IDs. Use a namespace you control, such as `item.example-aurora-relay`, to avoid collisions. The built-in [UEGT base package](../Content/Rules/uegt.base.json) is a broad schema-1 field reference; the smaller [Aurora Relay sample](../Samples/Mods/AuroraRelay/aurora-relay.uegt.json) is the recommended starting point.

An item-only package can be as small as:

```json
{
  "schemaVersion": 1,
  "packageId": "example.aurora-relay",
  "displayName": "Aurora Relay Field Kit",
  "version": "1.0.0",
  "priority": 100,
  "dependencies": ["uegt.base"],
  "loadAfter": ["uegt.base"],
  "rules": {
    "items": [
      {
        "id": "item.example-aurora-relay",
        "displayName": "Aurora Relay",
        "category": "sensor",
        "purchaseCost": 24000,
        "sellValue": 9000,
        "mass": 4,
        "power": 22,
        "manufactureCost": 16000,
        "manufactureHours": 20,
        "requires": ["research.signal-analysis"]
      }
    ]
  }
}
```

Rule values and cross-references are checked after all packages are merged. Invalid types, bounds, enumerated values, duplicate IDs, missing references, unreachable adversary branches, and other structural contradictions reject the complete load. Unknown fields produce sorted warnings so misspellings remain visible.

## Regional simulation policies

Regional rules author partner centers, initial support, funding weights, pressure tolerance, and monthly recovery/loss rates. Civic Relief, Security Accord, and Crisis Mobilization are simulation policies rather than schema-1 region fields. The standard Crisis Mobilization policy becomes available at pressure 60, spends 15 regional support instead of campaign funds, and removes 25 pressure under the existing once-per-region monthly guard.

Resilience Charter is likewise a simulation policy rather than a schema-1 field. A partner at support 60 or higher may sign once for 250,000 campaign funds and 10 support. The persisted charter then scales that partner's support-tier contribution to 90%, eligible non-base mission-selection weight to 50%, and escape pressure to 75%. Signing is independent of the once-per-region monthly outreach guard. Save format v28 owns the durable signed state; v1-v27 migration defaults it to unsigned.

Horizon Compact is a global simulation policy, not a package field. Once at least two charter partners each retain support 50, ratification spends 400,000 campaign funds and 5 support from every current member. Signed members then contribute 95% of their support-tier funding. When an escaped mission targets a member, the charter first retains 75% of pressure and the compact redirects 33% of that retained amount to the least-pressure other member; checked ceiling arithmetic and lexical region IDs make the result deterministic. Later charters join automatically. Save format v29 owns the ratification flag; v1-v28 migration defaults it to unratified.

Reciprocal Aid is also a global simulation policy rather than a package field. After Horizon Compact ratification, one signed member at pressure 60 or higher may receive aid once per campaign month for 150,000 campaign funds. Up to 20 pressure moves to the least-pressure other signed member, with lexical region IDs breaking ties and the donor's remaining pressure capacity limiting the transfer; the target gains up to 5 support and the donor loses exactly 5. Total pressure is conserved, regional funding is recalculated, and the command consumes no random draw. Save format v30 owns the last-aid-month guard; v1-v29 migration defaults it to unused.

Compact Cohesion is another global simulation policy, not a schema-1 field. Once ratified, an active member withdraws whenever a support loss leaves it below 25 support. Withdrawal preserves the signed Resilience Charter's funding, mission-weight, and escape-pressure effects, but removes compact funding, future shared-pressure routing, and Reciprocal Aid eligibility. At support 40 or higher, restoring that member costs 100,000 campaign funds. The 25/40 hysteresis prevents threshold oscillation; checks, funding projections, and membership changes are deterministic and consume no random draw. Save format v31 owns the per-mandate withdrawal flag; v1-v30 migration treats every existing signed member as active.

Emergency Solidarity Vote is also a global simulation policy rather than a package field. Once per campaign month, a vote can target one withdrawn compact member. Every active member votes for the motion only when its pressure is at most 70 and paying the 2-support voter cost would keep it at or above the 25-support withdrawal threshold. A strict majority passes, spending 200,000 campaign funds, adding up to 12 support to the target, removing up to 15 target pressure, and charging each affirmative voter 2 support. The motion never restores membership automatically; a recovered target must still meet the 40-support restoration gate and use the separate restoration command. Ballots, effects, and tie-free ordering are deterministic and consume no random draw. Save format v32 owns the last-vote-month guard; v31 migration defaults it to unused.

Do not add crisis, charter, compact, aid, emergency-vote, ballot, withdrawal, or restoration threshold, cost, relief, funding, mission-weight, pressure-transfer, pressure-limit, support-transfer, voter-support, majority, or escape-pressure fields to package JSON. Content authors tune how often these choices appear through regional tolerance/support and adversary-mission pressure/support consequences; the active simulation configuration owns each exact exchange. Changes to authored inputs affect campaign balance and should be accompanied by a package-version update and paired long-horizon pressure tests. Changing only a simulation policy does not require a schema-1 package-version bump, but persisted policy state still requires the corresponding save migration.

## Tactical signal profiles

A sensor item becomes a player-carried signal projector when `power`, `tacticalRange`, and `tacticalActionPointCost` are all positive. Projector power is bounded to 1–100; range and AP cost use the same tactical bounds as other item actions. A partial profile is invalid.

```json
{
  "id": "item.example-signal-projector",
  "displayName": "Example Signal Projector",
  "category": "sensor",
  "purchaseCost": 26000,
  "sellValue": 10000,
  "mass": 4,
  "power": 24,
  "tacticalRange": 8,
  "tacticalActionPointCost": 4,
  "requires": ["research.signal-analysis"]
}
```

An adversary tactical unit may instead define an intrinsic profile with the all-or-none `signalPower`, `signalRange`, and `signalActionPointCost` fields. Intrinsic signal power is also bounded to 1–100. Player units cannot substitute an intrinsic profile for an equipped and carried projector, and adversary units do not accept an item-projector override.

Both profiles use the same authoritative resolve, range, visibility, line-of-sight, smoke, AP, hit/miss, suppression, and morale rules. A legal attempt always consumes exactly one deterministic tactical draw, including a miss. These profiles add no persisted fields, but changing them changes gameplay compatibility, so increment the package `version` before distributing an update.

## Base-defense supply profiles

A facility with a complete positive `baseDefenseAccuracy` and `baseDefenseDamage` profile may opt into finite base-defense supply by defining both `baseDefenseSupplyItemId` and `baseDefenseSupplyPerShot`. The referenced item must use the exact `base-defense-supply` category. A partial profile, a nonpositive quantity, a quantity above 100,000, an incompatible item category, or supply fields on a facility without a defense profile rejects the catalog.

```json
{
  "id": "item.example-perimeter-cell",
  "displayName": "Example Perimeter Cell",
  "category": "base-defense-supply",
  "purchaseCost": 15000,
  "sellValue": 5000,
  "mass": 3,
  "power": 0,
  "manufactureCost": 8000,
  "manufactureHours": 16,
  "requires": []
}
```

```json
{
  "id": "facility.example-defense-array",
  "displayName": "Example Defense Array",
  "buildCost": 400000,
  "buildHours": 300,
  "monthlyMaintenance": 30000,
  "gridWidth": 2,
  "gridHeight": 1,
  "maxIntegrity": 300,
  "baseDefenseAccuracy": 70,
  "baseDefenseDamage": 80,
  "baseDefenseSupplyItemId": "item.example-perimeter-cell",
  "baseDefenseSupplyPerShot": 2,
  "requires": []
}
```

Supply is pooled in the threatened base's authoritative inventory, and a battery never receives a partial load. At assault readiness time, Coordinated Line preserves stable facility-instance-ID order, Precision Screen sorts by integrity-scaled accuracy then damage, and Breach Breaker sorts by integrity-scaled damage then accuracy; both specialist policies use stable instance identity as the final tie-breaker. The chosen order governs complete-load allocation and actual fire. Resolution consumes a load only when that battery actually fires, including a miss. An unready battery, or a later battery skipped because an earlier shot destroyed the contact, consumes nothing. Multiple facilities may share one item pool or reference different compatible items.

Omitting both fields preserves schema-1 compatibility and gives that facility the historic unlimited-fire behavior. Adding, removing, or changing a supply profile changes gameplay compatibility even though it adds no save field, so update the package `version` before distributing the change.

Grid Overcharge is a simulation policy, not a schema-1 facility field. Its emergency cost per threat point, accuracy bonus, and damage percentage come from the active simulation configuration; the standard runtime uses 25,000 funds, +15 accuracy, and 125% damage. The attacker contact's authored `threatRating` therefore determines the total funding price, while each facility's existing integrity-scaled defense and supply profile still determines allocation and fire. Do not add overcharge fields to package JSON; schema-1 authors tune the inputs through contact threat, battery profiles, and finite-supply loads. Changing any of those authored inputs changes gameplay compatibility and should be accompanied by a package-version update.

## Adversary plan graphs and tactical mappings

An `adversaryPlans` rule names one finite strategy graph and identifies its stage-one opening through `openingMissionRuleId`. Each linked `adversaryMissions` rule supplies the same `planId`, a one-based `planStage`, and optional `escapeBranchMissionRuleId` and `thwartBranchMissionRuleId` successors. Every successor must stay in the same plan and be exactly one stage later. All linked missions must be reachable from the opening; cycles, skipped stages, cross-plan branches, missing openings, and orphaned nodes reject the complete catalog.

Only standalone missions and each plan's declared opening enter weighted scheduling. `minimumEscalation` and `selectionWeight` therefore govern whether and how often an opening is selected. Once a plan is active, its authored outcome branch launches immediately, consumes no second weighted-selection draw, and receives its own full `intervalHours` cadence. A node with neither successor is terminal. Use positive values for every required mission field even when an internal branch never enters the weighted pool.

Coalition counterplay is authored on a mission with `compactPeerSupportLossOnEscape` and `withdrawnCompactSupportGainOnThwarted`, each optional and bounded from 0 to 100. The escape value is difficulty-scaled with the mission's other escape consequences, then applied in lexical region-ID order to every active Horizon Compact member except the mission target; a member crossing below 25 support withdraws normally. The thwart value is not difficulty-scaled and restores support, in the same stable order, to every signed member that is already withdrawn. Recovery can reach the 40-support restoration gate but never restores membership automatically. Both paths recalculate exact regional and total funding, consume no additional random draw, and are inert until the Compact is ratified. These fields change campaign behavior and require a package-version update.

The standard runtime raises the adversary's deterministic escalation floor by one level for every two resolved missions; an escape can still accelerate the current level by one beyond that floor. Standard victory requires both 12 thwarted missions and escalation five, so an escalation-five opening is reachable after eight flawless thwarts before the campaign can end. These cadence and outcome settings belong to the simulation configuration rather than schema 1, but authors should test `minimumEscalation` against this real campaign road instead of only forcing a level in isolation.

Set `createsLandingSiteOnArrival` only when a visible intact arrival should leave a tactical site. That mission must also specify a positive `landingSiteLifetimeHours` and `landingSiteThreatBonus`; contact threat plus the bonus cannot exceed 10. The merged catalog must contain exactly one `tacticalMissions` recipe for the same contact with `context: "strategicSite"` and `siteType: "landing"`. Every contact that can be destroyed should likewise have one wreckage recipe. A mission with `targetsPlayerBase: true` must provide both `baseFacilityDamage` and `baseFacilitiesHit`, and its contact should have exactly one `context: "baseDefense"` recipe so the player can choose a ground defense.

Tactical mappings are unique by context, site type, and source contact. Multi-level strategic-site recipes require a traversable `verticalConnectorTerrainRuleId`; base-defense recipes are deliberately single-level and must omit multi-level layouts. Recovery objectives require a valid item reward and positive quantity, while disrupt and control objectives must not define a reward item. The built-in Cinder Lattice Sequence demonstrates a four-stage converging graph with a landing branch, three stage-three outcomes, two coalition-counterplay terminals, and separate wreckage, landing, and base-defense mappings.

Use the optional tactical-mission `aiPosture` field to author deterministic adversary behavior: `assault` preserves the legacy attack-then-advance flow, `signalPressure` prefers an available intrinsic signal projection over a conventional attack, `objectivePush` advances toward an active control objective even while a hostile is visible, and `sentinel` attacks from its current position before taking cover instead of advancing. Omit the field for backward-compatible assault behavior; signal pressure falls back to the normal attack/advance flow when the unit has no intrinsic signal profile. These choices alter tactical behavior and therefore require a package-version update.

## Explicit replacement

Adding a rule ID already defined by an earlier package is an error unless the later rule includes `"replace": true`. Replacement is whole-rule replacement, not a field merge, so specify every required and desired field:

```json
{
  "id": "item.field-scanner",
  "replace": true,
  "displayName": "Calibrated Field Scanner",
  "category": "sensor",
  "purchaseCost": 19000,
  "sellValue": 7000,
  "mass": 5,
  "power": 20,
	"tacticalRange": 8,
	"tacticalActionPointCost": 4,
  "manufactureCost": 12000,
  "manufactureHours": 18,
  "requires": ["research.signal-analysis"]
}
```

`replace` also fails when no earlier package defines the target. Depend on or load after the owning package so replacement order is explicit. This prevents a typo from silently creating or overwriting content.

## Transaction and save rules

Catalog loading is all-or-nothing across the built-in and user roots. If any required directory, file, package, dependency, override, value, or merged reference fails, no partial candidate catalog is published. The menu lists localized diagnostics and blocks campaign start until a clean reload succeeds.

Campaign saves record the sorted package ID/version fingerprint. A save made with a mod is rejected as incompatible when that package is absent or has a different version. Restore the matching package set and reload it before opening the save. This is intentional protection against interpreting persisted state with different rules.

Discovery and resolution do not consume gameplay randomness. Loaded filenames are normalized and sorted, duplicate roots/files are ignored, dependency resolution is deterministic, and resolved package versions are sorted before they enter a save.

## Text and distribution guidance

`displayName` is the safe fallback shown when no built-in localization key exists. Schema 1 does not ingest separate user localization bundles, so author custom display names with that limitation in mind.

Use only work you created or have permission to redistribute. Non-profit distribution does not waive copyright, trademark, license, privacy, or platform obligations. Give the package its own license and credits, avoid the `uegt.*` package namespace for third-party work, and never bundle secrets or local configuration.

## Verification checklist

1. Validate that every JSON file parses and uses a unique package ID.
2. Reload from the main menu and resolve every warning/error before starting a campaign.
3. Confirm the expected package IDs and versions appear under **CONTENT STATUS**.
4. Exercise new rules through the real strategic/tactical path, not only by inspecting JSON.
5. Save, reload with the same package set, then verify that removing or version-changing a package produces the intended incompatibility diagnostic.
6. Test deterministic replay and all five supported interface cultures when the package affects player-facing values.
