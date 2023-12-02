cbuffer cb_vsConstantBuffer
{
    float4x4 WVP;
    float4x4 World;
};

 struct VS_out
{
    float4 Pos : SV_POSITION;
    float2 texCoords : TEXCOORD;
};

VS_out main(float4 Pos : POSITION, float2 inTexCoord : TEXCOORD)
{
    VS_out output;
    output.Pos = mul(Pos, WVP);
    output.texCoords = inTexCoord;
    
    return output;
}