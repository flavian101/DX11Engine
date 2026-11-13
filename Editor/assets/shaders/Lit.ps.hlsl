// Lit.ps.hlsl: the surface shader for every material that is lit by the scene's lights (opaque, cut-out or blended: the
// BLEND STATE is not decided here, it is part of the material's render state).
//
// All the work is in surface.hlsli: EvaluateSurface() reads the material, ShadeSurface() lights it.

#include "common.hlsli"
#include "surface.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    const float3 V = normalize(input.viewDir);
    const Surface surface = EvaluateSurface(input, V);

#if ENABLE_ALPHA_TEST
    clip(surface.alpha - ALPHA_TEST_THRESHOLD);
#endif

    // Debug view: lights per cluster (Renderer environment: LightCullingMode::ClusterHeatmap)
    if (lightCullingMode == LIGHT_CULLING_HEATMAP)
        return float4(LightHeatmap(input.position), 1.0);

    return float4(ToDisplay(ShadeSurface(surface, input, V)), surface.alpha);
}
