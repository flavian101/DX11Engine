// Standard.vs.hlsl: the vertex shader of every model that reads the standard vertex layout (Unlit, Lit, Emissive).
// It used to exist as Lit.vs / PBR.vs / Emissive.vs / Transparent.vs (identical) and a hand-copied Unlit.vs that had no
// skinning, so a skinned mesh with an unlit material could not animate.
#include "common.hlsli"

StandardVertexOutput main(StandardVertexInput input)
{
    return StandardVertexShader(input);
}
