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

cbuffer TransformBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
    float4x4 View;
    float4x4 Projection;
    float3 CameraPosition;
    float Time;
    float ReceiveShadows;      // 1 = this draw looks the shadow maps up (MeshRendererComponent::receivesShadows), 0 = it does not
    float3 transformPadding;
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
    
};

// Scene lighting: the environment + the DIRECTIONAL lights (few, every pixel needs all of them).
// Mirrors SceneLightData in utils/ConstantBufferTypes.h.
cbuffer SceneLightData : register(b3)
{
    // Light counts
    uint directionalLightCount;
    uint localLightCount;      // valid elements of localLights below
    uint2 lightPadding;
    
    // Global parameters
    float3 ambientColor;
    float ambientIntensity;
    float iblIntensity;
    float exposure;
    float gamma;
    float globalPadding;
    
    // Directional lights (max 4). 64 bytes each; mirrors DirectionalLightGPU in utils/ConstantBufferTypes.h.
    // The cascade matrices are NOT here: every shadow view lives in the shadowViews buffer (t36), and a directional
    // light only says where its cascades start.
    struct DirectionalLightGPU
    {
        float3 direction;
        float intensity;
        float3 color;
        int shadowViewIndex;       // index of cascade 0 in shadowViews; the cascades follow it. -1 = no shadow
        float4 cascadeSplits;      // view-space depth at the FAR edge of cascade 0..3
        float4 shadowParams;       // x = cascade count, y = depth where the last cascade starts to fade out
    } directionalLights[4];
};

// LOCAL lights (point + spot): a structured buffer, so a scene is not limited to a few dozen.
// Mirrors LightGPU in utils/ConstantBufferTypes.h FIELD FOR FIELD (64 bytes; that header static_asserts the
// size and offsets). t32 = SRV_Lights; t0..t31 are the material textures.
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT  2

struct LightGPU
{
    float3 position;
    float  range;          // distance where the light has fully faded

    float3 color;          // linear RGB, already multiplied by the intensity
    uint   type;           // LIGHT_TYPE_*

    float3 direction;      // spot: the way the light points (unit)
    int    shadowIndex;    // -1 = no shadow

    float  spotScale;      // cone = saturate(dot(-L, direction) * spotScale + spotOffset)
    float  spotOffset;
    float2 lightPadding2;
};

StructuredBuffer<LightGPU> localLights : register(t32);

// CLUSTERED light culling. The view frustum is cut into clusterGrid.x * clusterGrid.y screen tiles and
// clusterGrid.z exponential depth slices. For every cluster the CPU lists the lights that reach it, so a pixel only
// loops over the lights of its own cluster. Mirrors ClusterConstants in utils/ConstantBufferTypes.h.
#define LIGHT_CULLING_NONE       0    // loop over ALL local lights (reference path)
#define LIGHT_CULLING_CLUSTERED  1
#define LIGHT_CULLING_HEATMAP    2    // clustered, and the pixel shows how many lights its cluster has

cbuffer ClusterData : register(b7)
{
    uint3  clusterGrid;
    uint   lightCullingMode;
    float2 clusterTilesPerPixel;      // clusterGrid.xy / screen size
    float  clusterSliceScale;         // slice = floor(ln(viewZ) * scale + bias)
    float  clusterSliceBias;
};

StructuredBuffer<uint2> clusterRanges       : register(t33);   // {offset, count} per cluster
StructuredBuffer<uint>  clusterLightIndices : register(t34);   // indices into localLights, per cluster

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
// ONE depth atlas holds every shadow view (directional cascades, spot views, point-light faces) as a square tile.
// Each view is one element of shadowViews. Mirrors ShadowViewGPU / ShadowConstants in utils/ConstantBufferTypes.h.
// (t14 = TextureSlot::Shadow used to be reserved here; it sat inside the material texture range, so the atlas lives
// at t35 with the other engine-wide resources.)
#if ENABLE_SHADOWS
struct ShadowView
{
    float4 row0;               // world -> light clip matrix, as DirectXMath stores it: clip = row0*x + row1*y + row2*z + row3
    float4 row1;
    float4 row2;
    float4 row3;
    float4 atlasRect;          // this view's tile in the atlas, in UV: xy = offset, zw = size
    float4 params;             // x = normal offset (texels), y = world size of a texel (perspective: at distance 1),
                               // z = constant depth bias (NDC), w = 0 orthographic / 1 perspective
};

