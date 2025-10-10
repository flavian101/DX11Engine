#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#ifndef HAS_DIFFUSE_TEXTURE
#define HAS_DIFFUSE_TEXTURE 0
#endif

#ifndef HAS_NORMAL_MAP
#define HAS_NORMAL_MAP 0
#endif

#ifndef HAS_SPECULAR_MAP
#define HAS_SPECULAR_MAP 0
#endif

#ifndef HAS_EMISSIVE_MAP
#define HAS_EMISSIVE_MAP 0
#endif

#ifndef HAS_ENVIRONMENT_MAP
#define HAS_ENVIRONMENT_MAP 0
#endif

#ifndef HAS_ROUGHNESS_MAP
#define HAS_ROUGHNESS_MAP 0
#endif

#ifndef HAS_METALLIC_MAP
#define HAS_METALLIC_MAP 0
#endif

#ifndef HAS_AO_MAP
#define HAS_AO_MAP 0
#endif

#ifndef HAS_HEIGHT_MAP
#define HAS_HEIGHT_MAP 0
#endif

#ifndef HAS_OPACITY_MAP
#define HAS_OPACITY_MAP 0
#endif

#ifndef HAS_DETAIL_DIFFUSE_MAP
#define HAS_DETAIL_DIFFUSE_MAP 0
#endif

#ifndef HAS_DETAIL_NORMAL_MAP
#define HAS_DETAIL_NORMAL_MAP 0
#endif

#ifndef HAS_DETAIL_TEXTURES
#define HAS_DETAIL_TEXTURES 0
#endif

// ========== ADD THIS NEW RENDERING FEATURE ==========
#ifndef ENABLE_PARALLAX_MAPPING
#define ENABLE_PARALLAX_MAPPING 0
#endif

// Vertex attribute features (set by vertex layout analysis)
#ifndef HAS_NORMAL_ATTRIBUTE
#define HAS_NORMAL_ATTRIBUTE 0 
#endif

#ifndef HAS_TEXCOORDS_ATTRIBUTE
#define HAS_TEXCOORDS_ATTRIBUTE 0 
#endif

#ifndef HAS_TANGENT_ATTRIBUTE
#define HAS_TANGENT_ATTRIBUTE 0
#endif

#ifndef HAS_VERTEX_COLOR_ATTRIBUTE
#define HAS_VERTEX_COLOR_ATTRIBUTE 0
#endif

#ifndef HAS_SECOND_UV_ATTRIBUTE
#define HAS_SECOND_UV_ATTRIBUTE 0
#endif

#ifndef HAS_SKINNING_ATTRIBUTES
#define HAS_SKINNING_ATTRIBUTES 0
#endif

// Rendering features (set by material type and flags)
#ifndef ENABLE_SHADOWS
#define ENABLE_SHADOWS 0
#endif

#ifndef ENABLE_FOG
#define ENABLE_FOG 0
#endif

#ifndef ENABLE_INSTANCING
#define ENABLE_INSTANCING 0
#endif

#ifndef ENABLE_ALPHA_TEST
#define ENABLE_ALPHA_TEST 0
#endif

#ifndef ENABLE_EMISSIVE
#define ENABLE_EMISSIVE 0
#endif

// Custom vertex input override
#ifndef CUSTOM_VERTEX_INPUT
#define CUSTOM_VERTEX_INPUT 0
#endif

//Common constants
static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;
static const float MIN_ROUGHNESS = 0.04;
static const float EPSILON = 0.0001;

// === MATERIAL FLAGS ===
#define HAS_DIFFUSE_TEXTURE_FLAG   0x01
#define HAS_NORMAL_MAP_FLAG        0x02
#define HAS_SPECULAR_MAP_FLAG      0x04
#define HAS_EMISSIVE_MAP_FLAG      0x08
#define CASTS_SHADOWS_FLAG         0x40
#define RECEIVES_SHADOWS_FLAG      0x80

cbuffer TransformBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
    float4x4 View;
    float4x4 Projection;
    float3 CameraPosition;
    float Time;
};

cbuffer cb_BoneMatrices : register(b1)
{
    float4x4 BoneMatrices[128];
};

