#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    float3 V = normalize(input.viewDir);
    
    // ========== PARALLAX MAPPING PREPROCESSING ==========
#if ENABLE_PARALLAX_MAPPING && HAS_HEIGHT_MAP && HAS_TANGENT_ATTRIBUTE
    input.texCoord = ParallaxOcclusionMapping(input.texCoord, V, 
                                              input.normal, input.tangent);
#endif
    
    // ========================================================================
    // MATERIAL PROPERTY INITIALIZATION
    // Start with material buffer values, then modulate with textures if available
    // ========================================================================
    
    // Base color from material buffer
    float4 baseColor = diffuseColor;
    
    // PBR properties from material buffer
    float roughnessValue = roughness;
    float metallicValue = metallic;
    
    // Additional properties from material buffer
    float occlusionValue = occlusionStrength;
    float opacityValue = alpha;
    float3 emissiveValue = emissiveColor.rgb * emissiveIntensity;
    
    // ========================================================================
    // TEXTURE SAMPLING - Modulate material buffer with textures
    // Only sample textures if coordinates are available and textures exist
    // ========================================================================
    
#if HAS_TEXCOORDS_ATTRIBUTE
    
    // ========== DIFFUSE/ALBEDO TEXTURE ==========
#if HAS_DIFFUSE_TEXTURE
        float4 diffuseSample = SampleDiffuseTexture(input.texCoord);
        baseColor *= diffuseSample;
#endif
    
    // ========== SPECULAR TEXTURE ==========
    // Can contain specular color OR packed PBR data (R=AO, G=Roughness, B=Metallic)
#if HAS_SPECULAR_MAP
        float3 specularSample = SampleSpecularMap(input.texCoord);
        
        // Check if we're using specular workflow or metallic workflow
        // If we have dedicated metallic/roughness maps, use specular as packed data
#if !HAS_METALLIC_MAP && !HAS_ROUGHNESS_MAP
            // Packed PBR workflow - extract from channels
            occlusionValue *= specularSample.r;      // Red = AO
            roughnessValue *= specularSample.g;      // Green = Roughness
            metallicValue *= specularSample.b;       // Blue = Metallic
#endif
#endif
    
    // ========== DEDICATED PBR TEXTURES ==========
    // These override packed specular map values if present
    
#if HAS_ROUGHNESS_MAP
        roughnessValue *= SampleRoughnessMap(input.texCoord);
#endif
    
#if HAS_METALLIC_MAP
        metallicValue *= SampleMetallicMap(input.texCoord);
#endif
    
#if HAS_AO_MAP
        occlusionValue *= SampleAOMap(input.texCoord);
#endif
    
    // ========== EMISSIVE TEXTURE ==========
#if HAS_EMISSIVE_MAP
        float3 emissiveSample = SampleEmissiveMap(input.texCoord);
        emissiveValue *= emissiveSample;
#endif
    
    // ========== OPACITY TEXTURE ==========
#if HAS_OPACITY_MAP
        opacityValue *= SampleOpacityMap(input.texCoord);
#endif
    
    // ========== DETAIL TEXTURES ==========
#if HAS_DETAIL_TEXTURES
#if HAS_DETAIL_DIFFUSE_MAP
            float4 detailDiffuse = SampleDetailDiffuse(input.texCoord);
            baseColor.rgb = lerp(baseColor.rgb, baseColor.rgb * detailDiffuse.rgb, detailDiffuse.a * 0.5);
#endif
#endif
    
#endif // HAS_TEXCOORDS_ATTRIBUTE
    
    // ========================================================================
    // VERTEX COLOR MODULATION
    // ========================================================================
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor.rgb *= input.color.rgb;
    baseColor.a *= input.color.a;
#endif
    
    // ========================================================================
    // NORMAL CALCULATION
    // ========================================================================
    
#if HAS_NORMAL_ATTRIBUTE
    float3 N = normalize(input.normal);
#else
    float3 N = float3(0.0, 0.0, 1.0);
#endif

#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE && HAS_TEXCOORDS_ATTRIBUTE
    N = CalculateWorldNormal(N, input.tangent, input.texCoord);
