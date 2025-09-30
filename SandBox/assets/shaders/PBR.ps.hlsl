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
    float occlusionValue = 1.0;
    float opacityValue = alpha;
    float3 emissiveValue = emissiveColor.rgb * emissiveIntensity;
    
    // Specular workflow properties
    float3 specularValue = specularColor.rgb;
    
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
        // If we have dedicated metallic/roughness maps, use specular as color
        // Otherwise, treat it as packed PBR data
#if HAS_METALLIC_MAP || HAS_ROUGHNESS_MAP
            // Specular workflow - use as specular color
            specularValue *= specularSample;
#else
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
    // Blend detail textures over base if available
#if HAS_DETAIL_TEXTURES
#if HAS_DETAIL_DIFFUSE_MAP
            float4 detailDiffuse = SampleDetailDiffuse(input.texCoord);
            // Blend based on detail alpha channel
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
    
    // Clamp roughness to avoid artifacts
    roughnessValue = clamp(roughnessValue, MIN_ROUGHNESS, 1.0);
    
    // Clamp metallic to valid range
    metallicValue = saturate(metallicValue);
    
    // Apply occlusion strength from material buffer
    occlusionValue = lerp(1.0, occlusionValue, occlusionStrength);
    
    // Calculate F0 (surface reflection at zero incidence)
    // For metallic workflow: interpolate between dielectric (0.04) and conductor (albedo)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, baseColor.rgb, metallicValue);
    
    // For specular workflow, we could also use:
    // F0 = specularValue; // Direct specular color control
    
    // ========================================================================
    // LIGHTING ACCUMULATION
    // ========================================================================
    
    // Start with ambient + occlusion
    float3 color = ambientColor * ambientIntensity * baseColor.rgb * occlusionValue;
    
    // ========== DIRECTIONAL LIGHTS ==========
    [loop]
    for (uint i = 0; i < directionalLightCount; ++i)
    {
        color += CalculateDirectionalLight(
            directionalLights[i],
            N,
            V,
            baseColor.rgb,
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
            baseColor.rgb,
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
            baseColor.rgb,
            metallicValue,
            roughnessValue,
            F0
        );
    }
    
    // ========================================================================
    // EMISSIVE CONTRIBUTION
    // Only add if enabled via flag or texture is present
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
    
    // Apply gamma correction
    color = ApplyGamma(color, gamma);
    
    // ========================================================================
    // FINAL ALPHA CALCULATION
    // ========================================================================
    
    float finalAlpha = baseColor.a * opacityValue;
    
    // ========================================================================
    // ALPHA TESTING
    // Discard transparent pixels if alpha testing is enabled
    // ========================================================================
    
#if ENABLE_ALPHA_TEST
    clip(finalAlpha - 0.5);
#endif
    
    return float4(color, finalAlpha);
}