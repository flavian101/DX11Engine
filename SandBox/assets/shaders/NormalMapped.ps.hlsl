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
    float padding;
};

cbuffer DirectionalLightData : register(b2)
{
    float4 d_Color;
    float4 d_Ambient;
    float4 d_Diffuse;
    float3 d_Direction;
    int d_Enabled;
};

cbuffer PointLightData : register(b3)
{
    float3 p_Position;
    float p_Range;
    float4 p_Color;
    float3 p_Attenuation;
    int p_Enabled;
};

cbuffer SpotLightData : register(b4)
{
    float4 s_Color;
    float3 s_Position;
    float s_Range;
    float3 s_Direction;
    float s_Cone;
    float3 s_Attenuation;
    int s_Enabled;
};

struct PS_input
{
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float2 texCoords : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangents : TANGENT;
};

Texture2D diffuseTexture : register(t0);
Texture2D normalMap : register(t1);
SamplerState textureSampler : register(s0);

float3 CalculateSpotLight(PS_input input, float4 baseColor, float3 worldNormal)
{
    float3 lightToPixelVec = s_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    if (distance > s_Range)
        return float3(0.0f, 0.0f, 0.0f);
        
    lightToPixelVec /= distance;
    
    float nDotL = saturate(dot(lightToPixelVec, worldNormal));
    
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (s_Attenuation[0] +
                               (s_Attenuation[1] * distance) +
                               (s_Attenuation[2] * distance * distance));
    
    float spotEffect = pow(max(dot(-lightToPixelVec, normalize(s_Direction)), 0.0f), s_Cone);
    
    float3 lightContrib = baseColor.rgb * s_Color.rgb * nDotL * attenuation * spotEffect;
    
    return lightContrib;
}

float3 CalculateDirectionalLight(float3 worldNormal, float4 baseColor)
{
    float3 lightDir = normalize(-d_Direction);
    float nDotL = saturate(dot(worldNormal, lightDir));
    
    // Ambient contribution
    float3 ambient = baseColor.rgb * d_Ambient.rgb;
    
    // Diffuse contribution
    float3 diffuse = baseColor.rgb * d_Color.rgb * nDotL;
    
    return ambient + diffuse;
}

float3 CalculatePointLight(PS_input input, float4 baseColor, float3 worldNormal)
{
    float3 lightToPixelVec = p_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    if (distance > p_Range)
        return float3(0.0f, 0.0f, 0.0f);
        
    lightToPixelVec /= distance;
    
    float nDotL = saturate(dot(worldNormal, lightToPixelVec));
    
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (p_Attenuation[0] +
                               (p_Attenuation[1] * distance) +
                               (p_Attenuation[2] * distance * distance));
    
    float3 lightContrib = baseColor.rgb * p_Color.rgb * nDotL * attenuation;
    
    return lightContrib;
}

float4 main(PS_input input) : SV_Target
{
    // Normalize the input normal
    input.normal = normalize(input.normal);
    
    // Sample the base texture
    float4 baseColor = diffuseColor;
    if (flags & 1) // HasDiffuseTexture flag
    {
        float2 scaledUV = input.texCoords * textureScale + textureOffset;
        baseColor = diffuseTexture.Sample(textureSampler, scaledUV);
        baseColor *= diffuseColor; // Modulate with material color
    }
    
    // Calculate world space normal
    float3 worldNormal;
    if (flags & 2) // HasNormalMap flag
    {
        float2 scaledUV = input.texCoords * textureScale + textureOffset;
        float3 normalSampleTS = normalMap.Sample(textureSampler, scaledUV).rgb * 2.0f - 1.0f;

        // Normalize vertex axes
        float3 T = normalize(input.tangents.xyz);
        float3 N = normalize(input.normal);
        float3 B = normalize(cross(N, T) * input.tangents.w);

        worldNormal = normalize(
            normalSampleTS.x * T +
            normalSampleTS.y * B +
            normalSampleTS.z * N
        );
    }
    else
    {
        worldNormal = input.normal;
    }
    
    // Initialize final color with emissive
    float3 finalColor = emissiveColor.rgb;
    
    // Add contributions from each active light type
    if (d_Enabled)
    {
        finalColor += CalculateDirectionalLight(worldNormal, baseColor);
    }
    
    if (p_Enabled)
    {
        finalColor += CalculatePointLight(input, baseColor, worldNormal);
    }
    
    if (s_Enabled)
    {
        finalColor += CalculateSpotLight(input, baseColor, worldNormal);
    }
    
    return float4(finalColor, baseColor.a * alpha);
}