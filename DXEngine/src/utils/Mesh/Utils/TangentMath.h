#pragma once
// ============================================================================
// Tangent frames. The shader builds the bitangent as  B = cross(N, T) * w  (w = +1 or -1, the 4th component of the tangent
// attribute), so w must be chosen so that this B is the REAL bitangent of the mesh. A mesh whose UVs are mirrored on some
// islands (every character, every mirrored half) needs w = -1 there; a fixed w = +1 lights a normal map upside down on
// those islands.
//
// Convention of the engine's normal maps: OpenGL style (+Y of the map = UP in the image, which is the direction of DEcreasing
// v: the importer flips v to the top-left origin of D3D images). The bitangent therefore points along -dP/dv.
// ============================================================================
#include <DirectXMath.h>
#include <cmath>

namespace DXEngine::TangentMath
{
    // +1 when cross(n, t) points the same way as 'b', -1 when it points the other way.
    inline float Handedness(const DirectX::XMFLOAT3& n, const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT3& b)
    {
        using namespace DirectX;
        const XMVECTOR cross = XMVector3Cross(XMLoadFloat3(&n), XMLoadFloat3(&t));
        return XMVectorGetX(XMVector3Dot(cross, XMLoadFloat3(&b))) < 0.0f ? -1.0f : 1.0f;
    }

    // Some unit tangent perpendicular to 'n': used where the UVs give no direction (a triangle with collapsed UVs).
    inline DirectX::XMFLOAT3 AnyPerpendicular(const DirectX::XMFLOAT3& n)
    {
        using namespace DirectX;
        const XMVECTOR nv = XMVector3Normalize(XMLoadFloat3(&n));
        const XMVECTOR axis = std::fabs(n.x) < 0.9f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3Normalize(XMVector3Cross(nv, axis)));
        return result;
    }
}