cbuffer MaterialBufferData : register(b2)
{
    // ========== BASE MATERIAL PROPERTIES ==========
    float4 diffuseColor;
    float4 specularColor;
    float4 emissiveColor;
    
    // ========== PBR PROPERTIES ==========
    float shininess;
    float metallic;
    float roughness;
    float alpha;
    
    // ========== NEW: ENHANCED MATERIAL PROPERTIES ==========
    float normalScale;       //Normal map intensity
    float heightScale;       //Parallax/height scale
    float occlusionStrength; //AO intensity
    float emissiveIntensity; //Emissive multiplier
    
    // ========== TEXTURE TILING AND OFFSET ==========
    float2 textureScale;
    float2 textureOffset;
    float2 detailScale;      //Detail texture scale
    float2 detailOffset;     //Detail texture offset
    
    // ========== FLAGS ==========
    uint flags;
    float3 padding;
};

// Enhanced Scene Lighting buffer
cbuffer SceneLightData : register(b3)
{
    // Light counts
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint lightPadding;
    
    // Global parameters
    float3 ambientColor;
    float ambientIntensity;
    float iblIntensity;
    float exposure;
    float gamma;
    float globalPadding;
    
    // Directional lights (max 4)
    struct DirectionalLightGPU
    {
        float3 direction;
        float intensity;
        float3 color;
        float shadowMapIndex;
        float4 cascadeSplits;
        float4x4 shadowMatrices[4];
    } directionalLights[4];
    
    // Point lights (max 64)
    struct PointLightGPU
    {
        float3 position;
        float radius;
        float3 color;
        float intensity;
        float3 attenuation;
        float shadowMapIndex;
    } pointLights[64];
    
    // Spot lights (max 32)
    struct SpotLightGPU
    {
        float3 position;
        float range;
        float3 direction;
        float innerCone;
        float3 color;
        float outerCone;
        float intensity;
        float3 attenuation;
        float shadowMapIndex;
        float4x4 shadowMatrix;
    } spotLights[32];
};

cbuffer UIBuffer : register(b4)
{
    float4x4 uiProjection;
    float screenWidth;
    float screenHeight;
    float uiTime;
    float uiPadding;
};

//vertex shader Input and outputs

struct StandardVertexInput
{
    float3 position : POSITION;
#if HAS_NORMAL_ATTRIBUTE
    float3 normal : NORMAL;
#endif
    
#if HAS_TEXCOORDS_ATTRIBUTE
    float2 texCoord : TEXCOORD0;
#endif
    
#if HAS_TANGENT_ATTRIBUTE
    float4 tangent : TANGENT;
#endif

#if HAS_VERTEX_COLOR_ATTRIBUTE
    float4 color : COLOR0;
#endif

#if HAS_SECOND_UV_ATTRIBUTE
    float2 texCoord1 : TEXCOORD1;
#endif

#if HAS_SKINNING_ATTRIBUTES
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
#endif
};

struct StandardVertexOutput
{
    float4 position : SV_POSITION;
    float4 worldPos : POSITION;
#if HAS_NORMAL_ATTRIBUTE
    float3 normal : NORMAL;
#endif
#if HAS_TANGENT_ATTRIBUTE
    float4 tangent : TANGENT;
#endif
#if HAS_TEXCOORDS_ATTRIBUTE
    float2 texCoord : TEXCOORD0;
#endif
#if HAS_SECOND_UV_ATTRIBUTE
    float2 texCoord1 : TEXCOORD1;  // ADD THIS
#endif
#if HAS_VERTEX_COLOR_ATTRIBUTE
    float4 color : COLOR;
#endif
    float3 viewDir : VIEWDIR;
    float time : TIME;
};

//(Unlit, basic Lit,...)
struct BasicVertexInput
{
    float3 position : POSITION;
#if HAS_NORMAL_ATTRIBUTE
    float3 normal : NORMAL;
#endif
#if HAS_TEXCOORDS_ATTRIBUTE
    float2 texCoord : TEXCOORD;
#endif
};

struct UIVertexInput
{
    float3 position : POSITION;
#if HAS_TEXCOORDS_ATTRIBUTE
    float2 texCoord : TEXCOORD;
#endif
#if HAS_VERTEX_COLOR_ATTRIBUTE
    float4 color : COLOR;
#endif
#if HAS_NORMAL_ATTRIBUTE
    float3 normal : NORMAL;
#endif
};

