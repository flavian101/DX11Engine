# DXEngine — Open Issues Tracker

Source of truth for **open** problems found by review of the Renderer branch (code, shaders, ROADMAP, materials, lighting, variants).
All items below are **Open**. Status values: `Open` | `In progress` | `Fixed` | `Won't fix`.

Related issues are grouped into **work packages**. Fixing a package together is cheaper than fixing items one-by-one.

When an item is fixed: set **Status** to Fixed, note the commit, and update [ROADMAP.md](ROADMAP.md) in the same change if that file mentioned it.

---

## Tracker summary

| Package | Title | Issues | Priority |
|---------|--------|--------|----------|
| [WP1](#wp1-alpha-and-transparency) | Alpha & transparency | I-01 … I-03 | High |
| [WP2](#wp2-normals-tangents-parallax) | Normals, tangents, parallax | I-04 … I-07 | High |
| [WP3](#wp3-ibl-and-environment) | IBL & environment | I-08 … I-11 | High |
| [WP4](#wp4-material-flags-vs-reality) | Material flags vs reality | I-12 … I-17 | Medium |
| [WP5](#wp5-shader-variants-and-cache) | Shader variants & cache | I-18 … I-23 | Medium |
| [WP6](#wp6-lighting-and-shadows) | Lighting & shadows | I-24 … I-28 | Medium |
| [WP7](#wp7-shader-dead-code-and-bugs) | Shader dead code & bugs | I-29 … I-34 | Medium |
| [WP8](#wp8-architecture-and-api-leaks) | Architecture & API leaks | I-35 … I-39 | Medium |
| [WP9](#wp9-editor-play-mode-and-gpu-validation) | Editor, play mode, GPU validation | I-40 … I-45 | High (validation) |
| [WP10](#wp10-docs-license-assets) | Docs, license, assets | I-46 … I-48 | Low |

---

## WP1: Alpha and transparency

**Goal:** Real cut-out materials and correct blended behaviour; stop forcing alpha-tested content into the blended path.

### I-01 — `ShaderFeature::AlphaTest` is never set
- **Status:** Open
- **Area:** ShaderCache, Material, RenderState
- **Problem:** The feature bit and `ENABLE_ALPHA_TEST` path exist. Nothing in C++ sets `ShaderFeature::AlphaTest`. Cut-out (leaves, fences, grass) can only be approximated by blending.
- **Related:** I-02, I-03
- **Exit:** A material (or import rule) can request AlphaTest; variant compiles with threshold 0.5; depth write stays on; no transparent sort required.

### I-02 — Cut-out forced into Blended
- **Status:** Open
- **Area:** Material / extraction / pipelines
- **Problem:** Blended materials get soft alpha-test (threshold 0.1), no depth write, transparent queue. Wrong for mask materials.
- **Related:** I-01
- **Exit:** AlphaTest materials stay in opaque (or dedicated) queue with depth write; only true translucent use Blended.

### I-03 — No alpha-tested shadow casters
- **Status:** Open
- **Area:** Shadow pass, MeshDrawer, extraction
- **Problem:** Transparent materials never cast; there is no alpha-test shadow path either. Foliage shadows are missing or solid wrong.
- **Related:** I-01
- **Exit:** Shadow caster VS/PS (or clip in existing caster) honours opacity/diffuse alpha when material is AlphaTest.

---

## WP2: Normals, tangents, parallax

**Goal:** Normal maps and height maps work whenever the UI says they should; no silent geometric-normal fallback.

### I-04 — Normal maps require tangents with no hard guarantee
- **Status:** Open
- **Area:** Import, MeshProcessor, shaders
- **Problem:** `HAS_NORMAL_MAP` only bends the normal when `HAS_TANGENT_ATTRIBUTE` is set. Missing tangents → silent geometric normals while the inspector still shows Normal strength / map.
- **Related:** I-05, I-06
- **Exit:** Import or first use always generates tangents when a normal (or height) map is bound; or the editor warns and blocks the slot.

### I-05 — Detail normals rarely populated on import
- **Status:** Open
- **Area:** MaterialProcessor, MaterialMapPlan
- **Problem:** Shader blends detail normals (RNM); importer almost never fills `TextureSlot::DetailNormal`.
- **Related:** I-04
- **Exit:** Map plan assigns detail normals when present; or document as editor-only.

### I-06 — Parallax UV clamp breaks tiling
- **Status:** Open
- **Area:** `common.hlsli` ParallaxOcclusionMapping
- **Problem:** Final UVs are clamped to [0,1], so repeating height maps fail.
- **Related:** I-07
- **Exit:** Use wrap/sampler addressing; remove clamp (or make it optional).

### I-07 — Parallax quality / scale limitations
- **Status:** Open
- **Area:** Shaders, Material
- **Problem:** Fixed 8–32 layers; default import height scale 0.05; no cheap offset-only fallback; high cost always on when height is bound.
- **Related:** I-06
- **Exit:** Optional quality tier or simple parallax path; sensible defaults per asset.

---

## WP3: IBL and environment

**Goal:** Environment lighting is a scene resource with real data, not material slots that only the sky uses.

### I-08 — No producer for prefiltered environment / irradiance (ROADMAP D9)
- **Status:** Open
- **Area:** Renderer, assets, shaders
- **Problem:** Lit path samples `environmentTexture` at `roughness * 8` and optional irradiance. Nothing bakes a GGX mip chain or irradiance cube. Non-sky objects get flat ambient; metals go dark.
- **Related:** I-09, I-10, I-11
- **Exit:** Bake (or load) prefiltered specular (documented mip count) + irradiance; scene owns environment; materials sample scene IBL by default.

### I-09 — IBL slots only useful on sky materials today
- **Status:** Open
- **Area:** Material assignment, SceneFactory
- **Problem:** Only the sky dome typically receives environment textures; other materials never get maps even if slots exist.
- **Related:** I-08
- **Exit:** Default Lit materials use scene environment without per-material cubes unless overridden.

### I-10 — BRDF_LUT slot is dead
- **Status:** Open
- **Area:** MaterialSlots, `surface.hlsli`
- **Problem:** `TextureSlot::BRDF_LUT` / `HAS_BRDF_LUT` declared; shaders always use analytic `EnvBRDFApprox`.
- **Related:** I-08
- **Exit:** Either sample a real LUT in the split-sum path or remove the slot and define.

### I-11 — Ambient-as-environment hack for metals without IBL
- **Status:** Open
- **Area:** `surface.hlsli` ShadeSurface
- **Problem:** When `!HAS_ENVIRONMENT_MAP`, ambient is also used as a specular environment so metals are not pure black. Masks missing IBL rather than fixing it.
- **Related:** I-08
- **Exit:** Remove or gate behind debug once real IBL exists.

---

## WP4: Material flags vs reality

**Goal:** Every inspector control and CB field either drives shading or is removed.

### I-12 — `shininess` in material CB never read
- **Status:** Open
- **Area:** MaterialProperties, HLSL cbuffer
- **Problem:** Field reserved and static-asserted; no shader uses it (PBR uses roughness).
- **Related:** I-13
- **Exit:** Remove from CB + C++ or wire a legacy path; keep layout versioned.

### I-13 — `specularColor` unused by Lit
- **Status:** Open
- **Area:** Lit / surface
- **Problem:** Only a weak tint in Emissive.ps. Lit is metallic-roughness only.
- **Related:** I-12
- **Exit:** Document as unused for Lit, or support specular workflow explicitly.

### I-14 — Second UV never used for sampling
- **Status:** Open
- **Area:** VS + surface sampling
- **Problem:** `HAS_SECOND_UV_ATTRIBUTE` passes `texCoord1`; detail maps use scaled primary UV only.
- **Related:** I-05
- **Exit:** Detail (or a chosen slot) samples `texCoord1` when present.

### I-15 — Emissive shading model duplicates Lit (ROADMAP D7)
- **Status:** Open
- **Area:** ShadingModel, Emissive.ps
- **Problem:** Separate model/files but same surface evaluation; “one fact, two places.”
- **Related:** I-16
- **Exit:** Fold into Lit + style/emission weight, or keep Emissive with a clear, minimal shader and no duplicated rules.

### I-16 — Unlit vs Lit tonemapping mismatch
- **Status:** Open
- **Area:** Unlit.ps vs Lit.ps `ToDisplay`
- **Problem:** Unlit applies gamma only; Lit uses exposure tonemap then gamma. Side-by-side mismatch under non-neutral exposure.
- **Related:** I-15
- **Exit:** Shared display transform policy (with an explicit “raw” option if needed).

### I-17 — Imported material edits not fully round-tripped
- **Status:** Open
- **Area:** Scene serializer / asset bridge
- **Problem:** Model materials edited in the editor save as the model’s material; original model values can return on load (known limit).
- **Related:** —
- **Exit:** Overrides stored as entity-owned materials or explicit override records.

---

## WP5: Shader variants and cache

**Goal:** Predictable variant set, live reload, no dead feature flags.

### I-18 — Texture-mask combinatorial explosion
- **Status:** Open
- **Area:** ShaderVariantKey
- **Problem:** `textureMask` is part of the key (up to 2^14 × layouts × models × blend). Rare map combinations compile unique shaders with no budget or uber fallback.
- **Related:** I-19
- **Exit:** Cap / group masks, or a max-feature variant + dynamic branching for rare bits; surface stats in Renderer panel.

### I-19 — Hot-reload never enabled (ROADMAP A4 / E2)
- **Status:** Open
- **Area:** ShaderCache::Update, editor
- **Problem:** `enableHotReload` false; Update not driven from editor. Shader edits need restart.
- **Related:** I-18
- **Exit:** Editor enables reload; failed compiles show errors instead of silent black materials.

### I-20 — Base shaders compiled without defines then largely unused
- **Status:** Open
- **Area:** ShaderCache::LoadBaseShader
- **Problem:** Init compiles raw files; draws always use CreateShaderVariant. Base entries are mostly scaffolding.
- **Related:** I-19
- **Exit:** Stop compiling unused bases or use them only as explicit fallbacks.

### I-21 — Shadows always compiled into every Lit variant
- **Status:** Open
- **Area:** CombineFeatures
- **Problem:** All Lit variants get `ENABLE_SHADOWS`; runtime `ReceiveShadows` only skips lookups.
- **Related:** I-18
- **Exit:** Optional: separate receive-shadows feature if variant count must drop; or keep as-is and document cost.

### I-22 — AlphaTest flag dead in variant key
- **Status:** Open
- **Area:** ShaderFeature
- **Problem:** Same as I-01 from the cache side.
- **Related:** I-01
- **Exit:** See WP1.

### I-23 — Variant stats not visible in editor
- **Status:** Open
- **Area:** ShaderCacheStats, Renderer panel
- **Problem:** Hit/miss/failure counters exist; not shown in UI.
- **Related:** I-18
- **Exit:** Renderer panel shows variant count, hit rate, last failures.

---

## WP6: Lighting and shadows

**Goal:** Correct soft cascades, validated bias, tighter spot clustering.

### I-24 — No inter-cascade blending
- **Status:** Open
- **Area:** `DirectionalShadow` in common.hlsli
- **Problem:** Cascade chosen by view-Z; fade only past the **last** split. Hard seams possible between cascades.
- **Related:** I-25
- **Exit:** Blend near split boundaries using split distances already in the light CB.

### I-25 — Shadow bias values never validated on GPU
- **Status:** Open
- **Area:** Shadow system, ROADMAP
- **Problem:** `normalBias`, `constantBias`, raster biases chosen by reasoning only.
- **Related:** I-24
- **Exit:** Tune on real meshes (including normal-mapped); document defaults.

### I-26 — Spot lights clustered as spheres
- **Status:** Open
- **Area:** LightClustering
- **Problem:** Assignment uses spherical bounds; cone is only applied in the pixel shader. Extra false-positive lights per cluster.
- **Related:** —
- **Exit:** Cone-aware cluster bounds or acceptance test (ROADMAP “deliberately not built” → promote when needed).

### I-27 — Point-light shadow face seams
- **Status:** Open
- **Area:** LocalShadow
- **Problem:** Face by dominant axis; no filtering across face boundaries.
- **Related:** I-25
- **Exit:** Acceptable for now or add seam-aware filtering later.

### I-28 — GeometrySchlickGGX comment vs IBL path
- **Status:** Open
- **Area:** common.hlsli
- **Problem:** Comment claims different k for IBL; function always uses direct k; IBL uses EnvBRDFApprox. Misleading for future LUT work.
- **Related:** I-10
- **Exit:** Fix comment or implement real IBL G/LUT path.

---

## WP7: Shader dead code and bugs

**Goal:** HLSL matches the engine that still ships; no footguns.

### I-29 — `GetScaledTexCoord` wrong and unused
- **Status:** Open
- **Area:** common.hlsli
- **Problem:** Uses `uv * textureScale * textureOffset` (should be scale + offset); returns float3; no callers.
- **Related:** I-30
- **Exit:** Delete or fix and use everywhere.

### I-30 — Dead UI / Basic vertex structs
- **Status:** Open
- **Area:** common.hlsli
- **Problem:** `UIVertexInput` / `UIVertexOutput` / unused `BasicVertexInput` after UI removal.
- **Related:** I-29
- **Exit:** Remove.

### I-31 — Fog mentioned, not implemented
- **Status:** Open
- **Area:** Standard VS comment
- **Problem:** viewDir length kept “for distance fog”; no fog in shaders.
- **Related:** —
- **Exit:** Implement fog or remove the comment.

### I-32 — Normal scale partially cancelled by renormalize
- **Status:** Open
- **Area:** SampleNormalMap
- **Problem:** After `normal.xy *= normalScale`, full normalize reduces high-strength effect.
- **Related:** I-04
- **Exit:** Standard “scale xy then re-normalize carefully” formulation or document limited range.

### I-33 — Sky has no procedural/atmosphere path
- **Status:** Open
- **Area:** Skybox.ps
- **Problem:** Cube sample + tint + emissive only.
- **Related:** I-08
- **Exit:** Optional procedural sky later; not required for IBL bake.

### I-34 — Missing advanced PBR (clear coat, anisotropy, transmission, …)
- **Status:** Open
- **Area:** Materials / shaders
- **Problem:** Not in scope yet; listed so flags are not added without a plan.
- **Related:** —
- **Exit:** Explicit ROADMAP phase before any half-wired slots.

---

## WP8: Architecture and API leaks

**Goal:** Core stays free of hidden globals; types live in the right place.

### I-35 — `Renderer::Get` / `PrimaryDevice` still used (ROADMAP D8)
- **Status:** Open
- **Area:** Loaders, some panels, SceneFactory
- **Problem:** Device/renderer not always passed explicitly.
- **Related:** I-36
- **Exit:** Call sites take device or renderer; statics only for true legacy bridges.

### I-36 — Core types still under `utils/` (ROADMAP D8)
- **Status:** Open
- **Area:** Mesh, Material, Transform
- **Problem:** Core types live in utils.
- **Related:** I-35
- **Exit:** Move to appropriate modules without breaking includes (alias headers if needed).

### I-37 — Camera not a scene component (ROADMAP D4)
- **Status:** Open
- **Area:** Camera / scene
- **Problem:** Behaviour-blending stack heavier than a fly camera needs; camera outside ECS.
- **Related:** I-40
- **Exit:** Camera component or simplified controller; editor camera serialised if required.

### I-38 — Esc still quits app (ROADMAP B7 / P5)
- **Status:** Open
- **Area:** EditorApp
- **Problem:** Desired: Esc stops play mode only.
- **Related:** I-40
- **Exit:** Esc → stop play when playing; quit only via explicit menu/shortcut.

### I-39 — LICENSE.txt still Premake copyright (ROADMAP E3)
- **Status:** Open
- **Area:** LICENSE.txt
- **Problem:** BSD-3 text with Premake project copyright line.
- **Related:** I-48
- **Exit:** Owner sets correct copyright holder and license text.

---

## WP9: Editor, play mode, and GPU validation

**Goal:** Features marked “done in code/tests” are proven on a real Windows GPU run.

### I-40 — Play mode / snapshot not validated on GPU
- **Status:** Open
- **Area:** PlayMode, SceneSnapshot, GameRuntime
- **Problem:** Unit-tested without GPU; F1, toolbar, lamp demo, skinned mesh + shadow through Play/Stop, restore timing with real textures still unchecked.
- **Related:** I-41, I-42
- **Exit:** Checklist in ROADMAP §5 signed off on hardware.

### I-41 — Save / Open / View 2 / multi-view resize not GPU-validated
- **Status:** Open
- **Area:** Editor
- **Problem:** File dialogs, last scene, View 2, resize with two views open need real runs.
- **Related:** I-40
- **Exit:** Same checklist.

### I-42 — Depth pre-pass and shadow bias need visual sign-off
- **Status:** Open
- **Area:** Renderer
- **Problem:** Code present; ROADMAP still lists GPU check.
- **Related:** I-25
- **Exit:** Visual + timing notes attached to this issue.

### I-43 — Undo / redo and multi-select missing (P3d)
- **Status:** Fixed in code and tests (GPU check pending): undo / redo (`UndoHistory`) and multi-selection (an ordered `EditorSelection` with a primary; the operations act on the selection's roots). Editing the same VALUE on several entities at once in the Inspector is not included: the Inspector shows the primary
- **Area:** Editor
- **Problem:** After save/load, largest editor gaps.
- **Related:** —
- **Exit:** Stack for transform/property edits; multi-entity selection operations.

### I-44 — Shader compile errors not user-visible
- **Status:** Open
- **Area:** ShaderCache + editor
- **Problem:** Failures log; material may go fallback/black without a clear Assets/console message.
- **Related:** I-19
- **Exit:** Surface compile errors in Assets log / status bar.

### I-45 — Starting scene / ProjectRoot edge cases
- **Status:** Open
- **Area:** ProjectRoot, EditorPrefs
- **Problem:** Code + tests exist; start from another cwd, missing last_scene, etc. need GPU/manual pass.
- **Related:** I-40
- **Exit:** ROADMAP §5 items checked.

---

## WP10: Docs, license, assets

### I-46 — ROADMAP vs this tracker
- **Status:** Open
- **Area:** docs
- **Problem:** ROADMAP remains design source of truth; this file is the open-issue punch list. They can drift.
- **Related:** —
- **Exit:** When fixing, update both or link issue IDs from ROADMAP.

### I-47 — Large assets in git (C6 policy)
- **Status:** Open (policy)
- **Area:** repo
- **Problem:** ~450 MB tree; pre-commit blocks new >10 MiB; LFS deliberately not used. Revisit when thresholds in ROADMAP hit.
- **Related:** —
- **Exit:** Per ROADMAP C6 revisit criteria.

### I-48 — License copyright line
- **Status:** Open
- **Area:** LICENSE.txt
- **Problem:** Same as I-39; tracked here for release hygiene.
- **Related:** I-39
- **Exit:** Correct holder before public release tagging.

---

## Suggested order of attack

1. **WP9 (subset)** — Run the Editor once on Windows; file bugs under I-40–I-42 with evidence.
2. **WP1** — AlphaTest end-to-end (highest visual “flags don’t work” payoff for foliage).
3. **WP2** — Tangents guaranteed when normal/height bound; parallax clamp fix.
4. **WP3** — Minimal IBL bake or honest removal of dead env slots.
5. **WP7** — Delete dead HLSL; fix `GetScaledTexCoord` if kept.
6. **WP5** — Hot-reload + variant visibility.
7. **WP6** — Cascade blend + bias pass.
8. **WP4 / WP8 / WP10** — Cleanup and API hygiene as you touch those areas.

---

## Change log

| Date | Note |
|------|------|
| 2026-11-25 | Initial tracker from Renderer-branch review (code, shaders, ROADMAP open items, materials/lighting/variants). All issues Open. |
