
cbuffer cb_vsConstantBuffer 
{
    float4x4 WVP;
    float4x4 Model;
};
struct SKYMAP_VS_OUTPUT    //output structure for skymap vertex shader
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD;
};

SKYMAP_VS_OUTPUT main(float3 inPos : POSITION)
{
    SKYMAP_VS_OUTPUT output;
    //Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
    output.Pos = mul(float4(inPos,1.0f), WVP);
    output.Pos.z = output.Pos.w;
    
    output.TexCoord.x = -inPos.x;
    
    output.TexCoord.yz = inPos.yz;

    return output;
}