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

VS_out main(float4 Pos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL)
{
    VS_out output;
    output.Pos = mul(Pos, WVP);
    output.worldPos = mul(Pos, Model);
    output.normal = mul(normal, Model);
    output.texCoords = inTexCoord;
    
    return output;
}