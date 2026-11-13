// Emissive.ps.hlsl: a glowing, stylised surface (lamps, markers, screens). Not lit by the scene's lights: the emission
// dominates, with a soft rim glow and a boosted exposure. The material is read by EvaluateSurface() like every other.

#include "common.hlsli"
#include "surface.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    const float3 V = normalize(input.viewDir);
    const Surface surface = EvaluateSurface(input, V);

    float3 finalEmissive = surface.emissive;
#if HAS_VERTEX_COLOR_ATTRIBUTE
    // vertex colours may modulate the emission strength (their alpha also drives the coverage, see EvaluateSurface)
    finalEmissive *= (1.0 + input.color.a);
#endif

    // ---------------- rim glow: the emissive colour brightens toward the silhouette ----------------
    float3 rim = float3(0.0, 0.0, 0.0);
#if HAS_NORMAL_ATTRIBUTE
    const float rimDot = 1.0 - saturate(dot(V, surface.normal));
    rim = finalEmissive * pow(rimDot, 3.0) * 0.3;
#endif

    // ---------------- emission vs. base colour: a strong emitter hides its own base colour ----------------
    const float emissiveStrength = dot(finalEmissive, float3(0.299, 0.587, 0.114));     // luminance
    float3 color;
    if (emissiveStrength > 0.1)
    {
        const float baseMix = saturate(1.0 - emissiveStrength * 0.5);
        color = lerp(finalEmissive, surface.baseColor * 0.2 + finalEmissive, baseMix);
    }
    else
    {
        color = surface.baseColor + finalEmissive;
    }
    color += rim;

    // a little metallic tint from the specular colour
    color = lerp(color, color * specularColor.rgb, saturate(metallic * 0.3));

    // emitters get a higher exposure so they stay bright next to lit surfaces
    color = ToneMapExposure(color, exposure * (1.0 + emissiveStrength * 0.5));
    color = ApplyGamma(color, gamma);

    const float finalAlpha = saturate(surface.alpha);
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - ALPHA_TEST_THRESHOLD);
#endif
    return float4(color, finalAlpha);
}