struct UIVertexOutput
{
    float4 position : SV_POSITION;
#if HAS_TEXCOORDS_ATTRIBUTE
    float2 texCoord : TEXCOORD;
#endif
#if HAS_VERTEX_COLOR_ATTRIBUTE
    float4 color : COLOR;
#endif
};

// === TEXTURE BINDINGS ===
// ========== CORE TEXTURE BINDINGS ==========
Texture2D diffuseTexture : register(t0); // TextureSlot::Diffuse
Texture2D normalTexture : register(t1); // TextureSlot::Normal
Texture2D specularTexture : register(t2); // TextureSlot::Specular
Texture2D emissiveTexture : register(t3); // TextureSlot::Emissive

// ========== PBR TEXTURE BINDINGS ==========
Texture2D roughnessTexture : register(t4); // TextureSlot::Roughness
Texture2D metallicTexture : register(t5); // TextureSlot::Metallic
Texture2D aoTexture : register(t6); // TextureSlot::AmbientOcclusion
Texture2D heightTexture : register(t7); // TextureSlot::Height

// ========== ADDITIONAL TEXTURES ==========
Texture2D opacityTexture : register(t8); // TextureSlot::Opacity
Texture2D detailDiffuseTexture : register(t9); // TextureSlot::DetailDiffuse
Texture2D detailNormalTexture : register(t10); // TextureSlot::DetailNormal

// ========== ENVIRONMENT MAPPING ==========
TextureCube environmentTexture : register(t11); // TextureSlot::Environment
TextureCube irradianceTexture : register(t12); // TextureSlot::Irradiance
Texture2D brdfLUTTexture : register(t13); // TextureSlot::BRDF_LUT

// ========== SHADOWS ==========
#if ENABLE_SHADOWS
Texture2DArray shadowMaps       : register(t14); // TextureSlot::Shadow
SamplerComparisonState shadowSampler : register(s1);
#endif

// ========== SAMPLERS ==========
SamplerState standardSampler : register(s0);

// Utility Functions 
float3 GetScaledTexCoord(float2 uv)
{
    return float3(uv * textureScale * textureOffset, 0.0);
}

float4 SampleDiffuseTexture(float2 uv)
{
#if HAS_DIFFUSE_TEXTURE
    float2 scaledUV = uv * textureScale + textureOffset;
    return diffuseTexture.Sample(standardSampler, scaledUV);
#else
    return float4(1.0, 1.0, 1.0, 1.0);
#endif
}

// Add validation in SampleNormalMap
float3 SampleNormalMap(float2 uv)
{
#if HAS_NORMAL_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    float4 normalSample = normalTexture.Sample(standardSampler, scaledUV);
    
    // Convert from [0,1] to [-1,1] range
    float3 normal = normalSample.rgb * 2.0 - 1.0;
    
    // Apply normal intensity/scale
    normal.xy *= normalScale;
    
    // Renormalize after scaling
    normal = normalize(normal);
    
    // Safety check: if normal is degenerate, return flat normal
    if (any(isnan(normal)) || any(isinf(normal)) || length(normal) < EPSILON)
    {
        return float3(0.0, 0.0, 1.0);
    }
    
    return normal;
#else
    return float3(0.0, 0.0, 1.0);
#endif
}

float3 SampleSpecularMap(float2 uv)
{
#if HAS_SPECULAR_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return specularTexture.Sample(standardSampler, scaledUV).rgb;
#else
    return float3(1.0, 1.0, 1.0);
#endif
}

float3 SampleEmissiveMap(float2 uv)
{
#if HAS_EMISSIVE_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return emissiveTexture.Sample(standardSampler, scaledUV).rgb;
#else
    return float3(0.0, 0.0, 0.0);
#endif
}

float SampleRoughnessMap(float2 uv)
{
#if HAS_ROUGHNESS_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return roughnessTexture.Sample(standardSampler, scaledUV).r;
#else
    return roughness;
#endif
}

