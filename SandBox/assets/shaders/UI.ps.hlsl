Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

cbuffer MaterialBuffer : register(b1)
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
    float3 materialPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 finalColor = input.color;
    
    // Check if we have a diffuse texture (flag bit 0)
    if (flags & 1)
    {
        float4 texColor = uiTexture.Sample(uiSampler, input.texCoord);
        
        // For UI, we typically want to modulate the texture with the color
        finalColor = finalColor * texColor;
    }
    
    // Apply material alpha
    finalColor.a *= alpha;
    
    // Ensure we don't exceed valid color ranges
    finalColor = saturate(finalColor);
    
    return finalColor;
}