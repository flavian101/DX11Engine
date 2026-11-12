// A transparent material is a Lit material with a blend state (see Lit.ps.hlsl and surface.hlsli).
// Blended surfaces only discard pixels that would be invisible anyway, not everything below 50% alpha.
// This file only exists until the shading-model / render-state split removes the Transparent material type.
#define ALPHA_TEST_THRESHOLD 0.1
#include "Lit.ps.hlsl"