Texture2D<float>             shadowAtlas : register(t35);
StructuredBuffer<ShadowView> shadowViews : register(t36);
SamplerComparisonState       shadowSampler : register(s1);   // linear + "reference <= stored": 1 = lit

cbuffer ShadowData : register(b5)
{
    float shadowAtlasSize;     // texels (square)
    float shadowInvAtlasSize;
    uint  shadowPcfRadius;     // 0 = one tap, 1 = 3x3, 2 = 5x5, 3 = 7x7 (every tap is itself a 2x2 hardware PCF)
    uint  shadowsEnabled;
};
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

// ========== SHADOW LOOKUPS ==========
// Every function returns the LIT fraction: 1 = fully lit, 0 = fully in shadow. Without ENABLE_SHADOWS (or for a light /
// object that has no shadow) they cost nothing and return 1.
#if ENABLE_SHADOWS
float4 ShadowProject(ShadowView v, float3 p)
{
    return v.row0 * p.x + v.row1 * p.y + v.row2 * p.z + v.row3;     // (p, 1) through the rows: no matrix-packing rule involved
}

// One view: normal offset -> project -> PCF. 'N' must be the surface normal, 'NdotL' how directly the light hits it.
float SampleShadowView(ShadowView v, float3 worldPos, float3 N, float NdotL)
{
    // 1) NORMAL OFFSET: push the receiver a little along its normal, more when the light grazes it. This removes shadow
    //    acne without the peter-panning a big depth bias causes. Measured in shadow TEXELS so it follows the resolution.
    const float w0 = ShadowProject(v, worldPos).w;
    const float texelWorld = v.params.y * (v.params.w > 0.5 ? max(w0, 0.0) : 1.0);   // perspective: a texel grows with distance
    const float3 pos = worldPos + N * (v.params.x * texelWorld * (0.25 + saturate(1.0 - NdotL)));

    const float4 clip = ShadowProject(v, pos);
    if (clip.w <= 0.0)
        return 1.0;
    const float3 ndc = clip.xyz / clip.w;

    // outside the view = nothing there can shadow it
    if (any(abs(ndc.xy) > 1.0) || ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    // 2) NDC -> a position inside this view's tile of the atlas
    const float2 tileUV = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    const float2 center = v.atlasRect.xy + tileUV * v.atlasRect.zw;
    const float  depth  = ndc.z - v.params.z;                         // constant bias

    // every tap stays INSIDE the tile: a tap that left it would read the neighbouring view's depths
    const float2 lo = v.atlasRect.xy + 0.5 * shadowInvAtlasSize;
    const float2 hi = v.atlasRect.xy + v.atlasRect.zw - 0.5 * shadowInvAtlasSize;

    // 3) PCF: (2r+1)^2 taps, each a hardware 2x2 bilinear comparison
    const int r = (int)shadowPcfRadius;
    float sum = 0.0;
    [loop]
    for (int y = -r; y <= r; ++y)
    {
        [loop]
        for (int x = -r; x <= r; ++x)
        {
            const float2 uv = clamp(center + float2(x, y) * shadowInvAtlasSize, lo, hi);
            sum += shadowAtlas.SampleCmpLevelZero(shadowSampler, uv, depth);
        }
    }
    const float taps = (float)((2 * r + 1) * (2 * r + 1));
    return sum / taps;
}

// Cascaded shadow map of a directional light: the cascade comes from the pixel's view-space depth.
float DirectionalShadow(DirectionalLightGPU light, float3 worldPos, float3 N, float NdotL)
{
    if (light.shadowViewIndex < 0 || shadowsEnabled == 0 || ReceiveShadows < 0.5)
        return 1.0;

    const uint cascades = max((uint)(light.shadowParams.x + 0.5), 1u);
    const float viewZ = mul(float4(worldPos, 1.0), View).z;
    const float lastSplit = light.cascadeSplits[cascades - 1];
    if (viewZ > lastSplit)
        return 1.0;                                                    // past the last cascade: the shadow has ended

    uint cascade = 0;
    [unroll]
    for (uint i = 0; i < 3; ++i)
        if (i + 1 < cascades && viewZ > light.cascadeSplits[i])
            cascade = i + 1;

    const float shadow = SampleShadowView(shadowViews[light.shadowViewIndex + cascade], worldPos, N, NdotL);

    // the end of the last cascade fades out instead of popping
    const float fade = saturate((lastSplit - viewZ) / max(lastSplit - light.shadowParams.y, 0.001));
    return lerp(1.0, shadow, fade);
}

// Spot light: one view. Point light: six faces, the one the direction (fragment - light) points at.
float LocalShadow(LightGPU light, float3 worldPos, float3 N, float NdotL)
{
    if (light.shadowIndex < 0 || shadowsEnabled == 0 || ReceiveShadows < 0.5)
        return 1.0;

    uint viewIndex = (uint)light.shadowIndex;
    if (light.type == LIGHT_TYPE_POINT)
    {
        // the dominant axis picks the face (+X -X +Y -Y +Z -Z): the same rule as ShadowMath::PointFaceIndex on the CPU
        const float3 d = worldPos - light.position;
        const float3 a = abs(d);
        uint face;
        if (a.x >= a.y && a.x >= a.z)  face = d.x > 0.0 ? 0 : 1;
        else if (a.y >= a.z)           face = d.y > 0.0 ? 2 : 3;
        else                           face = d.z > 0.0 ? 4 : 5;
        viewIndex += face;
    }
    return SampleShadowView(shadowViews[viewIndex], worldPos, N, NdotL);
}
#endif

//lighting calculation

// Cook-Torrance (GGX) + Lambert for ONE light direction. 'L' points from the surface TO the light and
// 'radiance' is what arrives at the surface (colour * intensity * attenuation). Every light type ends here,
// so the BRDF exists exactly once.
float3 EvaluateDirectLight(float3 N, float3 V, float3 L, float3 radiance,
                           float3 albedo, float metallicValue, float roughnessValue, float3 F0)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0)
        return float3(0, 0, 0);
    
    // Clamp instead of returning black: interpolated or normal-mapped normals often face slightly away
    // from the viewer near silhouettes, and an early-out there paints dark speckles.
    float NdotV = max(dot(N, V), 0.001);
    
    float3 H = normalize(V + L);
    
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
    
    return (diffuse + specular) * radiance * NdotL;
}

