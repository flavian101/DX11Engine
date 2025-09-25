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

cbuffer MaterialBufferData : register(b1)
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

// Enhanced Scene Lighting buffer (register b7)
cbuffer SceneLightData : register(b7)
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

cbuffer UIBuffer : register(b5)
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
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    
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
    float3 normal : NORMAL;
    
#if HAS_TANGENT_ATTRIBUTE
    float4 tangent : TANGENT;
#endif

    float2 texCoord : TEXCOORD;
    
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
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct UIVertexInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
#if HAS_VERTEX_COLOR_ATTRIBUTE
    float4 color : COLOR;
#endif
    float3 normal : NORMAL;
};

struct UIVertexOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
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


#if ENABLE_SHADOWS
Texture2DArray shadowMaps : register(t5);
SamplerComparisonState shadowSampler : register(s1);
#endif


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

float3 SampleNormalMap(float2 uv)
{
#if HAS_NORMAL_MAP
    float2 scaledUV = uv * textureScale + textureOffset;
    float3 normal = normalTexture.Sample(standardSampler, scaledUV).rgb;
    return normalize(normal * 2.0 - 1.0);
#else
    return float3(0.0, 0.0, 1.0); // Default normal
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
    float3 N = normalize(vertexNormal);
    float3 T = normalize(tangent.xyz);
    float3 B = normalize(cross(N, T) * tangent.w);
    
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(normalMapSample, TBN));
#else
    return normalize(vertexNormal);
#endif
}

float3 CalculateWorldNormalFallback(float3 vertexNormal, float3 normalMapSample)
{
    if (!(flags & HAS_NORMAL_MAP_FLAG))
    {
        return normalize(vertexNormal);
    }
    
    // Simple perturbation without proper tangent space
    float3 N = normalize(vertexNormal);
    return normalize(N + normalMapSample * 0.1);
}

// Safe normal calculation that works with or without tangents
float3 GetWorldNormal(StandardVertexOutput input, float3 normalSample)
{
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE
    normalSample = SampleNormalMap(input.texCoord);
    return CalculateWorldNormal(input.normal, input.tangent, normalSample);
#elif HAS_NORMAL_MAP
    // Fallback: simple normal perturbation without tangent space
    normalSample = SampleNormalMap(input.texCoord);
    float3 N = normalize(input.normal);
    return normalize(N + normalSample * 0.1);
#else
    return normalize(input.normal);
#endif
}

///pbr BRDF funtions

float DistributionGGX(float3 N, float H, float roughness)
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

float GeometrySmith(float N, float V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGXX(NdotV, roughness);
    float ggx1 = GeometrySchlickGXX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
                                float F0, float4 worldPos)
{
    if(light.intensity<= 0.0)
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
    if (light.intensity >= 0.0)
        return float3(0, 0, 0);
    
    float3 L = light.position - worldPos;
    float distance = length(L);
    
    L /= distance;
    float3 H = normalize(V + L);
    
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
    float F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
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
    return float3(1.0, 1.0, 1.0) - exp(-color * exposure);
}
float ApplyGamma(float3 color, float gamma)
{
    return pow(abs(color), float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));
}

StandardVertexOutput StandardVertexShader(StandardVertexInput input)
{
    StandardVertexOutput output;
    
    output.position = mul(float4(input.position, 1.0), WVP);
    output.worldPos = mul(float4(input.position, 1.0), Model);
    output.normal = mul(input.normal, (float3x3)Model);
    output.texCoord = input.texCoord;
    
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

//// Debug visualization functions
//#ifdef SHADER_DEBUG
//float4 DebugVertexAttributes(StandardVertexOutput input)
//{
//#if HAS_VERTEX_COLOR_ATTRIBUTE
//    return input.color; // Show vertex colors
//#elif HAS_TANGENT_ATTRIBUTE
//    return float4(input.tangent.xyz * 0.5 + 0.5, 1.0); // Show tangents
//#else
//    return float4(input.normal * 0.5 + 0.5, 1.0); // Show normals
//#endif
//}
//#endif

#endif