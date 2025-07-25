cbuffer PointLightData : register(b2)
{
    float3 p_Position;
    float p_Range;
    float4 p_Color;
    float3 p_Attenuation;
    int p_Enabled;
};

float4 main() : SV_Target
{
    return p_Color;
}