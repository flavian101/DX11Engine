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

// Vertex attribute features (set by vertex layout analysis)
#ifndef HAS_NORMAL_ATTRIBUTE
#define HAS_NORMAL_ATTRIBUTE 1 
#endif

#ifndef HAS_TEXCOORDS_ATTRIBUTE
#define HAS_TEXCOORDS_ATTRIBUTE 1 
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
    float3 CameraPosition;
    float Time;
};

cbuffer MaterialBufferData : register(b2)
{
    float4 diffuseColor;
    float4 specularColor;
    float4 emissiveColor;
    float shininess;
    float metallic;
    float roughness;
    float alpha;
    float2 textureScale;
    float2 textureOffset;
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
Texture2D diffuseTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D specularTexture : register(t2);
Texture2D emissiveTexture : register(t3);
TextureCube environmentTexture : register(t4);

SamplerState standardSampler : register(s0);

#if ENABLE_SHADOWS
Texture2DArray shadowMaps : register(t5);
SamplerComparisonState shadowSampler : register(s1);
#endif

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
    
    // DEBUG: Check if texture is actually bound and valid
    if (all(normalSample.rgb == float3(0, 0, 0)) || all(normalSample.rgb == float3(1, 1, 1)))
    {
        // Texture might not be bound or is invalid
        return float3(0.0, 0.0, 1.0);
    }
    
    float3 normal = normalSample.rgb * 2.0 - 1.0;
    return length(normal) > EPSILON ? normalize(normal) : float3(0.0, 0.0, 1.0);
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

//Normal Mappings

float3 CalculateWorldNormal(float3 vertexNormal, float4 tangent, float3 normalMapSample)
{
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE
    // Validate inputs
    if (length(vertexNormal) < EPSILON || length(tangent.xyz) < EPSILON)
        return float3(0.0, 0.0, 1.0);
    
    float3 N = normalize(vertexNormal);
    float3 T = normalize(tangent.xyz);
    
    // Re-orthogonalize tangent (Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);
    
    float3 B = cross(N, T) * tangent.w;
    
    // Validate TBN matrix determinant
    float det = dot(cross(T, B), N);
    if (abs(det) < EPSILON)
        return N; // Fall back to vertex normal
    
    float3x3 TBN = float3x3(T, B, N);
    float3 result = mul(normalMapSample, TBN);
    
    return length(result) > EPSILON ? normalize(result) : N;
#else
    return length(vertexNormal) > EPSILON ? normalize(vertexNormal) : float3(0.0, 0.0, 1.0);
#endif
}

float3 GetFinalWorldNormal(StandardVertexOutput input)
{
    float3 baseNormal = float3(0.0, 0.0, 1.0); // Default
    
#if HAS_NORMAL_ATTRIBUTE
    baseNormal = length(input.normal) > EPSILON ? normalize(input.normal) : baseNormal;
#endif

#if HAS_NORMAL_MAP && HAS_TEXCOORDS_ATTRIBUTE
    float3 normalSample = SampleNormalMap(input.texCoord);
    
#if HAS_TANGENT_ATTRIBUTE
        // Full tangent space normal mapping
        return CalculateWorldNormal(baseNormal, input.tangent, normalSample);
#else
        // Fallback: Use detail normal mapping technique
        return ApplyDetailNormal(baseNormal, normalSample, input.worldPos.xyz);
#endif
#else
    return baseNormal;
#endif
}

float3 ApplyDetailNormal(float3 vertexNormal, float3 normalMapSample, float3 worldPos)
{
    // Use world position to generate fake tangent space
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float3 N = normalize(vertexNormal);
    
    // Generate tangent vectors from position derivatives
    float3 T = normalize(dp1 - dot(dp1, N) * N);
    float3 B = normalize(cross(N, T));
    
    // Apply normal map
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(normalMapSample, TBN));
}

///pbr BRDF functions

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

float GeometrySchlickGXX(float NdotV, float roughness)
{
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
    float ggx2 = GeometrySchlickGXX(NdotV, roughness);
    float ggx1 = GeometrySchlickGXX(NdotL, roughness);
    
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
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    kD *= 1.0 - metallicValue;
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    return (kD * albedo / PI + specular) * radiance * NdotL * shadow;
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
    
    //physical attenuation with smooth falloff
    float attenuation = 1.0 / (light.attenuation.x +
                              light.attenuation.y * distance +
                              light.attenuation.z * distance * distance);
    float falloff = saturate(1.0 - pow(distance / light.radius, 4.0));
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
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    return (kD * albedo / PI + specular) * radiance * NdotL;
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
    
    //spotLight cone calculation
    float3 spotDirection = normalize(light.direction);
    float cosAngle = dot(-L, spotDirection);
    
    float spotFalloff = saturate((cosAngle - light.outerCone) / (light.innerCone - light.outerCone));
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
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
    
    float3 specular = numerator / denominator;
    
    return (kD * albedo / PI + specular) * radiance * NdotL;
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
    
    output.position = mul(float4(input.position, 1.0), WVP);
    output.worldPos = mul(float4(input.position, 1.0), Model);

#if HAS_NORMAL_ATTRIBUTE
    output.normal = mul(input.normal, (float3x3) Model);
#endif
    
#if HAS_TEXCOORDS_ATTRIBUTE
    output.texCoord = input.texCoord;
#endif
#if HAS_SECOND_UV_ATTRIBUTE
    output.texCoord1 = input.texCoord1; 
#endif
    
#if HAS_TANGENT_ATTRIBUTE
    output.tangent = float4(mul(input.tangent.xyz, (float3x3)Model), input.tangent.w);
#endif

#if HAS_VERTEX_COLOR_ATTRIBUTE
    output.color = input.color;
#endif

#if HAS_SECOND_UV_ATTRIBUTE
    output.texCoord1 = input.texCoord1;
#endif

    output.viewDir = CameraPosition - output.worldPos.xyz;
    output.time = Time;
    
#if HAS_SKINNING_ATTRIBUTES
    // Apply skinning transformations
    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);
    
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float weight = input.blendWeights[i];
        if (weight > 0.0)
        {
            uint boneIndex = input.blendIndices[i];
            // Apply bone transformation (you'd have bone matrices in a constant buffer)
            // skinnedPos += weight * mul(float4(input.position, 1.0), BoneMatrices[boneIndex]);
            // skinnedNormal += weight * mul(input.normal, (float3x3)BoneMatrices[boneIndex]);
        }
    }
    
    // Use skinned position/normal instead of input ones
    // output.position = mul(skinnedPos, WVP);
    // output.normal = normalize(skinnedNormal);
#endif
    
    return output;
}

#endif