float SampleMetallicMap(float2 uv)
{
#if HAS_METALLIC_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return metallicTexture.Sample(standardSampler, scaledUV).r;
#else
    return metallic;
#endif
}

float SampleAOMap(float2 uv)
{
#if HAS_AO_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return aoTexture.Sample(standardSampler, scaledUV).r;
#else
    return 1.0; // No occlusion by default
#endif
}

float SampleHeightMap(float2 uv)
{
#if HAS_HEIGHT_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return heightTexture.Sample(standardSampler, scaledUV).r;
#else
    return 0.0; // Flat surface by default
#endif
}

float SampleOpacityMap(float2 uv)
{
#if HAS_OPACITY_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    return opacityTexture.Sample(standardSampler, scaledUV).r;
#else
    return 1.0; // Fully opaque by default
#endif
}

// ========== NEW: DETAIL TEXTURE SAMPLING ==========

float4 SampleDetailDiffuse(float2 uv)
{
#if HAS_DETAIL_DIFFUSE_MAP
    float2 scaledUV = uv * detailScale + detailOffset;
    return detailDiffuseTexture.Sample(standardSampler, scaledUV);
#else
    return float4(1.0, 1.0, 1.0, 1.0);
#endif
}

float3 SampleDetailNormal(float2 uv)
{
#if HAS_DETAIL_NORMAL_MAP
    float2 scaledUV = uv * detailScale + detailOffset;
    float3 normal = detailNormalTexture.Sample(standardSampler, scaledUV).rgb;
    return normalize(normal * 2.0 - 1.0);
#else
    return float3(0.0, 0.0, 1.0); // Flat normal
#endif
}

//Normal Mappings
float3 BlendNormals(float3 normal1, float3 normal2)
{
    // Reoriented Normal Mapping (RNM) blending
    float3 t = normal1 * float3(2, 2, 2) + float3(-1, -1, 0);
    float3 u = normal2 * float3(-2, -2, 2) + float3(1, 1, -1);
    float3 r = t * dot(t, u) - u * t.z;
    return normalize(r);
}

float3 CalculateWorldNormal(float3 vertexNormal, float4 tangent, float2 texCoord)
{
#if HAS_NORMAL_ATTRIBUTE
    float3 N = normalize(vertexNormal);
#else
    float3 N = float3(0.0, 0.0, 1.0);
#endif
    
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE
    // Sample base normal map (in tangent space)
    float3 tangentNormal = SampleNormalMap(texCoord);
    
    // Blend with detail normal if available
#if HAS_DETAIL_NORMAL_MAP && HAS_DETAIL_TEXTURES
    float3 detailNormal = SampleDetailNormal(texCoord);
    tangentNormal = BlendNormals(tangentNormal, detailNormal);
#endif
    
    // Build TBN matrix - ensure all vectors are normalized
    float3 T = normalize(tangent.xyz);
    float3 N_normalized = normalize(N);
    
    // Gram-Schmidt process to ensure T is perpendicular to N
    T = normalize(T - dot(T, N_normalized) * N_normalized);
    
    // Calculate bitangent with correct handedness
    float3 B = cross(N_normalized, T) * tangent.w;
    B = normalize(B);
    
    // Construct TBN matrix (tangent space to world space)
    float3x3 TBN = float3x3(T, B, N_normalized);
    
    // Transform normal from tangent space to world space
    float3 worldNormal = mul(tangentNormal, TBN);
    
    // Final normalization
    return normalize(worldNormal);
#else
    return N;
#endif
}

// ========== NEW: PARALLAX OCCLUSION MAPPING ==========

