#pragma once
#include "MaterialSlots.h"
#include "RHI/GraphicsTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // A material is TWO independent things:
    //
    //   ShadingModel : WHAT the pixels compute      (Unlit, Lit, Emissive, Sky, UI)   -> which shader
    //   RenderState  : HOW the result is drawn      (blend, cull, depth, queue)       -> which pipeline / draw order
    //
    // They used to be one enum (Unlit, Lit, PBR, Transparent, UI, Skybox, Emissive) that mixed both: "Transparent" was
    // really "Lit + alpha blending", "Skybox" was "Sky + cull front + background queue", and "PBR" was "Lit" under another
    // name. Every consumer then switched on the enum to rediscover the state (about fifty places), and Material's
    // setters even changed the enum behind your back. Now a transparent glass, a transparent unlit hologram and a blended
    // emissive glow are just different RenderStates on a shading model; nothing needs a new type or a new shader.
    // ==================================================================

    enum class ShadingModel : uint8_t
    {
        Unlit,      // colour x texture (+ emission), no lights
        Lit,        // metallic-roughness PBR, lit by the scene's lights, shadows and environment (the old Lit AND PBR)
        Emissive,   // a stylised glow: emission dominates, soft rim, boosted exposure (lamps, markers, screens)
        Sky,        // an environment cube map looked up by direction (needs its own vertex shader)
        UI,         // screen-space quads: texture x colour, no lighting
        Count
    };

    struct ShadingModelInfo
    {
        ShadingModel model;
        const char*  name;           // "Lit": logs, shader cache keys
        const char*  vertexShader;   // file in the shader directory
        const char*  pixelShader;
        bool         litByScene;     // receives the scene lights + shadows (compiled with those features)
    };

    namespace ShadingModels
    {
        const std::vector<ShadingModelInfo>& All();             // row i == ShadingModel i (tested)
        const ShadingModelInfo&              Info(ShadingModel model);
        const char*                          Name(ShadingModel model);
        bool                                 Parse(const std::string& name, ShadingModel& out);   // exact, case-sensitive
    }

    // ------------------------------------------------------------------
    // Draw order buckets.
    // ------------------------------------------------------------------
    enum class RenderQueue : uint32_t
    {
        Auto       = 0,       // not chosen: derived from the blend mode (see ResolveRenderQueue)
        Background = 1000,    // the sky: first, behind everything
        Opaque     = 2000,
        Transparent= 3000,    // blended: last of the world, sorted back to front
        UI         = 4000,
        Overlay    = 5000
    };

    enum class DepthWriteMode : uint8_t
    {
        Auto,    // write depth unless the surface is blended or has no depth test
        On,
        Off
    };

    // ------------------------------------------------------------------
    // HOW a material is drawn. Plain data; the helpers below resolve the "Auto" choices in ONE place, so the pipeline,
    // the extraction and the shadow pass cannot disagree about what a state means.
    // ------------------------------------------------------------------
    struct RenderState
    {
        RHI::BlendMode     blend      = RHI::BlendMode::Opaque;
        RHI::CullMode      cull       = RHI::CullMode::Back;
        RHI::DepthTestMode depthTest  = RHI::DepthTestMode::Less;
        DepthWriteMode     depthWrite = DepthWriteMode::Auto;
        RenderQueue        queue      = RenderQueue::Auto;

        bool operator==(const RenderState& o) const
        {
            return blend == o.blend && cull == o.cull && depthTest == o.depthTest && depthWrite == o.depthWrite && queue == o.queue;
        }
        bool operator!=(const RenderState& o) const { return !(*this == o); }

        // Presets for the common cases (a material starts from one of these and may change any field afterwards).
        static RenderState Opaque();     // the defaults
        static RenderState Blended();    // alpha blending; depth write resolves to off
        static RenderState Sky();        // inside-out dome behind everything: cull front, depth LessEqual, Background queue
        static RenderState Overlay();    // screen-space UI: no depth test, alpha blending, UI queue, no culling
    };

    bool        IsBlended(const RenderState& state);              // blend != Opaque
    RenderQueue ResolveRenderQueue(const RenderState& state);     // an explicit queue wins; else Transparent when blended, else Opaque
    bool        ResolveDepthWrite(const RenderState& state);      // On / Off win; Auto = Opaque blend && a depth test
}
