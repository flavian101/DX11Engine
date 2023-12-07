
struct SKYMAP_VS_OUTPUT    //output structure for skymap vertex shader
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD;
};

TextureCube<float4> SkyMap : register(t1);
SamplerState samp;

float4 main(SKYMAP_VS_OUTPUT inPS) : SV_Target
{
    
    
    return SkyMap.Sample(samp, inPS.TexCoord);
}
