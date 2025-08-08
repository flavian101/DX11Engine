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
};

SamplerState textureSampler : register(s0);

float3 CalculateDirectionalLight(float3 normal, float3 viewDir, float4 basecolor)
{
    float3 lightDir = normalize(-d_Direction);
    float nDotL = saturate(dot(normal, lightDir));
    
    ///ambient contrib
    float3 ambient = basecolor.rgb * d_Ambient.rgb;
   
    //diffuse contrib
    float3 diffuse = basecolor.rgb * d_Color.rgb * nDotL;
    
    //specular
    float3 halfDir = normalize(lightDir + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess);
    float3 specularContrib = specularColor.rgb * d_Color.rgb * specular * nDotL;
    
    return ambient + diffuse + specularContrib;
}

float3 CalculatePointLight(PS_input input, float3 normal, float3 viewDir, float4 baseColor)
{
    float3 lightToPixelVec = p_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    if(distance > p_Range)
        return float3(0.0f, 0.0f, 0.0f);
    
    lightToPixelVec /= distance;
    
    float nDotL = saturate(dot(normal, lightToPixelVec));
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (p_Attenuation[0] +
                               (p_Attenuation[1] * distance) +
                               (p_Attenuation[2] * distance * distance));
    float3 diffuse = baseColor.rgb * p_Color.rgb * nDotL * attenuation;

    float3 halfDir = normalize(lightToPixelVec + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess);
    float3 specularContrib = specularColor.rgb * p_Color.rgb * specular * nDotL * attenuation;
    
    return diffuse + specularContrib;
}

float3 CalculateSpotLight(PS_input input, float3 normal, float3 viewDir, float4 baseColor)
{
    float3 lightToPixelVec = s_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);

    if (distance > s_Range)
        return float3(0.0f, 0.0f, 0.0f);
    
    lightToPixelVec /= distance;

    float nDotL = saturate(dot(normal, lightToPixelVec));
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (s_Attenuation[0] +
                               (s_Attenuation[1] * distance) +
                               (s_Attenuation[2] * distance * distance));
    
    float spotEffect = pow(max(dot(-lightToPixelVec, normalize(s_Direction)), 0.0f), s_Cone);
    
    float3 diffuse = baseColor.rgb * s_Color.rgb * nDotL * attenuation * spotEffect;
    
    // Specular
    float3 halfDir = normalize(lightToPixelVec + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess);
    float3 specularContrib = specularColor.rgb * s_Color.rgb * specular * nDotL * attenuation * spotEffect;
    
    return diffuse + specularContrib;
}

float4 main(PS_input input) : SV_Target
{
    // Normalize the input normal
    float3 normal = normalize(input.normal);
    
    // Calculate view direction (assuming camera at origin for now)
    float3 viewDir = normalize(-input.worldPos.xyz);
    
    // Base color from material
    float4 baseColor = diffuseColor;
    
    // Initialize final color
    float3 finalColor = emissiveColor.rgb;
    
    // Add contributions from each active light type
    if (d_Enabled)
    {
        finalColor += CalculateDirectionalLight(normal, viewDir, baseColor);
    }
    
    if (p_Enabled)
    {
        finalColor += CalculatePointLight(input, normal, viewDir, baseColor);
    }
    
    if (s_Enabled)
    {
        finalColor += CalculateSpotLight(input, normal, viewDir, baseColor);
    }
    
    return float4(finalColor, baseColor.a * alpha);
}