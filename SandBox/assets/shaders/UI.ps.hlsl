struct VSInput
{
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    // Sample texture if available, otherwise use vertex color
    float4 texColor = uiTexture.Sample(uiSampler, input.texCoord);
    
    // Combine texture color with vertex color
    float4 finalColor = input.color;
    
    // If texture has alpha, use it for text rendering
    if (texColor.a > 0.0f)
    {
        finalColor.rgb = input.color.rgb;
        finalColor.a = texColor.a * input.color.a;
    }
    
    return finalColor;
}