float3 CalculateDirectionalLight(DirectionalLightGPU light, float3 N, float3 V, float3 albedo, float metallicValue, float roughnessValue,
                                float3 F0, float4 worldPos)
{
    if (light.intensity <= 0.0)
        return float3(0, 0, 0);
    
    float3 L = normalize(-light.direction);
    
    if (dot(N, L) <= 0.0)
        return float3(0, 0, 0);
    
    float shadow = 1.0;
#if ENABLE_SHADOWS
    shadow = DirectionalShadow(light, worldPos.xyz, N, dot(N, L));
#endif
    
    float3 radiance = light.color * light.intensity;
    return EvaluateDirectLight(N, V, L, radiance, albedo, metallicValue, roughnessValue, F0) * shadow;
}

// Distance falloff of a local light: 1 / (1 + d^2), windowed to exactly zero at 'range'.
// (The curve the old attenuation vector {1, 0, 1} produced. Changing the falloff model is a change to this
// one function.)
float LightDistanceAttenuation(float distance, float range)
{
    float ratio2 = (distance * distance) / (range * range);
    float window = saturate(1.0 - ratio2 * ratio2);      // 1 - (d / range)^4
    return (window * window) / (1.0 + distance * distance);
}

// One point or spot light (an element of localLights).
float3 CalculateLocalLight(LightGPU light, float3 worldPos, float3 N, float3 V,
                           float3 albedo, float metallicValue, float roughnessValue, float3 F0)
{
    float3 toLight = light.position - worldPos;
    float distance = max(length(toLight), 0.0001);       // avoid L / 0 when the surface is at the light
    
    if (distance > light.range)
        return float3(0, 0, 0);
    
    float3 L = toLight / distance;
    float attenuation = LightDistanceAttenuation(distance, light.range);
    
    if (light.type == LIGHT_TYPE_SPOT)
        attenuation *= saturate(dot(-L, light.direction) * light.spotScale + light.spotOffset);
    
    if (attenuation <= 0.0)
        return float3(0, 0, 0);
    
    float3 lit = EvaluateDirectLight(N, V, L, light.color * attenuation, albedo, metallicValue, roughnessValue, F0);

#if ENABLE_SHADOWS
    // the lookup is the expensive part: only for a light that actually reaches this pixel
    if (light.shadowIndex >= 0 && any(lit > 0.0))
        lit *= LocalShadow(light, worldPos, N, dot(N, L));
#endif
    return lit;
}

