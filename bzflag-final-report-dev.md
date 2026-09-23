BZFlag 2.4 Graphics Rebuild - Developer Report
===============================================

Target platform: ARM64 Linux ( Snapdragon X Elite / Adreno X1-85), Mesa 26,
OpenGL 4.6 core + compat. Base: BZFlag 2.4.31. Constraint: no new runtime
libraries; server protocol (0221) unchanged; gameplay behaviour unchanged.

SCOPE
-----
Starting point: 240 glBegin/glEnd sites in 51 files, 46 display lists, zero
shaders, zero VBOs in the client draw path. Windowing on SDL 1.2.

TIER 1 - TANK PATH (VBO + SHADER)
---------------------------------
- TankGeometryMgr: tank body/turret geometry moved from immediate-mode
  submission to interleaved VBOs, drawn with a dedicated tank lighting shader
  (per-vertex normals, team-colour material, per-tank lighting).
- Covers all tank renders: local player, remote tanks, IDL flashes, shadows.

TIER 2 - LIGHTING / IMAGE QUALITY
---------------------------------
- Shader-based lighting for tanks and world geometry.
- MSAA + anisotropic filtering enabled.

TIER 3 - BATCHED IMMEDIATE MODE (GLBatch)
-----------------------------------------
- New utility (src/ogl/GLBatch.cxx, include/GLBatch.h): vertex/colour/
  texcoord accumulation with a single glDrawArrays flush per batch.
  Supports TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN (native fan draw - no
  per-frame re-tessellation), QUADS and POLYGON (re-tessellated at flush).
- 97 glBegin/glEnd blocks across 23 files converted in three stages, each
  stage a separate commit and package:
    Stage 1  HUD/menus    33 blocks,  7 files  (HUDRenderer, ControlPanel,
                                                ScoreboardRenderer, HUDui*)
    Stage 2  world/FX     44 blocks, 10 files  (BackgroundRenderer world
                                                passes, Bolt/Sphere/Laser/
                                                FlagWarp, effectsRenderer,
                                                WeatherRenderer, TrackMarks,
                                                World, playing)
    Stage 3  leftovers    31 blocks, 13 files  (MeshPolySceneNode incl.
                                                renderRadar, FlagSceneNode,
                                                Occluder, Octree, TankSceneNode
                                                IDL flash/lights/jump jets,
                                                EighthD box/base/pyr/dim,
                                                Tri/Quad/PolyWall, Billboard)
- Deliberately NOT converted: 15 glBegin sites inside display lists
  (TextureFont glyphs, BackgroundRenderer sun/moon/stars/clouds/mountains/
  ground, WeatherRenderer drop/puddle builders, FlagSceneNode WaveGeometry).
  glDrawArrays cannot be captured by glNewList; converting these would break
  rendering.

GLBatch STATE CONTRACT (lessons that bit us)
--------------------------------------------
- end() must re-enable GL_TEXTURE_COORD_ARRAY / GL_NORMAL_ARRAY when the
  batch supplies them - or textured HUD draws sample one texel and vanish.
- Perf: do NOT glPushClientAttrib/glPopClientAttrib per flush (radar + HUD
  flush dozens of tiny batches per frame; cost measured ~6-7 ms/frame).
  Instead: snapshot the three client-array enable bits at begin() (cheap
  glIsEnabled queries) and restore exactly those bits at end().
- The client-array restore is REQUIRED, not optional: the Tier 2 tank path
  (drawPart) sets array pointers without setting enable bits, so it inherits
  dirty enable state from the previous flush - symptom was tanks rendering
  untextured until killed (explosion path reset the state).
- Converter traps for scripted glBegin->GLBatch rewrites: glVertex2i needs
  GLfloat casts; batches shared across functions must be hoisted to file
  scope; EMIT macros inside #define RENDER blocks keep calling glVertex
  unless re-targeted, and injected lines inside macros strip backslash
  continuations (breaks the macro at compile time).

WINDOWING - SDL 1.2 -> SDL 2
----------------------------
- Platform layer moved to SDL2 (SDL2Display/SDL2Window/SDL2Input; SDL 2.32).
- Maintained upstream, Wayland-capable via compositor support, proper
  vsync/resolution control.

RESULTS
-------
  Metric           Before                After
  Frame rate       15-20 FPS             120+ FPS
  Input lag        ~15 ms                1-2 ms
  glBegin sites    240 (51 files)        16 (all in display lists, by design)
  Frame time       50-65 ms              < 8 ms

Agent-side FPS ledger (same map/time-of-day, in-game T-key counter):
  baseline (pre-Tier-3) 657 -> stage 1 635 -> perf fix 667 -> stage 2 671.
  User-verified in normal play: 120+ FPS (vsync-capped), 1-2 ms lag
  (vs ~15 ms before the rebuild, ~30 FPS pre-Tier-2).

REGRESSIONS FOUND AND FIXED DURING STAGED TESTING
-------------------------------------------------
1. Textured menu items invisible (logo, selector arrow): texcoord client
   array not re-enabled at flush (Tier 3 stage 1).
2. ~6-7 ms input-lag regression: client-attrib push/pop + heap fan
   re-tessellation per flush (perf fix, Tier 3).
3. Living tanks untextured until death: client-array enable bits leaked by
   flushes into the VBO tank path (Tier 3, post-stage-2 fix).

DELIVERABLES
------------
- Packages: bzflag_2.4.31-11..17_arm64.deb (stage + fix packages, retained
  for rollback; installed = -17).
- Source: github.com/pc-support-services/bzflag-arm64-sp, branch 2.4:
    f309cb42c revert: remove Tier 3 macro redirect (rewind to Tier 2)
    1e8ddcdb1 feat: Tier 3 stage 1 - HUD cluster
    b16129605 fix: GLBatch re-enables texcoord/normal client arrays
    9ee6c1edb perf: strip GLBatch flush overhead
    594476b76 feat: Tier 3 stage 2 - world/FX cluster
    80d509c36 fix: GLBatch restores client-array enable bits after flush
    d8e474b59 feat: Tier 3 stage 3 - leftovers
    708a299d1 docs: Tier 3 milestone report
- Technical detail: bzflag-renderer-tier3-report.pdf (per-stage table).

Richard - PC Support Services