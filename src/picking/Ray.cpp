#include "Ray.h"

Ray RayIntersection::ProjectRay(const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight)
{
    float x = (2.0f * screenX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * screenY) / screenHeight;

    XMVECTOR rayClip = XMVectorSet(x, y, -1.0f, 1.0f); //clip space

    // Transform to eye space
    XMMATRIX invProj = XMMatrixInverse(nullptr, camera.GetProjection());
    XMVECTOR rayEye = XMVector4Transform(rayClip, invProj);
    rayEye = XMVectorSet(XMVectorGetX(rayEye), XMVectorGetY(rayEye), -1.0f, 0.0f);

    //Transform to wordl space
    XMMATRIX invView = XMMatrixInverse(nullptr, camera.GetView());
    XMVECTOR rayWorld = XMVector4Transform(rayEye, invView);
    rayWorld = XMVector3Normalize(rayWorld);

    //ray origin is camera position 
    XMVECTOR rayOrigin = XMVectorSet(
        XMVectorGetX(invView.r[3]),
        XMVectorGetY(invView.r[3]),
        XMVectorGetZ(invView.r[3]),
        1.0f
    );
    return Ray(rayOrigin, rayWorld);
}

HitInfo RayIntersection::IntersectSphere(const Ray& ray, const DirectX::XMVECTOR& center, DirectX::XMVECTOR& radius, void* objectPtr)
{
    XMVECTOR oc = ray.Origin - center;

    float a = XMVectorGetX(XMVector3Dot(ray.Direction, ray.Direction));
    float b = 2.0f * XMVectorGetX(XMVector3Dot(oc, ray.Direction));
    float c = XMVectorGetX(XMVector3Dot(oc, oc) - radius * radius);

    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
        return HitInfo(); // No intersection

    float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
    float t2 = (-b + sqrt(discriminant)) / (2.0f * a);

    float t = (t1 > 0) ? t1 : t2;

    if (t <= 0)
        return HitInfo(); // Intersection behind ray origin

    XMVECTOR hitPoint = ray.Origin + t * ray.Direction;
    XMVECTOR normal = XMVector3Normalize(hitPoint - center);

    return HitInfo(true, t, hitPoint, normal, objectPtr);
}

HitInfo RayIntersection::IntersectPlane(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr)
{
    const float EPSILON = 0.0000001f;

    XMVECTOR edge1 = v1 - v0;
    XMVECTOR edge2 = v2 - v0;
    XMVECTOR h = XMVector3Cross(ray.Direction, edge2);
    float a = XMVectorGetX(XMVector3Dot(edge1, h));

    if (a > -EPSILON && a < EPSILON)
        return HitInfo(); // Ray is parallel to triangle

    float f = 1.0f / a;
    XMVECTOR s = ray.Origin - v0;
    float u = f * XMVectorGetX(XMVector3Dot(s, h));

    if (u < 0.0f || u > 1.0f)
        return HitInfo();

    XMVECTOR q = XMVector3Cross(s, edge1);
    float v = f * XMVectorGetX(XMVector3Dot(ray.Direction, q));

    if (v < 0.0f || u + v > 1.0f)
        return HitInfo();

    float t = f * XMVectorGetX(XMVector3Dot(edge2, q));

    if (t > EPSILON)
    {
        XMVECTOR hitPoint = ray.Origin + t * ray.Direction;
        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));
        return HitInfo(true, t, hitPoint, normal, objectPtr);
    }

    return HitInfo();
}

HitInfo RayIntersection::IntersectMesh(const Ray& ray,const std::shared_ptr<MeshResource>& mesh, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
{
    HitInfo closestHit;
    float closestDistance = FLT_MAX;

    // Transform ray to object space
    XMMATRIX invWorldMatrix = XMMatrixInverse(nullptr, modelMatrix);
    XMVECTOR localOrigin = XMVector3Transform(ray.Origin, invWorldMatrix);
    XMVECTOR localDirection = XMVector3TransformNormal(ray.Direction, invWorldMatrix);
    localDirection = XMVector3Normalize(localDirection);

    Ray localRay(localOrigin, localDirection);

    std::vector<Vertex> vertices = mesh->GetVertices();
    std::vector<unsigned short> indices = mesh->GetIndices();

    // Test against all triangles
    for (size_t i = 0; i < mesh->GetIndices().size(); i += 3)
    {
        XMVECTOR v0 = XMLoadFloat3(&vertices[indices[i]].pos);
        XMVECTOR v1 = XMLoadFloat3(&vertices[indices[i + 1]].pos);
        XMVECTOR v2 = XMLoadFloat3(&vertices[indices[i + 2]].pos);

        HitInfo hit = IntersectPlane(localRay, v0, v1, v2, objectPtr);

        if (hit.Hit && hit.Distance < closestDistance)
        {
            closestDistance = hit.Distance;
            closestHit = hit;

            // Transform hit point and normal back to world space
            closestHit.Point = XMVector3Transform(hit.Point, modelMatrix);
            closestHit.Normal = XMVector3TransformNormal(hit.Normal, modelMatrix);
            closestHit.Normal = XMVector3Normalize(closestHit.Normal);

            // Recalculate distance in world space
            XMVECTOR worldDistance = closestHit.Point - ray.Origin;
            closestHit.Distance = XMVectorGetX(XMVector3Length(worldDistance));
        }
    }

    return closestHit;
}

HitInfo RayIntersection::IntersectAABB(const Ray& ray, const DirectX::XMVECTOR& minPoint, const DirectX::XMVECTOR& maxPoint, void* objectPtr)
{
    XMVECTOR invDir = XMVectorReciprocal(ray.Direction);
    XMVECTOR t1 = (minPoint - ray.Origin) * invDir;
    XMVECTOR t2 = (maxPoint - ray.Origin) * invDir;

    XMVECTOR tMin = XMVectorMin(t1, t2);
    XMVECTOR tMax = XMVectorMax(t1, t2);

    float tNear = std::max({ XMVectorGetX(tMin), XMVectorGetY(tMin), XMVectorGetZ(tMin) });
    float tFar = std::min({ XMVectorGetX(tMax), XMVectorGetY(tMax), XMVectorGetZ(tMax) });

    if (tNear > tFar || tFar < 0)
        return HitInfo(); // No intersection

    float t = (tNear > 0) ? tNear : tFar;
    XMVECTOR hitPoint = ray.Origin + t * ray.Direction;

    // Calculate normal based on which face was hit
    XMVECTOR center = (minPoint + maxPoint) * 0.5f;
    XMVECTOR relativePos = hitPoint - center;
    XMVECTOR size = maxPoint - minPoint;

    XMVECTOR normal = XMVectorZero();
    float maxComponent = 0;

    // Find the dominant axis to determine normal
    for (int i = 0; i < 3; i++)
    {
        float component = abs(XMVectorGetByIndex(relativePos, i) / XMVectorGetByIndex(size, i));
        if (component > maxComponent)
        {
            maxComponent = component;
            normal = XMVectorZero();
            normal = XMVectorSetByIndex(normal, (XMVectorGetByIndex(relativePos, i) > 0.0f) ? 1.0f : -1.0f, i);
        }
    }

    return HitInfo(true, t, hitPoint, normal, objectPtr);
}
