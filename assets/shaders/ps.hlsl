struct VS_out
{
    float4 Pos : SV_POSITION;
    float2 texCoords : TEXCOORD;
};

Texture2D tex;
SamplerState samp;

float4 main(VS_out input) : SV_Target
{
    return tex.Sample(samp, input.texCoords);
    
}