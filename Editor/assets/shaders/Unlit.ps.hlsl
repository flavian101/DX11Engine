// Unlit.ps.hlsl: colour x texture (+ emission), no lights. Still linear light: the result is encoded for the display, so
// at the neutral gamma an unlit texture comes out exactly as it is in the file.

#include "common.hlsli"
#include "surface.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    const Surface surface = EvaluateSurface(input, normalize(input.viewDir));

#if ENABLE_ALPHA_TEST
    clip(surface.alpha - ALPHA_TEST_THRESHOLD);
#endif

    return float4(ApplyGamma(surface.baseColor + surface.emissive, gamma), surface.alpha);
}