float2 ParallaxOcclusionMapping(float2 texCoords, float3 viewDir,
                               float3 normal, float4 tangent)
{
#if ENABLE_PARALLAX_MAPPING && HAS_HEIGHT_MAP && HAS_TANGENT_ATTRIBUTE
    // Build TBN matrix (same as normal mapping)
    float3 N = normalize(normal);
    float3 T = normalize(tangent.xyz);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(N, T) * tangent.w);
    
    float3x3 TBN = float3x3(T, B, N);
    
    // Transform view direction to tangent space
    // Use transpose to go from world to tangent space
    float3x3 TBN_inv = transpose(TBN);
    float3 viewDirTS = normalize(mul(TBN_inv, viewDir));
    
    // Early exit if viewing from behind surface
    if (viewDirTS.z <= 0.0)
        return texCoords;
    
    // Number of depth layers (adaptive based on view angle)
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = lerp(maxLayers, minLayers, abs(viewDirTS.z));
    
    // Calculate layer depth
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    // Calculate parallax offset direction and amount
    float2 P = viewDirTS.xy * heightScale;
    float2 deltaTexCoords = P / numLayers;
    
    // Initial values
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = SampleHeightMap(currentTexCoords);
    
    // Steep parallax mapping - ray marching through height field
    [loop]
    while(currentLayerDepth < currentDepthMapValue && currentLayerDepth < 1.0)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = SampleHeightMap(currentTexCoords);
        currentLayerDepth += layerDepth;
    }
    
    // Parallax occlusion mapping - interpolation for smooth result
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = SampleHeightMap(prevTexCoords) - (currentLayerDepth - layerDepth);
    
    // Weighted interpolation
    float weight = afterDepth / (afterDepth - beforeDepth + EPSILON);
    float2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);
    
    // Clamp to prevent sampling outside valid range
    finalTexCoords = clamp(finalTexCoords, 0.0, 1.0);
    
    return finalTexCoords;
#else
    return texCoords;
#endif
}


///PBR BRDF functions
//Improved GGX Distribution (Trowbridge-Reitz)
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, EPSILON);
}


float GeometrySchlickGGX(float NdotV, float roughness)
{
    // Direct lighting uses different k than IBL
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / max(denom, EPSILON);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float oneMinusCosTheta = saturate(1.0 - cosTheta);
    float oneMinusCosThetaPow5 = oneMinusCosTheta * oneMinusCosTheta;
    oneMinusCosThetaPow5 *= oneMinusCosThetaPow5;
    oneMinusCosThetaPow5 *= oneMinusCosTheta;
    
    return F0 + (1.0 - F0) * oneMinusCosThetaPow5;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float oneMinusCosTheta = saturate(1.0 - cosTheta);
    float oneMinusCosThetaPow5 = oneMinusCosTheta * oneMinusCosTheta;
    oneMinusCosThetaPow5 *= oneMinusCosThetaPow5;
    oneMinusCosThetaPow5 *= oneMinusCosTheta;
    
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * oneMinusCosThetaPow5;
}

//Shadow Sampling 
float SampleShadowMap(float4 shadowPos, int shadowMapIndex)
{
#if ENABLE_SHADOWS
    if (shadowMapIndex < 0)
        return 1.0;
    
    // Perform perspective divide
    float3 projCoords = shadowPos.xyz / shadowPos.w;
    
    // Check if the position is outside the shadow map bounds
    if (any(abs(projCoords.xy) > 1.0) || projCoords.z < 0.0 || projCoords.z > 1.0)
        return 1.0;
    
    // Convert from NDC to texture coordinates
    float2 shadowTexCoord = float2(projCoords.x * 0.5 + 0.5, projCoords.y * -0.5 + 0.5);
    
    // Sample the shadow map
    return shadowMaps.SampleCmpLevelZero(shadowSampler,
           float3(shadowTexCoord, shadowMapIndex), projCoords.z).r;
#else
    return 1.0;
#endif
}

//lighting calculation

float3 CalculateDirectionalLight(DirectionalLightGPU light, float3 N, float3 V, float3 albedo, float metallicValue, float roughnessValue,
                                float3 F0, float4 worldPos)
{
    if (light.intensity <= 0.0)
        return float3(0, 0, 0);
    
    float3 L = normalize(-light.direction);
    float3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return float3(0, 0, 0);
    
    float NdotV = max(dot(N, V), 0.0);
    if (NdotV <= 0.0)
        return float3(0, 0, 0);
    
    float shadow = 1.0;
#if ENABLE_SHADOWS
    if (light.shadowMapIndex >= 0 && (flags & RECEIVES_SHADOWS_FLAG))
    {
        // Transform world position to shadow space
        float4 shadowPos = mul(worldPos, light.shadowMatrices[0]);
        shadow = SampleShadowMap(shadowPos, (int)light.shadowMapIndex);
    }
#endif
    
    //BRDF calculation
    float3 radiance = light.color * light.intensity;
    
    float NDF = DistributionGGX(N, H, roughnessValue);
    float G = GeometrySmith(N, V, L, roughnessValue);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallicValue);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    //Lambert diffuse (divided by PI for energy conservation)
    float3 diffuse = kD * albedo / PI;
    
    // Final contribution
    return (diffuse + specular) * radiance * NdotL * shadow;
}

