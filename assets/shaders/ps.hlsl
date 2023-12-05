struct Light
{
    float3 pointPos;
    float3 spotPos;
    float range;
    float3 dir;
    float cone;
    float3 att;
    float4 ambient;
    float4 diffuse;
};
cbuffer cbPerFrame
{
    Light light;
}; 

struct PS_input
{
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float2 texCoords : TEXCOORD;
    float3 normal : NORMAL;
};

Texture2D tex;
SamplerState samp;

float4 SpotLight(PS_input input)
{
    input.normal = normalize(input.normal);
    
    float4 diffuse = tex.Sample(samp, input.texCoords);
    
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    
    float3 lightToPixelVec = light.spotPos - input.worldPos;
    float d = length(lightToPixelVec);
    
    //add the ambient light
    float3 finalAmbient = diffuse * light.ambient;
    
    //if pixel is too far, return pixel color with ambient light
    if (d > light.range)
        return float4(finalAmbient, diffuse.a);
        
    //Turn lightToPixelVec into a unit length vector describing
    //the pixels direction from the lights position
    lightToPixelVec /= d;
        
    //Calculate how much light the pixel gets by the angle
    //in which the light strikes the pixels surface
    float howMuchLight = dot(lightToPixelVec, input.normal);

    //If light is striking the front side of the pixel
    if (howMuchLight > 0.0f)
    {
        //Add light to the finalColor of the pixel
        finalColor += diffuse * light.diffuse;
                    
        //Calculate Light's Distance Falloff factor
        finalColor /= (light.att[0] + (light.att[1] * d)) + (light.att[2] * (d * d));

        //Calculate falloff from center to edge of pointlight cone
        finalColor *= pow(max(dot(-lightToPixelVec, light.dir), 0.0f), light.cone);
    }
    
    //make sure the values are between 1 and 0, and add the ambient
    finalColor = saturate(finalColor + finalAmbient);
    
    //Return Final Color
    return float4(finalColor, diffuse.a);
    
}
float4 DirectionalLight(PS_input input)
{
    input.normal = normalize(input.normal);

    float4 diffuse = tex.Sample(samp, input.texCoords);

    float3 finalColor;

    finalColor = diffuse * light.ambient;
    finalColor += saturate(dot(light.dir, input.normal) * light.diffuse * diffuse);
	
    return float4(finalColor, diffuse.a);
    
}
float4 PointLight(PS_input input)
{
    input.normal = normalize(input.normal);
    
    float4 diffuse = tex.Sample(samp, input.texCoords);
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    
    //Create the vector between light position and pixels position
    float3 lightToPixelVec = light.pointPos - input.worldPos;
        
    //Find the distance between the light pos and pixel pos
    float d = length(lightToPixelVec);
    
    //Create the ambient light
    float3 finalAmbient = diffuse * light.ambient;

    //If pixel is too far, return pixel color with ambient light
    if (d > light.range)
        return float4(finalAmbient, diffuse.a);
        
    //Turn lightToPixelVec into a unit length vector describing
    //the pixels direction from the lights position
    lightToPixelVec /= d;
    
    //Calculate how much light the pixel gets by the angle
    //in which the light strikes the pixels surface
    float howMuchLight = dot(lightToPixelVec, input.normal);

    //If light is striking the front side of the pixel
    if (howMuchLight > 0.0f)
    {
        //Add light to the finalColor of the pixel
        finalColor += howMuchLight * diffuse * light.diffuse;
        
        //Calculate Light's Falloff factor
        finalColor /= light.att[0] + (light.att[1] * d) + (light.att[2] * (d * d));
    }
        
    //make sure the values are between 1 and 0, and add the ambient
    finalColor = saturate(finalColor + finalAmbient);
    
    //Return Final Color
    return float4(finalColor, diffuse.a);
}

float4 main(PS_input input) : SV_Target
{
   
    return SpotLight(input) + PointLight(input);
}