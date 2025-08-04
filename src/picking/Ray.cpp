#include "Ray.h"

namespace DXEngine {

    Ray RayIntersection::ProjectRay(const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight)
    {
        float x = (2.0f * screenX) / screenWidth - 1.0f;
        float y = 1.0f - (2.0f * screenY) / screenHeight;

        DirectX::XMVECTOR rayClip = DirectX::XMVectorSet(x, y, -1.0f, 1.0f); //clip space

        // Transform to eye space
        DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(nullptr, camera.GetProjection());
        DirectX::XMVECTOR rayEye = DirectX::XMVector4Transform(rayClip, invProj);
        rayEye = DirectX::XMVectorSet(DirectX::XMVectorGetX(rayEye), DirectX::XMVectorGetY(rayEye), -1.0f, 0.0f);

        //Transform to wordl space
        DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, camera.GetView());
        DirectX::XMVECTOR rayWorld = DirectX::XMVector4Transform(rayEye, invView);
        rayWorld = DirectX::XMVector3Normalize(rayWorld);

        //ray origin is camera position 
        DirectX::XMVECTOR rayOrigin = DirectX::XMVectorSet(
             DirectX::XMVectorGetX(invView.r[3]),
             DirectX::XMVectorGetY(invView.r[3]),
             DirectX::XMVectorGetZ(invView.r[3]),
            1.0f
        );
        return Ray(rayOrigin, rayWorld);
    }

    HitInfo RayIntersection::IntersectSphere(const Ray& ray, const DirectX::XMVECTOR& center, DirectX::XMVECTOR& radius, void* objectPtr)
    {
        using namespace DirectX;

        DirectX::XMVECTOR oc = ray.Origin - center;

        float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray.Direction, ray.Direction));
        float b = 2.0f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(oc, ray.Direction));
        float c = DirectX::XMVectorGetX(DirectX:: XMVector3Dot(oc, oc) - radius * radius);

        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0)
            return HitInfo(); // No intersection

        float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
        float t2 = (-b + sqrt(discriminant)) / (2.0f * a);

        float t = (t1 > 0) ? t1 : t2;

        if (t <= 0)
            return HitInfo(); // Intersection behind ray origin

        DirectX::XMVECTOR hitPoint = ray.Origin + t * ray.Direction;
        DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(hitPoint - center);

        return HitInfo(true, t, hitPoint, normal, objectPtr);
    }

    HitInfo RayIntersection::IntersectPlane(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr)
    {
        using namespace DirectX;
        const float EPSILON = 0.0000001f;

        DirectX::XMVECTOR edge1 = v1 - v0;
        DirectX::XMVECTOR edge2 = v2 - v0;
        DirectX::XMVECTOR h = DirectX::XMVector3Cross(ray.Direction, edge2);
        float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge1, h));

        if (a > -EPSILON && a < EPSILON)
            return HitInfo(); // Ray is parallel to triangle

        float f = 1.0f / a;
        DirectX::XMVECTOR s = ray.Origin - v0;
        float u = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(s, h));

        if (u < 0.0f || u > 1.0f)
            return HitInfo();

        DirectX::XMVECTOR q = DirectX::XMVector3Cross(s, edge1);
        float v = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray.Direction, q));

        if (v < 0.0f || u + v > 1.0f)
            return HitInfo();

        float t = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge2, q));

        if (t > EPSILON)
        {
            DirectX::XMVECTOR hitPoint = ray.Origin + t * ray.Direction;
            DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
            return HitInfo(true, t, hitPoint, normal, objectPtr);
        }

        return HitInfo();
    }

    HitInfo RayIntersection::IntersectMesh(const Ray& ray, const std::shared_ptr<MeshResource>& mesh, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
    {
        using namespace DirectX;

        HitInfo closestHit;
        float closestDistance = FLT_MAX;

        // Transform ray to object space
        DirectX::XMMATRIX invWorldMatrix = DirectX::XMMatrixInverse(nullptr, modelMatrix);
        DirectX::XMVECTOR localOrigin = DirectX::XMVector3Transform(ray.Origin, invWorldMatrix);
        DirectX::XMVECTOR localDirection = DirectX::XMVector3TransformNormal(ray.Direction, invWorldMatrix);
        localDirection = DirectX::XMVector3Normalize(localDirection);

        Ray localRay(localOrigin, localDirection);

        std::vector<Vertex> vertices = mesh->GetVertices();
        std::vector<unsigned short> indices = mesh->GetIndices();

        // Test against all triangles
        for (size_t i = 0; i < mesh->GetIndices().size(); i += 3)
        {
            DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&vertices[indices[i]].pos);
            DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&vertices[indices[i + 1]].pos);
            DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&vertices[indices[i + 2]].pos);

            HitInfo hit = IntersectPlane(localRay, v0, v1, v2, objectPtr);

            if (hit.Hit && hit.Distance < closestDistance)
            {
                closestDistance = hit.Distance;
                closestHit = hit;

                // Transform hit point and normal back to world space
                closestHit.Point = DirectX::XMVector3Transform(hit.Point, modelMatrix);
                closestHit.Normal = DirectX::XMVector3TransformNormal(hit.Normal, modelMatrix);
                closestHit.Normal = DirectX::XMVector3Normalize(closestHit.Normal);

                // Recalculate distance in world space
                DirectX::XMVECTOR worldDistance = closestHit.Point - ray.Origin;
                closestHit.Distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(worldDistance));
            }
        }

        return closestHit;
    }

    HitInfo RayIntersection::IntersectAABB(const Ray& ray, const DirectX::XMVECTOR& minPoint, const DirectX::XMVECTOR& maxPoint, void* objectPtr)
    {
        using namespace DirectX;

        DirectX::XMVECTOR invDir = DirectX::XMVectorReciprocal(ray.Direction);
        DirectX::XMVECTOR t1 = (minPoint - ray.Origin) * invDir;
        DirectX::XMVECTOR t2 = (maxPoint - ray.Origin) * invDir;

        DirectX::XMVECTOR tMin = DirectX::XMVectorMin(t1, t2);
        DirectX::XMVECTOR tMax = DirectX::XMVectorMax(t1, t2);

        float tNear = std::max({ DirectX::XMVectorGetX(tMin),  DirectX::XMVectorGetY(tMin),  DirectX::XMVectorGetZ(tMin) });
        float tFar = std::min({ DirectX::XMVectorGetX(tMax),  DirectX::XMVectorGetY(tMax),  DirectX::XMVectorGetZ(tMax) });

        if (tNear > tFar || tFar < 0)
            return HitInfo(); // No intersection

        float t = (tNear > 0) ? tNear : tFar;
        DirectX::XMVECTOR hitPoint = ray.Origin + t * ray.Direction;

        // Calculate normal based on which face was hit
         DirectX::XMVECTOR center = (minPoint + maxPoint) * 0.5f;
         DirectX::XMVECTOR relativePos = hitPoint - center;
         DirectX::XMVECTOR size = maxPoint - minPoint;

         DirectX::XMVECTOR normal = DirectX::XMVectorZero();
        float maxComponent = 0;

        // Find the dominant axis to determine normal
        for (int i = 0; i < 3; i++)
        {
            float component = abs(DirectX::XMVectorGetByIndex(relativePos, i) / DirectX::XMVectorGetByIndex(size, i));
            if (component > maxComponent)
            {
                maxComponent = component;
                normal = DirectX::XMVectorZero();
                normal = DirectX::XMVectorSetByIndex(normal, (DirectX::XMVectorGetByIndex(relativePos, i) > 0.0f) ? 1.0f : -1.0f, i);
            }
        }

        return HitInfo(true, t, hitPoint, normal, objectPtr);
    }
}