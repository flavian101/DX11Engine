// ShadowCaster.vs.hlsl
//
// The ONLY shader of the shadow pass: a depth-only vertex shader (there is no pixel shader).
//
// It reuses the constant buffers the normal vertex shaders use, so the renderer does not need a second set of uploads:
//   b0 TransformBuffer : WVP, Model, View, Projection
//   b1 BoneMatrices    : skinning palette
// The shadow pass fills them like this: View = the LIGHT's view*projection matrix, Projection = identity, so
//   WVP            = world * lightViewProj            (static meshes)
//   View*Projection = lightViewProj                   (skinned meshes: they skin to world space first)
// and both vertex paths below land in the light's clip space with no extra code.
//
// Defines (from ShaderCache::GetShadowCasterShader):
//   HAS_SKINNING_ATTRIBUTES  the mesh has bone indices/weights

#include "common.hlsli"

struct ShadowVSInput
{
    float3 position : POSITION;
#if HAS_SKINNING_ATTRIBUTES
    uint4  blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
#endif
};

float4 main(ShadowVSInput input) : SV_POSITION
{
    float4 localPos = float4(input.position, 1.0f);

#if HAS_SKINNING_ATTRIBUTES
    // EXACTLY the blend StandardVertexShader does (weights re-normalised, bone index validated), or a shadow would
    // not line up with the mesh it belongs to. The bone matrices already include the model transform.
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float totalWeight = input.blendWeights.x + input.blendWeights.y + input.blendWeights.z + input.blendWeights.w;
    float4 weights = input.blendWeights;
    if (totalWeight > 0.0001f)
        weights /= totalWeight;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (weights[i] > 0.0001f)
        {
            uint boneIndex = input.blendIndices[i];
            if (boneIndex < 128)
                skinnedPos += weights[i] * mul(localPos, BoneMatrices[boneIndex]);
        }
    }
    return mul(skinnedPos, mul(View, Projection));
#else
    return mul(localPos, WVP);
#endif
}
