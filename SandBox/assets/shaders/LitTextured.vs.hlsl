cbuffer cb_vsConstantBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
};

struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float2 texCoords : TEXCOORD;
    float3 normal : NORMAL;
};

VS_out main(float3 Pos : POSITION, float3 normal : NORMAL, float2 inTexCoord : TEXCOORD)
{
    VS_out output;
 
    output.Pos = mul(float4(Pos, 1.0f), WVP);
    output.worldPos = mul(float4(Pos, 1.0f), Model);
    output.normal = mul(normal, (float3x3) Model);
    output.texCoords = inTexCoord;
    
    return output;
}