cbuffer UIConstants : register(b0)
{
    matrix projection;
    float screenWidth;
    float screenHeight;
    float time;
    float padding;
};

struct VSInput
{
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    // Convert screen space coordinates to normalized device coordinates
    float2 screenPos;
    screenPos.x = (input.position.x / screenWidth) * 2.0f - 1.0f;
    screenPos.y = -((input.position.y / screenHeight) * 2.0f - 1.0f); // Flip Y
    
    output.position = float4(screenPos, 0.0f, 1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    
    return output;
}