float3 CalculatePointLight(PointLightGPU light, float3 worldPos, float3 N, float3 V,
                           float3 albedo, float metallicValue, float roughnessValue, float3 F0)
{
    if (light.intensity <= 0.0)
        return float3(0, 0, 0);
    
    float3 L = light.position - worldPos;
    float distance = length(L);
    
    L /= distance;
    float3 H = normalize(V + L);
    
    if (distance > light.radius)
        return float3(0, 0, 0);
    
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return float3(0, 0, 0);
    
    float NdotV = max(dot(N, V), 0.0);
    if (NdotV <= 0.0)
        return float3(0, 0, 0);
    
    //physical attenuation with smooth falloff
    float attenuation = 1.0 / (light.attenuation.x +
                              light.attenuation.y * distance +
                              light.attenuation.z * distance * distance);
    
    // Window function for smooth falloff at radius
    float distanceRatio = distance / light.radius;
    float falloff = saturate(1.0 - pow(distanceRatio, 4.0));
    falloff *= falloff;
    attenuation *= falloff;
    
    float3 radiance = light.color * light.intensity * attenuation;

    //BRDF calculation
    float NDF = DistributionGGX(N, H, roughnessValue);
    float G = GeometrySmith(N, V, L, roughnessValue);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallicValue;
    
   // BRDF
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    float3 diffuse = kD * albedo / PI;
    
    return (diffuse + specular) * radiance * NdotL;
}

float3 CalculateSpotLight(SpotLightGPU light, float3 worldPos, float3 N, float3 V,
                         float3 albedo, float metallicValue, float roughnessValue, float3 F0)
{
    if (light.intensity <= 0.0)
        return float3(0, 0, 0);
    
    float3 L = light.position - worldPos;
    float distance = length(L);
    
    if (distance > light.range)
        return float3(0, 0, 0);
    
    L /= distance;
    float3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return float3(0, 0, 0);
    
    float NdotV = max(dot(N, V), 0.0);
    if (NdotV <= 0.0)
        return float3(0, 0, 0);
    
    //spotLight cone calculation
    float3 spotDirection = normalize(light.direction);
    float cosAngle = dot(-L, spotDirection);
    
    float epsilon = light.innerCone - light.outerCone;
    float spotFalloff = saturate((cosAngle - light.outerCone) / epsilon);
    if (spotFalloff <= 0.0)
        return float3(0, 0, 0);
    
    //phisical attenuation
    float attenuation = 1.0 / (light.attenuation.x +
                              light.attenuation.y * distance +
                              light.attenuation.z * distance * distance);
    
    float falloff = saturate(1.0 - pow(distance / light.range, 4.0));
    falloff *= falloff;
    attenuation *= falloff * spotFalloff;
    
    float3 radiance = light.color * light.intensity * attenuation;
    
    // BRDF calculation
    float NDF = DistributionGGX(N, H, roughnessValue);
    float G = GeometrySmith(N, V, L, roughnessValue);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallicValue;
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    float3 diffuse = kD * albedo / PI;
    
    return (diffuse + specular) * radiance * NdotL;
}

///Tone mapping
float3 ToneMapReinhard(float3 color)
{
    return color / (color + float3(1.0, 1.0, 1.0));
}

float3 ToneMapExposure(float3 color, float exposure)
{
    // Simple exposure adjustment
    float3 mapped = 1.0f - exp(-color * exposure);
    return saturate(mapped);
}

float3 ApplyGamma(float3 color, float gamma)
{
    return pow(saturate(color), 1.0f / gamma);
}

