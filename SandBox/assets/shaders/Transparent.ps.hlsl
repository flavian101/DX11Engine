#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // Normalize inputs
#if HAS_NORMAL_ATTRIBUTE
    float3 N = normalize(input.normal);
#else
    float3 N = float3(0.0, 0.0, 1.0); // Safe default
#endif
    float3 V = normalize(input.viewDir);
    
    // ========================================================================
    // TEXTURE SAMPLING
    // ========================================================================
    
    float4 diffuseSample = float4(1.0, 1.0, 1.0, 1.0);
    float3 normalSample = float3(0.0, 0.0, 1.0);
    float3 emissiveSample = float3(0.0, 0.0, 0.0);
    float3 specularSample = float3(1.0, 1.0, 1.0);

#if HAS_TEXCOORDS_ATTRIBUTE
    // Only sample textures if we have texture coordinates
    diffuseSample = SampleDiffuseTexture(input.texCoord);
    normalSample = SampleNormalMap(input.texCoord);
    emissiveSample = SampleEmissiveMap(input.texCoord);
    specularSample = SampleSpecularMap(input.texCoord);
#endif
    
    // ========================================================================
    // NORMAL CALCULATION - Conditional based on available data
    // ========================================================================
    
    float3 worldNormal;
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE && HAS_TEXCOORDS_ATTRIBUTE
    // Full tangent space normal mapping
    worldNormal = CalculateWorldNormal(N, input.tangent, normalSample);
#elif HAS_NORMAL_MAP && HAS_TEXCOORDS_ATTRIBUTE && HAS_NORMAL_ATTRIBUTE
    // Fallback: simple normal perturbation without tangent space
    worldNormal = normalize(N + normalSample * 0.1);
#else
    // Use vertex normal or default
    worldNormal = N;
#endif    
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
    
#if HAS_SPECULAR_MAP && HAS_TEXCOORDS_ATTRIBUTE
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
#if (HAS_EMISSIVE_MAP && HAS_TEXCOORDS_ATTRIBUTE) || ENABLE_EMISSIVE
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