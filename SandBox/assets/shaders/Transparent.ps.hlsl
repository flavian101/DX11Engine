// Transparent Pixel Shader - For alpha blended materials

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

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

float3 CalculateDirectionalLight(float3 normal, float3 viewDir, float4 baseColor)
{
    float3 lightDir = normalize(-d_Direction);
    float nDotL = saturate(dot(normal, lightDir));
    
    // Ambient contribution
    float3 ambient = baseColor.rgb * d_Ambient.rgb;
    
    // Diffuse contribution
    float3 diffuse = baseColor.rgb * d_Color.rgb * nDotL;
    
    // Specular contribution (reduced for transparent materials)
    float3 halfDir = normalize(lightDir + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess) * 0.5f; // Reduced specular for transparency
    float3 specularContrib = specularColor.rgb * d_Color.rgb * specular * nDotL;
    
    return ambient + diffuse + specularContrib;
}

float3 CalculatePointLight(PS_input input, float3 normal, float3 viewDir, float4 baseColor)
{
    float3 lightToPixelVec = p_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    if (distance > p_Range)
        return float3(0.0f, 0.0f, 0.0f);
        
    lightToPixelVec /= distance;
    
    float nDotL = saturate(dot(normal, lightToPixelVec));
    
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    float attenuation = 1.0f / (p_Attenuation[0] +
                               (p_Attenuation[1] * distance) +
                               (p_Attenuation[2] * distance * distance));
    
    // Diffuse
    float3 diffuse = baseColor.rgb * p_Color.rgb * nDotL * attenuation;
    
    // Reduced specular for transparent materials
    float3 halfDir = normalize(lightToPixelVec + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess) * 0.5f;
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
    
    // Reduced specular
    float3 halfDir = normalize(lightToPixelVec + viewDir);
    float nDotH = saturate(dot(normal, halfDir));
    float specular = pow(nDotH, shininess) * 0.5f;
    float3 specularContrib = specularColor.rgb * s_Color.rgb * specular * nDotL * attenuation * spotEffect;
    
    return diffuse + specularContrib;
}

float4 main(PS_input input) : SV_Target
{
    // Normalize the input normal
    float3 normal = normalize(input.normal);
    
    // Calculate view direction
    float3 viewDir = normalize(-input.worldPos.xyz);
    
    // Sample the base texture with scaling and offset
    float4 baseColor = diffuseColor;
    if (flags & 1) // HasDiffuseTexture flag
    {
        float2 scaledUV = input.texCoords * textureScale + textureOffset;
        float4 textureColor = diffuseTexture.Sample(textureSampler, scaledUV);
        baseColor = textureColor * diffuseColor;
        
        // Use texture alpha for transparency
        baseColor.a *= textureColor.a;
    }
    
    // Initialize final color with emissive
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
    
    // Apply final alpha (combination of material alpha and texture alpha)
    float finalAlpha = baseColor.a * alpha;
    
    // Ensure alpha is within valid range
    finalAlpha = saturate(finalAlpha);
    
    return float4(finalColor, finalAlpha);
}