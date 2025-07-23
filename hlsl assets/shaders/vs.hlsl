cbuffer cb_vsConstantBuffer
{
    float4x4 WVP;
    float4x4 Model;
};

struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoords : TEXCOORD2;
};

VS_out main(float4 Pos : POSITION, float3 normal : NORMAL, float2 inTexCoord : TEXCOORD)
{
    VS_out output;
    output.Pos = mul(float4(Pos), WVP);
    output.worldPos = mul(float4(Pos), Model);
    output.normal = mul(normal, (float3x3)Model);
    output.texCoords = inTexCoord;
    return output;
}