#include "MaterialTypes.h"

namespace DXEngine
{
    namespace ShadingModels
    {
        namespace
        {
            // ONE row per shading model, in enum order. The shader files live here and nowhere else.
            // (Unlit, Lit and Emissive share one vertex shader: they all read the standard vertex layout.)
            const std::vector<ShadingModelInfo>& Table()
            {
                static const std::vector<ShadingModelInfo> kTable = {
                    //  model                  name        vertex shader         pixel shader         lit by the scene
                    { ShadingModel::Unlit,    "Unlit",    "Standard.vs.hlsl",   "Unlit.ps.hlsl",     false },
                    { ShadingModel::Lit,      "Lit",      "Standard.vs.hlsl",   "Lit.ps.hlsl",       true  },
                    { ShadingModel::Emissive, "Emissive", "Standard.vs.hlsl",   "Emissive.ps.hlsl",  false },
                    { ShadingModel::Sky,      "Sky",      "Skybox.vs.hlsl",     "Skybox.ps.hlsl",    false },
                    { ShadingModel::UI,       "UI",       "UI.vs.hlsl",         "UI.ps.hlsl",        false },
                };
                return kTable;
            }
        }

        const std::vector<ShadingModelInfo>& All() { return Table(); }

        const ShadingModelInfo& Info(ShadingModel model)
        {
            const size_t i = static_cast<size_t>(model);
            return Table()[i < Table().size() ? i : static_cast<size_t>(ShadingModel::Lit)];   // an invalid value is a bug: stay in bounds
        }

        const char* Name(ShadingModel model) { return Info(model).name; }

        bool Parse(const std::string& name, ShadingModel& out)
        {
            for (const ShadingModelInfo& row : Table())
                if (name == row.name) { out = row.model; return true; }
            return false;
        }
    }

    RenderState RenderState::Opaque() { return RenderState{}; }

    RenderState RenderState::Blended()
    {
        RenderState s;
        s.blend = RHI::BlendMode::AlphaBlend;
        return s;
    }

    RenderState RenderState::Sky()
    {
        RenderState s;
        s.cull      = RHI::CullMode::Front;           // we are INSIDE the dome: its inside faces are the back faces
        s.depthTest = RHI::DepthTestMode::LessEqual;  // the vertex shader pins it to the far plane
        s.queue     = RenderQueue::Background;
        return s;
    }

    RenderState RenderState::Overlay()
    {
        RenderState s;
        s.blend     = RHI::BlendMode::AlphaBlend;
        s.cull      = RHI::CullMode::None;
        s.depthTest = RHI::DepthTestMode::None;
        s.queue     = RenderQueue::UI;
        return s;
    }

    bool IsBlended(const RenderState& state) { return state.blend != RHI::BlendMode::Opaque; }

    RenderQueue ResolveRenderQueue(const RenderState& state)
    {
        if (state.queue != RenderQueue::Auto) return state.queue;
        return IsBlended(state) ? RenderQueue::Transparent : RenderQueue::Opaque;
    }

    bool ResolveDepthWrite(const RenderState& state)
    {
        switch (state.depthWrite)
        {
        case DepthWriteMode::On:  return true;
        case DepthWriteMode::Off: return false;
        default:
            // Blended geometry must not write depth: whatever is drawn behind it (back-to-front order) would be rejected.
            return !IsBlended(state) && state.depthTest != RHI::DepthTestMode::None;
        }
    }
}
