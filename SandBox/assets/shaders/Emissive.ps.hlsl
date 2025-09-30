#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // ========================================================================
    // TEXTURE SAMPLING
    // ========================================================================
    
    float4 diffuseSample = float4(1.0, 1.0, 1.0, 1.0);
    float3 emissiveSample = float3(0.0, 0.0, 0.0);
    
#if HAS_TEXCOORDS_ATTRIBUTE
    diffuseSample = SampleDiffuseTexture(input.texCoord);
    
#if HAS_EMISSIVE_MAP
        emissiveSample = SampleEmissiveMap(input.texCoord);
#endif
#endif
    
    // ========================================================================
    // BASE COLOR CALCULATION
    // ========================================================================
    
    float3 baseColor = diffuseColor.rgb * diffuseSample.rgb;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color.rgb;
#endif
    
    // ========================================================================
    // EMISSIVE CALCULATION
    // ========================================================================
    
    // Combine material emissive with texture
    float3 finalEmissive = emissiveColor.rgb;
    
#if HAS_EMISSIVE_MAP
    finalEmissive *= emissiveSample;
#endif
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    // Allow vertex colors to modulate emissive intensity
    emissiveBoost *= (1.0 + input.color.a);
#endif
    
    finalEmissive *= emissiveIntensity;
    
    // ========================================================================
    // RIM LIGHTING - Subtle glow effect
    // ========================================================================
    
    float3 rimContribution = float3(0.0, 0.0, 0.0);
    
#if HAS_NORMAL_ATTRIBUTE
    float3 worldNormal = normalize(input.normal);
    float3 viewDir = normalize(input.viewDir);
    
    // Fresnel-like rim effect
    float rimDot = 1.0 - saturate(dot(viewDir, worldNormal));
    float rimPower = 3.0; // Sharper falloff
    float rimIntensity = pow(rimDot, rimPower);
    
    // Use emissive color for rim to create coherent glow
    rimContribution = finalEmissive * rimIntensity * 0.3;
#endif
    
    // ========================================================================
    // FINAL COLOR COMBINATION
    // ========================================================================
    
    // Calculate emissive strength for blending
    float emissiveStrength = dot(finalEmissive, float3(0.299, 0.587, 0.114)); // Luminance
    
    // Blend between base color and emissive based on emissive strength
    float3 finalColor;
    
    if (emissiveStrength > 0.1)
    {
        // Strong emissive: emissive dominates
        float baseMix = saturate(1.0 - emissiveStrength * 0.5);
        finalColor = lerp(finalEmissive, baseColor * 0.2 + finalEmissive, baseMix);
    }
    else
    {
        // Weak emissive: base color dominates
        finalColor = baseColor + finalEmissive;
    }
    
    // Add rim glow
    finalColor += rimContribution;
    
    // Apply metallic tint (subtle)
    float metallicInfluence = saturate(metallic * 0.3);
    finalColor = lerp(finalColor, finalColor * specularColor.rgb, metallicInfluence);
    
    // ========================================================================
    // POST-PROCESSING
    // ========================================================================
    
    // Tone mapping with higher exposure for emissive materials
    float emissiveExposure = exposure * (1.0 + emissiveStrength * 0.5);
    finalColor = ToneMapExposure(finalColor, emissiveExposure);
    
    // Gamma correction
    finalColor = ApplyGamma(finalColor, gamma);
    
    // ========================================================================
    // ALPHA
    // ========================================================================
    
    float finalAlpha = diffuseSample.a * alpha;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a;
#endif
    
    finalAlpha = saturate(finalAlpha);
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5);
#endif
    
    return float4(finalColor, finalAlpha);
}