#include "common.hlsli"


float4 main(StandardVertexOutput input) : SV_Target
{
    // Normalize inputs
    float3 N = normalize(input.normal);
    float3 V = normalize(input.viewDir);
    
    // Sample textures
    float4 diffuseSample = SampleDiffuseTexture(input.texCoord);
    float3 normalSample = SampleNormalMap(input.texCoord);
    float3 specularSample = SampleSpecularMap(input.texCoord);
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    
    // Calculate world normal
    
    float3 worldNormal;
#if HAS_TANGENT_ATTRIBUTE
    worldNormal = CalculateWorldNormal(N, input.tangent, normalSample);
#else
    worldNormal = N;
#endif
    
    // Material properties
    float3 albedo = diffuseColor.rgb * diffuseSample.rgb;
    
    #if HAS_VERTEX_COLOR_ATTRIBUTE
    albedo *= input.color.rgb;
    // You can also use vertex color alpha
    // float vertexAlpha = input.color.a;
#endif
    
     // Handle metallic/roughness from textures
    float metallicValue = metallic;
    float roughnessValue = roughness;
    
#if HAS_SPECULAR_MAP
    // Use specular map channels for PBR values
    metallicValue *= specularSample.b; // Blue channel = metallic
    roughnessValue *= specularSample.g; // Green channel = roughness
#endif
    
    roughnessValue = max(roughnessValue, MIN_ROUGHNESS);
    
    // Calculate F0 (surface reflection at zero incidence)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallicValue);
    
    // Initialize color with ambient and emissive
    float3 color = ambientColor * ambientIntensity * albedo;
    color += emissiveColor.rgb * emissiveSample;
    
    // Calculate lighting contributions
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        color += CalculateDirectionalLight(directionalLights[i], worldNormal, V,
                                         albedo, metallicValue, roughnessValue, F0, input.worldPos);
    }
    
    for (uint i = 0; i < pointLightCount; ++i)
    {
        color += CalculatePointLight(pointLights[i], input.worldPos.xyz, worldNormal, V,
                                   albedo, metallicValue, roughnessValue, F0);
    }
    
    for (uint i = 0; i < spotLightCount; ++i)
    {
        color += CalculateSpotLight(spotLights[i], input.worldPos.xyz, worldNormal, V,
                                  albedo, metallicValue, roughnessValue, F0);
    }
    
    // Tone mapping and gamma correction
    color = ToneMapExposure(color, exposure);
    color = ApplyGamma(color, gamma);
    
    float finalAlpha = diffuseSample.a * alpha;
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalAlpha *= input.color.a; // Modulate with vertex alpha
#endif
    
    return float4(color, finalAlpha);
}