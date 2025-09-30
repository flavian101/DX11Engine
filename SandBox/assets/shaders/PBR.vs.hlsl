#include "common.hlsli"

StandardVertexOutput main(StandardVertexInput input)
{
    // Use standard vertex shader transformation
    return StandardVertexShader(input);
}
