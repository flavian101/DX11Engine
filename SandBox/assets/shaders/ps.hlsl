cbuffer MaterialBufferData : register(b1)
{
    float4 difColor;
    bool hasTexture;
    bool hasNormalMap;
};

cbuffer DirectionalLightData : register(b2)
{
    float4 d_Color;
    float4 d_Ambient;
    float4 d_Diffuse;
    float3 d_Direction;
    int d_Enable;
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

Texture2D tex : register(t0);
Texture2D normalMap : register(t1);
SamplerState samp;

// Helper function to calculate basic lighting components
struct LightResult
{
    float3 diffuse;
    float3 ambient;
};

float3 CalculateSpotLight(PS_input input, float4 baseColor)
{
    float3 lightToPixelVec = s_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    // Early exit if beyond range
    if (distance > s_Range)
        return float3(0.0f, 0.0f, 0.0f);
        
    lightToPixelVec /= distance; // Normalize
    
    // Calculate diffuse lighting
    float nDotL = saturate(dot(lightToPixelVec, input.normal));
    
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    // Calculate attenuation
    float attenuation = 1.0f / (s_Attenuation[0] +
                               (s_Attenuation[1] * distance) +
                               (s_Attenuation[2] * distance * distance));
    
    // Calculate spot cone falloff
    float spotEffect = pow(max(dot(-lightToPixelVec, normalize(s_Direction)), 0.0f), s_Cone);
    
    // Combine all factors
    float3 lightContrib = baseColor.rgb * s_Color.rgb * nDotL * attenuation * spotEffect;
    
    return lightContrib;
}

float3 CalculateDirectionalLight(float3 nWorld, float4 baseColor)
{
    float3 lightDir = normalize(-d_Direction); // Assuming d_Direction points away from light
    float nDotL = saturate(dot(nWorld, lightDir));
    
    // Ambient contribution
    float3 ambient = baseColor.rgb * d_Ambient.rgb;
    
    // Diffuse contribution
    float3 diffuse = baseColor.rgb * d_Color.rgb * nDotL;
    
    return ambient + diffuse;
}

float3 CalculatePointLight(PS_input input, float4 baseColor, float3 nWorld)
{
    
    float3 lightToPixelVec = p_Position - input.worldPos.xyz;
    float distance = length(lightToPixelVec);
    
    // Early exit if beyond range
    if (distance > p_Range)
        return float3(0.0f, 0.0f, 0.0f);
        
    lightToPixelVec /= distance; // Normalize
    
    // Calculate diffuse lighting
    float nDotL = saturate(dot(nWorld,lightToPixelVec));
    
    if (nDotL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);
    
    // Calculate attenuation
    float attenuation = 1.0f / (p_Attenuation[0] +
                               (p_Attenuation[1] * distance) +
                               (p_Attenuation[2] * distance * distance));
    
    // Combine factors
    float3 lightContrib = baseColor.rgb * p_Color.rgb * nDotL * attenuation;
    
    return lightContrib;
}

float4 main(PS_input input) : SV_Target
{
    // Normalize the input normal
    input.normal = normalize(input.normal);
    
    // Sample the base texture
    float4 baseColor =  float4(1.0f, 1.0f, 1.0f, 1.0f);
    if(hasTexture)
    {
        baseColor = tex.Sample(samp, input.texCoords);
    }
    else
    {
        baseColor = difColor;

    }
    float3 nWorld;
    if(hasNormalMap)
    {
        float3 nSampleTS = normalMap.Sample(samp, input.texCoords).rgb * 2.0f - 1.0f;

    // Normalize vertex axes
        float3 T = normalize(input.tangents.xyz);
        float3 N = normalize(input.normal);
        float3 B = normalize(cross(N, T) * input.tangents.w);

        nWorld = normalize(
        nSampleTS.x * T +
        nSampleTS.y * B +
        nSampleTS.z * N
    );
        
    }
    else
    {
        nWorld = input.normal;

    }
    
    // Initialize final color (start with black, no ambient globally)
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    
    // Add contributions from each active light type
    if (d_Enable)
    {
        finalColor += CalculateDirectionalLight(nWorld, baseColor);
    }
    
    if (p_Enabled)
    {
        finalColor += CalculatePointLight(input, baseColor, nWorld);
    }
    
    if (s_Enabled)
    {
        finalColor += CalculateSpotLight(input, baseColor);
    }
    
    // Ensure we don't exceed 1.0 (optional, depending on your HDR setup)
    // finalColor = saturate(finalColor);
    
    // Return final color with original alpha
    return float4(finalColor, baseColor.a);
}
