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

struct PS_input
{
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float2 texCoords : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

float4 main(PS_input input) : SV_Target
{
    // Normalize the input normal
    float3 normal = normalize(input.normal);
    
    // Calculate view direction (camera at origin assumption)
    float3 viewDir = normalize(-input.worldPos.xyz);
    
    // Sample base texture with scaling and offset
    float2 scaledUV = input.texCoords * textureScale + textureOffset;
    
    float4 baseColor = diffuseColor;
    if (flags & 1) // HasDiffuseTexture flag
    {
        float4 texColor = diffuseTexture.Sample(textureSampler, scaledUV);
        baseColor *= texColor;
    }
    
    // Use existing emissiveColor as the main emissive contribution
    float3 finalEmissive = emissiveColor.rgb;
    
    // Create time value from world position for animation (deterministic)
    float timeValue = input.worldPos.x + input.worldPos.y + input.worldPos.z;
    
    // Pulsing effect using shininess as frequency control
    if (flags & 2) // Use bit 2 for pulse enable
    {
        float pulse = sin(timeValue * shininess * 0.1f) * 0.5f + 0.5f;
        finalEmissive *= (0.5f + pulse * 0.5f);
    }
    
    // Rim lighting using specularColor and roughness
    float rimDot = 1.0f - saturate(dot(viewDir, normal));
    float rimPower = lerp(1.0f, 8.0f, roughness); // Use roughness to control rim falloff
    float rimEffect = pow(rimDot, rimPower);
    
    // Use specularColor for rim color and metallic as intensity
    float3 rimContribution = specularColor.rgb * rimEffect * metallic;
    
    // Distance-based effects using padding as distance scale
    if (flags & 4) // Use bit 4 for distance effects
    {
        float distance = length(input.worldPos.xyz);
        float falloff = 1.0f / (1.0f + distance * padding * 0.01f);
        finalEmissive *= falloff;
        rimContribution *= falloff;
    }
    
    // Texture-based animation using textureOffset as speed control
    if (flags & 8) // Use bit 8 for scrolling effects
    {
        float2 animatedUV = scaledUV + textureOffset * timeValue * 0.01f;
        
        if (flags & 1) // If we have a diffuse texture, sample it for animation
        {
            float animationMask = diffuseTexture.Sample(textureSampler, animatedUV).r;
            finalEmissive *= (0.7f + animationMask * 0.6f);
        }
    }
    
    // Use metallic value for intensity boosting
    finalEmissive *= (1.0f + metallic);
    
    // Noise effect using world position and roughness
    if (flags & 16) // Use bit 16 for noise
    {
        float2 noiseUV = input.worldPos.xz * roughness * 0.1f + timeValue * 0.01f;
        
        // Simple procedural noise using sin/cos
        float noise = sin(noiseUV.x * 6.28f) * cos(noiseUV.y * 6.28f);
        noise = (noise + 1.0f) * 0.5f; // Normalize to 0-1
        
        finalEmissive *= (0.8f + noise * 0.4f);
    }
    
    // Combine base color with emissive and rim effects
    float3 finalColor = baseColor.rgb * finalEmissive + rimContribution;
    
    // Use alpha value for final transparency
    return float4(finalColor, baseColor.a * alpha);
}