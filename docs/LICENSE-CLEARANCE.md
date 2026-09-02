# License clearance packet

> **Current direction (2026-08-29):** the user selected a modified, original, non-profit Unreal implementation rather than an exact/direct source port. The GPL-derived Unreal spike was removed from the active project. This packet remains the mandatory gate only if direct OpenXcom source translation or content incorporation is reconsidered. Non-profit status alone does not resolve the incompatibility.

## Status

**Hold:** do not distribute, package, or continue direct translation of OpenXcom source into Unreal modules until written clearance is complete.

This is a release-management record, not legal advice. Qualified counsel must confirm the final structure and permissions.

## Why clearance is required

1. OpenXcom identifies the project as GPL software. Its source headers grant GPL version 3 or later, and the official GitHub repository identifies the repository license as GPL-3.0.
2. Section 6(c) of Epic's current Unreal Engine EULA identifies GPL as a "Non-Compatible License" and prohibits combining Unreal Licensed Technology with GPL-covered code or content.
3. A direct C++ translation of OpenXcom logic into an Unreal runtime module is a combined work for release-planning purposes unless qualified counsel and the relevant licensors determine otherwise.
4. The OpenXcom repository contains no contributor-license agreement, copyright assignment, project-wide linking exception, or alternate license at baseline commit `630130c5c9ac236b9e1d8496005fb23e84e397ca`.
5. Git history contains 154 distinct author identities with commits touching `src/`. Identity count is not a legal ownership determination, but it shows that project-wide relicensing cannot safely be assumed to rest with one maintainer.

Authoritative references:

- OpenXcom repository and license: <https://github.com/OpenXcom/OpenXcom>
- Unreal Engine EULA, especially section 6(c): <https://www.unrealengine.com/eula/unreal>
- Epic custom-license contact route: <https://www.unrealengine.com/license>
- Repository license text: `LICENSE.txt`

## Clearance needed to retain both requirements

Keeping both OpenXcom source as the implementation foundation and Unreal Engine requires written terms covering both sides of the conflict:

- **Epic:** a custom Unreal license that expressly permits this product to combine Unreal Licensed Technology with the applicable OpenXcom GPL code/content and permits a distribution model compatible with the OpenXcom-side obligations.
- **OpenXcom rights holders:** an alternate license or a narrowly drafted Unreal linking/combination exception for every copyrightable portion incorporated into the full port. A project-wide agreement is preferable; file-by-file clearance is not viable for a complete port unless ownership is exhaustively mapped.
- **Original X-COM intellectual property:** confirmation that the product will continue to require user-supplied lawful game data, plus separate review of names, logos, marketing, screenshots, music, audiovisual material, and any other protected content intended for distribution.

An Epic exception alone may not remove the GPL obligations on the OpenXcom side. An OpenXcom exception alone does not override Epic's EULA. Both must be compatible with the final source and binary distribution model.

## Evidence to obtain and retain

- Signed Epic custom license or amendment naming the legal entity, project, engine versions, permitted GPL combination, source distribution, platforms, royalties, support, and term.
- Signed OpenXcom alternate-license or linking-exception grants, with a schedule mapping each grant to copyright holders and covered files/commits.
- Counsel memorandum confirming that the two grants are mutually compatible and cover development, build automation, testing, source access, binary distribution, patches, and mod support.
- Trademark/content clearance or a documented user-supplied-assets model that excludes protected assets from every distributed artifact.
- A third-party software bill of materials and notices bundle for every shipped package.

Store executed agreements outside the public repository. Record only non-secret identifiers, effective dates, covered versions, and counsel-approved summaries here.

## Questions for Epic custom licensing

1. Will Epic grant written permission for this named product to combine UE 5.8.2-or-later Licensed Technology with OpenXcom code licensed under GPL-3.0-or-later?
2. What source-code delivery can satisfy OpenXcom obligations without distributing Epic Engine Code to unlicensed recipients?
3. May the project publish its game-module source under GPL plus an OpenXcom linking exception while distributing Unreal Engine only in the object-code form allowed by Epic?
4. Does the permission cover development tools, CI, crash symbols, patches, mod SDKs, dedicated servers if later required, and every intended storefront/platform?
5. Which attribution, royalty, seat, notice, audit, and reporting terms supersede or supplement the standard EULA?

## Questions for OpenXcom maintainers and rights holders

1. Does any non-public CLA, copyright assignment, or alternate-license authority exist for the project?
2. Would the necessary rights holders consider an Unreal-specific linking/combination exception or a compatible alternate license for the covered code and standard data?
3. Who owns or can license the standard rulesets, translations, bundled media, modified SDL_gfx code, and other non-code content required for feature parity?
4. Is there an authoritative contributor/copyright map beyond Git authorship history?
5. What attribution, source-availability, modification-marking, and upstream-contribution conditions would be required?

## Resume gate

Direct source porting may resume only after all of the following are true:

- [ ] The project's distributing legal entity and intended distribution model are identified.
- [ ] Epic's written terms expressly cover the GPL combination.
- [ ] OpenXcom-side written grants cover the full source/content scope being ported.
- [ ] Original X-COM asset, trademark, and marketing boundaries are approved.
- [ ] Counsel confirms compatibility of all terms.
- [ ] The approved licenses, notices, source-offer process, and SBOM requirements are encoded into CI release gates.

## Alternatives if clearance is unavailable

- A genuinely independent, counsel-designed clean-room reimplementation in Unreal can target observable game behavior without copying OpenXcom implementation or protected content. It does not satisfy the requirement to use OpenXcom source as the implementation foundation.
- An OpenXcom-derived full port can use a GPL-compatible engine. It does not satisfy the Unreal Engine requirement.
- Keeping OpenXcom as a separate process or executable is not a full engine port and is legally uncertain under Epic's broad "combine or otherwise use" language; it is not an assumed workaround.
