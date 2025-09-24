#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // Normalize inputs
    float3 N = normalize(input.normal);
    float3 V = normalize(input.viewDir);
    
    // Sample textures
    float4 diffuseSample = SampleDiffuseTexture(input.texCoord);
    float3 normalSample = SampleNormalMap(input.texCoord);
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    
   // Handle normal mapping for transparency
    float3 worldNormal;
#if HAS_TANGENT_ATTRIBUTE
    worldNormal = CalculateWorldNormal(N, input.tangent, normalSample);
#else
    worldNormal = N;
#endif
    
    // Base color with vertex color modulation
    float3 albedo = diffuseColor.rgb * diffuseSample.rgb;
#if HAS_VERTEX_COLOR_ATTRIBUTE
    albedo *= input.color.rgb;
#endif
    
    float metallicValue = metallic * 0.5; // Reduce metallic for transparency
    float roughnessValue = max(roughness, MIN_ROUGHNESS);
    
    // Calculate F0
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallicValue);
    
    // Initialize color with reduced ambient and emissive
    float3 color = ambientColor * ambientIntensity * albedo * 0.5;
    color += emissiveColor.rgb * emissiveSample;
    
    // Calculate lighting contributions (reduced intensity for transparency)
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        float3 lightContrib = CalculateDirectionalLight(directionalLights[i], worldNormal, V,
                                                      albedo, metallicValue, roughnessValue, F0, input.worldPos);
        color += lightContrib * 0.8; // Reduce lighting intensity
    }
    
    for (uint i = 0; i < pointLightCount; ++i)
    {
        float3 lightContrib = CalculatePointLight(pointLights[i], input.worldPos.xyz, worldNormal, V,
                                                albedo, metallicValue, roughnessValue, F0);
        color += lightContrib * 0.8;
    }
    
    for (uint i = 0; i < spotLightCount; ++i)
    {
        float3 lightContrib = CalculateSpotLight(spotLights[i], input.worldPos.xyz, worldNormal, V,
                                               albedo, metallicValue, roughnessValue, F0);
        color += lightContrib * 0.8;
    }
    
    // Light tone mapping for transparent materials
    color = ToneMapExposure(color, exposure * 0.8);
    color = ApplyGamma(color, gamma);
    
    // Handle transparency alpha
    float finalAlpha = diffuseSample.a * alpha;
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a;
#endif
    
    return float4(color, finalAlpha);
}