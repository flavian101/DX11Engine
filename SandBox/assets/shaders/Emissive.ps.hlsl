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
    emissiveSample = SampleEmissiveMap(input.texCoord);
#endif
    
    // ========================================================================
    // BASE COLOR CALCULATION
    // ========================================================================
    
    // Base color
    float3 baseColor = diffuseColor.rgb * diffuseSample.rgb;
    
    // Modulate with vertex colors for variety
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color.rgb;
    // Use vertex color intensity for emissive boost
    float emissiveBoost = 1.0 + input.color.a;
#else
    float emissiveBoost = 1.0;
#endif
    
    // ========================================================================
    // ENHANCED EMISSIVE EFFECTS
    // ========================================================================
    
    // Enhanced emissive effects
    float3 finalEmissive = emissiveColor.rgb * emissiveSample * emissiveBoost;
    
    // Time-based animation using world position
    float timeValue = input.worldPos.x + input.worldPos.y + input.worldPos.z + input.time;
    
    // Pulsing effect controlled by shininess
    if (flags & 0x100) // Custom flag for pulse enable
    {
        float pulse = sin(timeValue * shininess * 0.1) * 0.5 + 0.5;
        finalEmissive *= (0.5 + pulse * 0.5);
    }
    
    // ========================================================================
    // RIM LIGHTING EFFECT - Handle missing normals/tangents gracefully
    // ========================================================================
    
    float3 viewDir = normalize(input.viewDir);
    float3 rimContribution = float3(0.0, 0.0, 0.0);
    
#if HAS_NORMAL_ATTRIBUTE
    float3 normal = normalize(input.normal);
    
#if HAS_TANGENT_ATTRIBUTE && HAS_TEXCOORDS_ATTRIBUTE
    // Enhanced rim with tangent space if available
    float3 normalSample = SampleNormalMap(input.texCoord);
    float3 worldNormal = CalculateWorldNormal(normal, input.tangent, normalSample);
    float rimDot = 1.0 - saturate(dot(viewDir, worldNormal));
#else
    // Simple rim with vertex normal
    float rimDot = 1.0 - saturate(dot(viewDir, normal));
#endif
    
    float rimPower = lerp(1.0, 8.0, roughness);
    float rimEffect = pow(rimDot, rimPower);
    rimContribution = specularColor.rgb * rimEffect * metallic;
#endif
    
    // ========================================================================
    // COMBINE EFFECTS
    // ========================================================================
    
    // Combine effects
    float3 finalColor = baseColor * finalEmissive + rimContribution;
    
    // Intensity boost using metallic value
    finalColor *= (1.0 + metallic);
    
    // Simple tone mapping for emissive materials
    finalColor = ToneMapReinhard(finalColor);
    
    // ========================================================================
    // ALPHA CALCULATION
    // ========================================================================
    
    float finalAlpha = diffuseSample.a * alpha;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a;
#endif
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5);
#endif
    
    return float4(finalColor, finalAlpha);
}