#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // ========================================================================
    // TEXTURE SAMPLING - Conditional based on available textures and UVs
    // ========================================================================
    
    float4 diffuseSample = float4(1.0, 1.0, 1.0, 1.0);
    float3 emissiveSample = float3(0.0, 0.0, 0.0);
    
#if HAS_TEXCOORDS_ATTRIBUTE
    // Only sample textures if we have texture coordinates
    diffuseSample = SampleDiffuseTexture(input.texCoord);
    
#if HAS_EMISSIVE_MAP
        emissiveSample = SampleEmissiveMap(input.texCoord);
#endif
#endif
    
    // ========================================================================
    // BASE COLOR CALCULATION
    // ========================================================================
    
    // Base color from material and diffuse texture
    float3 baseColor = diffuseColor.rgb * diffuseSample.rgb;
    
    // Modulate with vertex colors if available
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color.rgb;
    // Use vertex color alpha for emissive intensity modulation
    float emissiveBoost = 1.0 + input.color.a * 2.0; // Scale factor for more control
#else
    float emissiveBoost = 1.0;
#endif
    
    // ========================================================================
    // ENHANCED EMISSIVE EFFECTS
    // ========================================================================
    
    // Base emissive calculation
    float3 finalEmissive = emissiveColor.rgb;
    
    // Apply emissive texture if available
#if HAS_EMISSIVE_MAP
        finalEmissive *= emissiveSample;
#endif
    
    // Apply boost from vertex colors
    finalEmissive *= emissiveBoost;
    
    // Time-based animation using a more stable hash
    float3 worldPosHash = frac(input.worldPos.xyz * 0.1); // Better hash function
    float timeOffset = dot(worldPosHash, float3(0.3, 0.7, 0.2)); // Weighted sum
    float timeValue = input.time * 2.0 + timeOffset; // Scale time for visibility
    
    // Pulsing effect - use a defined bit flag or material property
    // Instead of hardcoded 0x100, use existing emissive enable flag
#if ENABLE_EMISSIVE || HAS_EMISSIVE_MAP
        float pulseFreq = max(0.5, shininess * 0.05); // Clamp minimum frequency
        float pulse = sin(timeValue * pulseFreq) * 0.5 + 0.5;
        finalEmissive *= (0.7 + pulse * 0.6); // More subtle pulsing
#endif
    
    // ========================================================================
    // RIM LIGHTING EFFECT - Handle missing normals gracefully
    // ========================================================================
    
    float3 viewDir = normalize(input.viewDir);
    float3 rimContribution = float3(0.0, 0.0, 0.0);
    
#if HAS_NORMAL_ATTRIBUTE
    float3 worldNormal = GetFinalWorldNormal(input);
    
    // Calculate rim lighting
    float rimDot = 1.0 - saturate(dot(viewDir, worldNormal));
    float rimPower = lerp(2.0, 6.0, saturate(roughness)); // Better range
    float rimEffect = pow(rimDot, rimPower);
    
    // Rim contribution should be additive, not multiplicative with metallic
    rimContribution = specularColor.rgb * rimEffect * 0.5; // Tone down rim effect
#endif
    
    // ========================================================================
    // FINAL COLOR COMBINATION
    // ========================================================================
    
    // For emissive materials, the base color should be less prominent
    float3 finalColor;
    
#if ENABLE_EMISSIVE || HAS_EMISSIVE_MAP
        // Emissive materials: emissive dominates, base color is subtle
        float emissiveStrength = max(max(finalEmissive.r, finalEmissive.g), finalEmissive.b);
        float baseLerp = saturate(1.0 - emissiveStrength); // Fade base color as emissive increases
        
        finalColor = lerp(finalEmissive, baseColor * 0.3 + finalEmissive, baseLerp);
        finalColor += rimContribution;
#else
        // Non-emissive materials: use base color with subtle emissive tint
    finalColor = baseColor + finalEmissive * 0.1 + rimContribution;
#endif
    
    // Apply metallic as an intensity multiplier more conservatively
    float intensityMult = 1.0 + metallic * 0.5; // Less aggressive multiplier
    finalColor *= intensityMult;
    
    // ========================================================================
    // TONE MAPPING AND GAMMA
    // ========================================================================
    
    // Apply exposure before tone mapping for better control
    finalColor = ToneMapExposure(finalColor, exposure);
    
    // Apply gamma correction
    finalColor = ApplyGamma(finalColor, gamma);
    
    // ========================================================================
    // ALPHA CALCULATION
    // ========================================================================
    
    float finalAlpha = diffuseSample.a * alpha;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a;
#endif
    
    // Ensure alpha is within valid range
    finalAlpha = saturate(finalAlpha);
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5);
#endif
    
    return float4(finalColor, finalAlpha);
}