// The cluster of this pixel. 'svPosition' is the pixel shader's SV_POSITION: xy = pixel coordinates in the render
// target, w = 1 / w_clip, and for a perspective projection w_clip IS the view-space depth.
// The slice formula is the one LightClusterer uses on the CPU (the unit tests drive both from the same constants).
uint ClusterIndexOf(float4 svPosition)
{
    float viewZ = 1.0 / svPosition.w;
    int slice = (int)floor(log(max(viewZ, 1e-4)) * clusterSliceScale + clusterSliceBias);
    slice = clamp(slice, 0, (int)clusterGrid.z - 1);
    
    uint2 tile = min((uint2)(svPosition.xy * clusterTilesPerPixel), clusterGrid.xy - 1);
    return tile.x + tile.y * clusterGrid.x + (uint)slice * (clusterGrid.x * clusterGrid.y);
}

// All local lights that reach this pixel. The ONE place that decides which lights a pixel considers: every
// lit shader calls it.
float3 ShadeLocalLights(float4 svPosition, float3 worldPos, float3 N, float3 V,
                        float3 albedo, float metallicValue, float roughnessValue, float3 F0)
{
    float3 result = float3(0, 0, 0);
    
    if (lightCullingMode == LIGHT_CULLING_NONE)
    {
        // Reference path: every light, every pixel.
        [loop]
        for (uint i = 0; i < localLightCount; ++i)
        {
            result += CalculateLocalLight(localLights[i], worldPos, N, V,
                                          albedo, metallicValue, roughnessValue, F0);
        }
    }
    else
    {
        // Only the lights listed for this pixel's cluster.
        uint2 range = clusterRanges[ClusterIndexOf(svPosition)];   // x = offset, y = count
        
        [loop]
        for (uint i = 0; i < range.y; ++i)
        {
            result += CalculateLocalLight(localLights[clusterLightIndices[range.x + i]], worldPos, N, V,
                                          albedo, metallicValue, roughnessValue, F0);
        }
    }
    return result;
}

// Debug view (LIGHT_CULLING_HEATMAP): how many lights does this pixel's cluster hold?
// dark = none, blue = few, green = some, red = 32 or more.
float3 LightHeatmap(float4 svPosition)
{
    uint count = clusterRanges[ClusterIndexOf(svPosition)].y;
    if (count == 0)
        return float3(0.02, 0.02, 0.04);
    
    float t = saturate((float)count / 32.0);
    float3 cool = float3(0.0, 0.25, 1.0);
    float3 mid = float3(0.0, 1.0, 0.2);
    float3 hot = float3(1.0, 0.1, 0.0);
    return (t < 0.5) ? lerp(cool, mid, t * 2.0) : lerp(mid, hot, t * 2.0 - 1.0);
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