#endif
    
    // ========================================================================
    // PBR MATERIAL FINALIZATION
    // ========================================================================
    
    // Clamp roughness to avoid artifacts (critical for PBR)
    roughnessValue = clamp(roughnessValue, MIN_ROUGHNESS, 1.0);
    
    // Clamp metallic to valid range
    metallicValue = saturate(metallicValue);
    
    // Apply occlusion strength from material buffer
    occlusionValue = lerp(1.0, occlusionValue, occlusionStrength);
    
    // Convert albedo to linear space if using sRGB textures
    // Most texture formats store colors in sRGB, but lighting calculations need linear
#if HAS_DIFFUSE_TEXTURE
    baseColor.rgb = pow(abs(baseColor.rgb), 2.2); // sRGB to Linear
#endif
    
    // Proper F0 calculation for metallic workflow
    // Dielectrics: ~0.04 (4% reflectance)
    // Metals: Use albedo color
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallicValue);
    
    //For metals, diffuse component should be black (energy conservation)
    float3 albedo = baseColor.rgb * (1.0 - metallicValue);
    
    // ========================================================================
    // LIGHTING ACCUMULATION
    // ========================================================================
    
    // Start with ambient + occlusion + emissive
    float3 color = ambientColor * ambientIntensity * baseColor.rgb * occlusionValue;
    
    // ========== DIRECTIONAL LIGHTS ==========
    [loop]
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        color += CalculateDirectionalLight(
            directionalLights[i],
            N,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0,
            input.worldPos
        );
    }
    
    // ========== POINT LIGHTS ==========
    [loop]
    for (uint j = 0; j < pointLightCount; ++j)
    {
        color += CalculatePointLight(
            pointLights[j],
            input.worldPos.xyz,
            N,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
    }
    
    // ========== SPOT LIGHTS ==========
    [loop]
    for (uint k = 0; k < spotLightCount; ++k)
    {
        color += CalculateSpotLight(
            spotLights[k],
            input.worldPos.xyz,
            N,
            V,
            albedo,
            metallicValue,
            roughnessValue,
            F0
        );
    }
    
    // ========================================================================
    // IMAGE-BASED LIGHTING (IBL)
    // ========================================================================
    
#if HAS_ENVIRONMENT_MAP
    // Sample environment map for specular reflection
    float3 R = reflect(-V, N);
    float3 prefilteredColor = environmentTexture.SampleLevel(standardSampler, R, roughnessValue * 8.0).rgb;
    
    // Fresnel for environment reflection
    float3 F_env = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughnessValue);
    
    // Energy-conserving specular IBL
    float3 kS_env = F_env;
    float3 kD_env = (1.0 - kS_env) * (1.0 - metallicValue);
    
    // Add diffuse IBL if we have irradiance map
#if HAS_IRRADIANCE_MAP
    float3 irradiance = irradianceTexture.Sample(standardSampler, N).rgb;
    float3 diffuseIBL = kD_env * irradiance * baseColor.rgb;
#else
    float3 diffuseIBL = float3(0.0, 0.0, 0.0);
#endif
    
    // Simplified BRDF integration (without LUT)
    float NdotV = max(dot(N, V), 0.0);
    float2 envBRDF = float2(1.0 - roughnessValue, 1.0) * NdotV;
    float3 specularIBL = prefilteredColor * (F_env * envBRDF.x + envBRDF.y);
    
    // Add IBL contribution with intensity control
    color += (diffuseIBL + specularIBL) * iblIntensity;
#endif
    
    // ========================================================================
    // EMISSIVE CONTRIBUTION
    // ========================================================================
    
#if ENABLE_EMISSIVE || HAS_EMISSIVE_MAP
    color += emissiveValue;
#endif
    
    // ========================================================================
    // FOG APPLICATION
    // ========================================================================
    
#if ENABLE_FOG
    float fogDistance = length(input.viewDir);
    float fogDensity = 0.001;
    float fogFactor = saturate(exp(-fogDistance * fogDensity));
    color = lerp(ambientColor, color, fogFactor);
#endif
    
    // ========================================================================
    // POST-PROCESSING
    // ========================================================================
    
    // Apply exposure tone mapping
    color = ToneMapExposure(color, exposure);
    
    // Apply gamma correction (Linear to sRGB)
    color = ApplyGamma(color, gamma);
    
    // ========================================================================
    // FINAL ALPHA CALCULATION
    // ========================================================================
    
    float finalAlpha = baseColor.a * opacityValue;
    
    // ========================================================================
    // ALPHA TESTING
    // ========================================================================
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5);
#endif
    
    return float4(color, finalAlpha);
}