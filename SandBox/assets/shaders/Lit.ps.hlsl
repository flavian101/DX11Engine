#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
   float3 V = normalize(input.viewDir);

#if ENABLE_PARALLAX_MAPPING && HAS_HEIGHT_MAP && HAS_TANGENT_ATTRIBUTE
    input.texCoord = ParallaxOcclusionMapping(input.texCoord, V, 
                                              input.normal, input.tangent);
#endif
    
    // Normalize inputs that are always present
#if HAS_NORMAL_ATTRIBUTE
    float3 N = normalize(input.normal);
#else
    float3 N = float3(0.0, 0.0, 1.0); // Default up normal
#endif

    
    // ========================================================================
    // TEXTURE SAMPLING - Conditional based on available textures
    // ========================================================================
    
    float4 diffuseSample = float4(1.0, 1.0, 1.0, 1.0);
    float3 specularSample = float3(1.0, 1.0, 1.0);
    float3 emissiveSample = float3(0.0, 0.0, 0.0);
    float roughnessValue = roughness;
    float metallicValue = metallic;
    float occlusionValue = 1.0;
    float opacityValue = alpha;
    
#if HAS_TEXCOORDS_ATTRIBUTE
    // Only sample textures if we have texture coordinates
    diffuseSample = SampleDiffuseTexture(input.texCoord);
    specularSample = SampleSpecularMap(input.texCoord);
    emissiveSample = SampleEmissiveMap(input.texCoord);
    
    roughnessValue = SampleRoughnessMap(input.texCoord);
    metallicValue *= SampleMetallicMap(input.texCoord);
    occlusionValue *= SampleAOMap(input.texCoord);
    opacityValue *= SampleOpacityMap(input.texCoord);
#endif
    
    // ========================================================================
    // NORMAL MAPPING - Conditional based on available data
    // ========================================================================
    
    float3 worldNormal;
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE && HAS_TEXCOORDS_ATTRIBUTE
    worldNormal = CalculateWorldNormal(N, input.tangent, input.texCoord);
#elif HAS_NORMAL_ATTRIBUTE
    worldNormal = N;
#else
    worldNormal = float3(0.0, 0.0, 1.0);
#endif   
    // ========================================================================
    // MATERIAL PROPERTIES - Enhanced with texture data
    // ========================================================================
    
    // Base color with vertex color modulation if available
    float3 albedo = diffuseColor.rgb * diffuseSample.rgb;
    
#if HAS_DETAIL_TEXTURES && HAS_TEXCOORDS_ATTRIBUTE
    // Apply detail diffuse texture
    float4 detailDiffuse = SampleDetailDiffuse(input.texCoord);
    albedo = lerp(albedo, albedo * detailDiffuse.rgb, detailDiffuse.a * 0.5);
#endif
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    albedo *= input.color.rgb;
#endif
    
 
#if HAS_SPECULAR_MAP && HAS_TEXCOORDS_ATTRIBUTE
    // Use specular map channels for PBR values if available
    metallicValue *= specularSample.b; // Blue channel = metallic
    roughnessValue *= specularSample.g; // Green channel = roughness
    
    // Can also use red channel for occlusion if needed
    // float occlusion = specularSample.r;
#endif
    
    roughnessValue = max(roughnessValue, MIN_ROUGHNESS);
    
    // Calculate F0 (surface reflection at zero incidence)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallicValue);
    
    // ========================================================================
    // LIGHTING ACCUMULATION
    // ========================================================================
    
    // Initialize color with ambient contribution
    float3 color = ambientColor * ambientIntensity * albedo;
    
    // Add emissive contribution if available
#if (HAS_EMISSIVE_MAP && HAS_TEXCOORDS_ATTRIBUTE) || ENABLE_EMISSIVE
    color += emissiveColor.rgb * emissiveSample;
#endif
    
    // ========================================================================
    // DIRECTIONAL LIGHTS
    // ========================================================================
    
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        color += CalculateDirectionalLight(
            directionalLights[i],
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0,
            input.worldPos
        );
    }
    
    // ========================================================================
    // POINT LIGHTS
    // ========================================================================
    
    for (uint i = 0; i < pointLightCount; ++i)
    {
        color += CalculatePointLight(
            pointLights[i],
            input.worldPos.xyz,
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
    }
    
    // ========================================================================
    // SPOT LIGHTS
    // ========================================================================
    
    for (uint i = 0; i < spotLightCount; ++i)
    {
        color += CalculateSpotLight(
            spotLights[i],
            input.worldPos.xyz,
            worldNormal,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
    }
    
    // ========================================================================
    // FOG - Optional feature
    // ========================================================================
    
#if ENABLE_FOG
    float fogDistance = length(input.viewDir);
    float fogFactor = saturate(exp(-fogDistance * 0.001)); // Simple exponential fog
    color = lerp(ambientColor, color, fogFactor);
#endif
    
    // ========================================================================
    // POST-PROCESSING
    // ========================================================================
    
    // Tone mapping and gamma correction
    color = ToneMapExposure(color, exposure);
    color = ApplyGamma(color, gamma);
    
    // ========================================================================
    // ALPHA CALCULATION
    // ========================================================================
    
    float finalAlpha = diffuseSample.a * alpha * opacityValue;
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    // Modulate alpha with vertex color if available
    finalAlpha *= input.color.a;
#endif
    
    // ========================================================================
    // ALPHA TEST - Optional feature
    // ========================================================================
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5); // Discard pixels below threshold
#endif
    
    return float4(color, finalAlpha);
}