StandardVertexOutput StandardVertexShader(StandardVertexInput input)
{
    StandardVertexOutput output;
    
    // Initialize local space data
    float4 localPos = float4(input.position, 1.0);
    float3 localNormal = float3(0.0, 0.0, 1.0);
    float3 localTangent = float3(1.0, 0.0, 0.0);
    
#if HAS_NORMAL_ATTRIBUTE
    localNormal = input.normal;
#endif

#if HAS_TANGENT_ATTRIBUTE
    localTangent = input.tangent.xyz;
#endif

    // ========================================================================
    // SKINNING TRANSFORMATION
    // ========================================================================
#if HAS_SKINNING_ATTRIBUTES
    
    // Accumulate weighted bone transformations
    float4 skinnedPos = float4(0.0, 0.0, 0.0, 0.0);
    float3 skinnedNormal = float3(0.0, 0.0, 0.0);
    float3 skinnedTangent = float3(0.0, 0.0, 0.0);
    
    // Normalize blend weights (safety check)
    float totalWeight = input.blendWeights.x + input.blendWeights.y + 
                       input.blendWeights.z + input.blendWeights.w;
    float4 normalizedWeights = input.blendWeights;
    if (totalWeight > 0.0001)
    {
        normalizedWeights /= totalWeight;
    }
    
    // Apply linear blend skinning (max 4 bones per vertex)
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float weight = normalizedWeights[i];
        
        // Skip bones with zero influence
        if (weight > 0.0001)
        {
            uint boneIndex = input.blendIndices[i];
            
            // Validate bone index
            if (boneIndex < 128)
            {
                float4x4 boneMatrix = BoneMatrices[boneIndex];
                
                // Transform and accumulate position
                // BoneMatrix already includes: OffsetMatrix * AnimatedWorldTransform
                // This transforms: MeshSpace -> BoneSpace -> AnimatedWorldSpace
                skinnedPos += weight * mul(localPos, boneMatrix);
                
#if HAS_NORMAL_ATTRIBUTE
                // Transform normal (only rotation/scale, no translation)
                skinnedNormal += weight * mul(localNormal, (float3x3)boneMatrix);
#endif
                
#if HAS_TANGENT_ATTRIBUTE
                // Transform tangent
                skinnedTangent += weight * mul(localTangent, (float3x3)boneMatrix);
#endif
            }
        }
    }
    
    // IMPORTANT: The bone matrices already transformed vertices to world space
    // So skinnedPos is in WORLD SPACE, not local space
    output.worldPos = skinnedPos;
    
    // Transform from world space to clip space (NOT using Model matrix)
    // Skinned vertices bypass the Model matrix since bones handle transformation
    float4x4 viewProj = mul(View, Projection);
    output.position = mul(output.worldPos, viewProj);
    
#if HAS_NORMAL_ATTRIBUTE
    // Normalize the accumulated normal
    output.normal = normalize(skinnedNormal);
#endif
    
#if HAS_TANGENT_ATTRIBUTE
    // Normalize tangent and preserve handedness (w component)
    output.tangent = float4(normalize(skinnedTangent), input.tangent.w);
#endif

#else  
    // NON-SKINNED TRANSFORMATION (Standard pipeline)
    output.worldPos = mul(localPos, Model);
    output.position = mul(localPos, WVP);
    
#if HAS_NORMAL_ATTRIBUTE
    output.normal = normalize(mul(localNormal, (float3x3)Model));
#endif
    
#if HAS_TANGENT_ATTRIBUTE
    output.tangent = float4(normalize(mul(localTangent, (float3x3)Model)), input.tangent.w);
#endif
    
#endif  // End HAS_SKINNING_ATTRIBUTES

    // ========================================================================
    // PASS-THROUGH ATTRIBUTES (same for skinned and non-skinned)
    // ========================================================================
    
#if HAS_TEXCOORDS_ATTRIBUTE
    output.texCoord = input.texCoord;
#endif
    
#if HAS_SECOND_UV_ATTRIBUTE
    output.texCoord1 = input.texCoord1;
#endif
    
#if HAS_VERTEX_COLOR_ATTRIBUTE
    output.color = input.color;
#endif

    // Calculate view direction (from vertex to camera)
    output.viewDir = normalize(CameraPosition - output.worldPos.xyz);
    
    // Pass time for animated effects
    output.time = Time;
    
    return output;
}
#endif