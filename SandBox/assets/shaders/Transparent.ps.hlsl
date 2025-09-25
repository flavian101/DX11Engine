#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // Normalize inputs
    float3 N = normalize(input.normal);
    float3 V = normalize(input.viewDir);
    
    // ========================================================================
    // TEXTURE SAMPLING
    // ========================================================================
    
    float4 diffuseSample = SampleDiffuseTexture(input.texCoord);
    float3 normalSample = SampleNormalMap(input.texCoord);
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    
    // ========================================================================
    // NORMAL CALCULATION - Conditional based on available data
    // ========================================================================
    
    float3 worldNormal = GetWorldNormal(input,normalSample);
    
    // ========================================================================
    // MATERIAL PROPERTIES - Adapted for transparency
    // ========================================================================
    
    // Base color with vertex color modulation
    float3 albedo = diffuseColor.rgb * diffuseSample.rgb;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    albedo *= input.color.rgb;
#endif
    
    // Reduce metallic contribution for transparency (more realistic)
    float metallicValue = metallic * 0.5;
    float roughnessValue = max(roughness, MIN_ROUGHNESS);
    
#if HAS_SPECULAR_MAP
    float3 specularSample = SampleSpecularMap(input.texCoord);
    metallicValue *= specularSample.b;
    roughnessValue *= specularSample.g;
#endif
    
    // Calculate F0
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallicValue);
    
    // ========================================================================
    // LIGHTING ACCUMULATION - Reduced intensity for transparency
    // ========================================================================
    
    // Reduced ambient for transparent materials
    float3 color = ambientColor * ambientIntensity * albedo * 0.6;
    
    // Add emissive contribution
#if HAS_EMISSIVE_MAP || ENABLE_EMISSIVE
    color += emissiveColor.rgb * emissiveSample;
#endif
    
    // Directional lights with reduced intensity
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        float3 lightContrib = CalculateDirectionalLight(
            directionalLights[i],
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0,
            input.worldPos
        );
        color += lightContrib * 0.8; // Reduce lighting intensity for transparency
    }
    
    // Point lights
    for (uint i = 0; i < pointLightCount; ++i)
    {
        float3 lightContrib = CalculatePointLight(
            pointLights[i],
            input.worldPos.xyz,
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
        color += lightContrib * 0.8;
    }
    
    // Spot lights
    for (uint i = 0; i < spotLightCount; ++i)
    {
        float3 lightContrib = CalculateSpotLight(
            spotLights[i],
            input.worldPos.xyz,
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
        color += lightContrib * 0.8;
    }
    
    // ========================================================================
    // TRANSPARENCY-SPECIFIC EFFECTS
    // ========================================================================
    
    // Enhanced rim lighting for glass-like materials
    float rimDot = 1.0 - saturate(dot(V, worldNormal));
    float rimPower = lerp(1.0, 4.0, roughnessValue);
    float rimEffect = pow(rimDot, rimPower);
    color += specularColor.rgb * rimEffect * 0.3; // Subtle rim lighting
    
    // ========================================================================
    // FOG - More pronounced for transparent objects
    // ========================================================================
    
#if ENABLE_FOG
    float fogDistance = length(input.viewDir);
    float fogFactor = saturate(exp(-fogDistance * 0.002)); // More aggressive fog
    color = lerp(ambientColor * 0.5, color, fogFactor);
#endif
    
    // ========================================================================
    // POST-PROCESSING - Lighter tone mapping
    // ========================================================================
    
    color = ToneMapExposure(color, exposure * 0.8);
    color = ApplyGamma(color, gamma);
    
    // ========================================================================
    // ALPHA CALCULATION - Multiple sources
    // ========================================================================
    
    float finalAlpha = diffuseSample.a * alpha;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a;
#endif
    
    // Distance-based alpha falloff for more realistic transparency
    float distance = length(input.viewDir);
    float distanceAlpha = saturate(1.0 - (distance * 0.001));
    finalAlpha *= distanceAlpha;
    
    // ========================================================================
    // ALPHA TEST - Usually disabled for transparency, but available
    // ========================================================================
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.1); // Lower threshold for transparency
#endif
    
    return float4(color, finalAlpha);
}