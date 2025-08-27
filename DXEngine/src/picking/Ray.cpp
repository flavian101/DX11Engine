// Ray.cpp - Fixed implementation with proper MeshResource integration
#include "dxpch.h"
#include "Ray.h"
#include "utils/mesh/Mesh.h"
#include "utils/mesh/MeshResource.h"
#include "camera/Camera.h"

namespace DXEngine {

    Ray RayIntersection::ProjectRay(const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight)
    {
        // Convert screen coordinates to normalized device coordinates [-1, 1]
        float x = (2.0f * screenX) / screenWidth - 1.0f;
        float y = 1.0f - (2.0f * screenY) / screenHeight;

        DirectX::XMVECTOR rayClip = DirectX::XMVectorSet(x, y, -1.0f, 1.0f); // clip space

        // Transform to eye space
        DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(nullptr, camera.GetProjection());
        DirectX::XMVECTOR rayEye = DirectX::XMVector4Transform(rayClip, invProj);
        rayEye = DirectX::XMVectorSet(DirectX::XMVectorGetX(rayEye), DirectX::XMVectorGetY(rayEye), -1.0f, 0.0f);

        // Transform to world space
        DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, camera.GetView());
        DirectX::XMVECTOR rayWorld = DirectX::XMVector4Transform(rayEye, invView);
        rayWorld = DirectX::XMVector3Normalize(rayWorld);

        // Ray origin is camera position 
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

        float radiusScalar = DirectX::XMVectorGetX(radius);
        float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray.Direction, ray.Direction));
        float b = 2.0f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(oc, ray.Direction));
        float c = DirectX::XMVectorGetX(DirectX::XMVector3Dot(oc, oc)) - radiusScalar * radiusScalar;

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

    HitInfo RayIntersection::IntersectTriangle(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr)
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

    HitInfo RayIntersection::IntersectMesh(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
    {
        using namespace DirectX;

        if (!meshResource || !meshResource->IsValid())
            return HitInfo();

        const VertexData* vertexData = meshResource->GetVertexData();
        const IndexData* indexData = meshResource->GetIndexData();

        if (!vertexData || vertexData->GetVertexCount() == 0)
            return HitInfo();

        // Check if vertex data has position attribute
        const VertexLayout& layout = vertexData->GetLayout();
        if (!layout.HasAttribute(VertexAttributeType::Position))
            return HitInfo();

        HitInfo closestHit;
        float closestDistance = FLT_MAX;

        // Transform ray to object space
        DirectX::XMMATRIX invWorldMatrix = DirectX::XMMatrixInverse(nullptr, modelMatrix);
        DirectX::XMVECTOR localOrigin = DirectX::XMVector3Transform(ray.Origin, invWorldMatrix);
        DirectX::XMVECTOR localDirection = DirectX::XMVector3TransformNormal(ray.Direction, invWorldMatrix);
        localDirection = DirectX::XMVector3Normalize(localDirection);

        Ray localRay(localOrigin, localDirection);

        // Test against all triangles
        if (indexData && indexData->GetIndexCount() > 0)
        {
            // Use indexed rendering
            size_t indexCount = indexData->GetIndexCount();

            // Process triangles (assuming triangle list topology)
            for (size_t i = 0; i < indexCount; i += 3)
            {
                if (i + 2 >= indexCount) break;

                uint32_t i0 = indexData->GetIndex(i);
                uint32_t i1 = indexData->GetIndex(i + 1);
                uint32_t i2 = indexData->GetIndex(i + 2);

                // Bounds check
                if (i0 >= vertexData->GetVertexCount() ||
                    i1 >= vertexData->GetVertexCount() ||
                    i2 >= vertexData->GetVertexCount())
                    continue;

                // Get vertex positions
                auto pos0 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Position);
                auto pos1 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Position);
                auto pos2 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Position);

                DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&pos0);
                DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&pos1);
                DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&pos2);

                HitInfo hit = IntersectTriangle(localRay, v0, v1, v2, objectPtr);

                if (hit.Hit && hit.Distance < closestDistance)
                {
                    closestDistance = hit.Distance;
                    closestHit = hit;
                }
            }
        }
        else
        {
            // Use non-indexed rendering (assuming triangle list)
            size_t vertexCount = vertexData->GetVertexCount();

            for (size_t i = 0; i < vertexCount; i += 3)
            {
                if (i + 2 >= vertexCount) break;

                // Get vertex positions
                auto pos0 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Position);
                auto pos1 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i + 1, VertexAttributeType::Position);
                auto pos2 = vertexData->GetAttribute<DirectX::XMFLOAT3>(i + 2, VertexAttributeType::Position);

                DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&pos0);
                DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&pos1);
                DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&pos2);

                HitInfo hit = IntersectTriangle(localRay, v0, v1, v2, objectPtr);

                if (hit.Hit && hit.Distance < closestDistance)
                {
                    closestDistance = hit.Distance;
                    closestHit = hit;
                }
            }
        }

        // Transform hit point and normal back to world space if we have a hit
        if (closestHit.Hit)
        {
            closestHit.Point = DirectX::XMVector3Transform(closestHit.Point, modelMatrix);
            closestHit.Normal = DirectX::XMVector3TransformNormal(closestHit.Normal, modelMatrix);
            closestHit.Normal = DirectX::XMVector3Normalize(closestHit.Normal);

            // Recalculate distance in world space
            DirectX::XMVECTOR worldDistance = closestHit.Point - ray.Origin;
            closestHit.Distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(worldDistance));
        }

        return closestHit;
    }

    HitInfo RayIntersection::IntersectMeshBoundingBox(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
    {
        if (!meshResource || !meshResource->IsValid())
            return HitInfo();

        const BoundingBox& localBounds = meshResource->GetBoundingBox();

        // Transform bounding box to world space
        DirectX::XMVECTOR corners[8];
        localBounds.GetCorners(corners);

        DirectX::XMVECTOR minPoint = DirectX::XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
        DirectX::XMVECTOR maxPoint = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

        // Transform all corners and find world-space AABB
        for (int i = 0; i < 8; ++i)
        {
            DirectX::XMVECTOR worldCorner = DirectX::XMVector3Transform(corners[i], modelMatrix);
            minPoint = DirectX::XMVectorMin(minPoint, worldCorner);
            maxPoint = DirectX::XMVectorMax(maxPoint, worldCorner);
        }

        return IntersectAABB(ray, minPoint, maxPoint, objectPtr);
    }

    HitInfo RayIntersection::IntersectMeshBoundingSphere(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
    {
        if (!meshResource || !meshResource->IsValid())
            return HitInfo();

        const BoundingSphere& localSphere = meshResource->GetBoundingSphere();

        // Transform sphere to world space
        DirectX::XMVECTOR worldCenter = DirectX::XMVector3Transform(
            DirectX::XMLoadFloat3(&localSphere.center), modelMatrix);

        // Scale radius by the maximum scale factor
        DirectX::XMVECTOR scale = DirectX::XMVectorSet(
            DirectX::XMVectorGetX(DirectX::XMVector3Length(modelMatrix.r[0])),
            DirectX::XMVectorGetX(DirectX::XMVector3Length(modelMatrix.r[1])),
            DirectX::XMVectorGetX(DirectX::XMVector3Length(modelMatrix.r[2])),
            1.0f
        );
        float maxScale = std::max({
            DirectX::XMVectorGetX(scale),
            DirectX::XMVectorGetY(scale),
            DirectX::XMVectorGetZ(scale)
            });

        DirectX::XMVECTOR worldRadius = DirectX::XMVectorReplicate(localSphere.radius * maxScale);

        return IntersectSphere(ray, worldCenter, worldRadius, objectPtr);
    }

    HitInfo RayIntersection::IntersectAABB(const Ray& ray, const DirectX::XMVECTOR& minPoint, const DirectX::XMVECTOR& maxPoint, void* objectPtr)
    {
        using namespace DirectX;

        // Handle potential division by zero
        DirectX::XMVECTOR invDir = DirectX::XMVectorReciprocal(ray.Direction);

        // Replace infinities with very large numbers
        DirectX::XMVECTOR largeValue = DirectX::XMVectorReplicate(FLT_MAX);
        DirectX::XMVECTOR mask = DirectX::XMVectorEqual(ray.Direction, DirectX::XMVectorZero());
        invDir = DirectX::XMVectorSelect(invDir, largeValue, mask);

        DirectX::XMVECTOR t1 = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(minPoint, ray.Origin), invDir);
        DirectX::XMVECTOR t2 = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(maxPoint, ray.Origin), invDir);

        DirectX::XMVECTOR tMin = DirectX::XMVectorMin(t1, t2);
        DirectX::XMVECTOR tMax = DirectX::XMVectorMax(t1, t2);

        float tNear = std::max({
            DirectX::XMVectorGetX(tMin),
            DirectX::XMVectorGetY(tMin),
            DirectX::XMVectorGetZ(tMin)
            });

        float tFar = std::min({
            DirectX::XMVectorGetX(tMax),
            DirectX::XMVectorGetY(tMax),
            DirectX::XMVectorGetZ(tMax)
            });

        if (tNear > tFar || tFar < 0)
            return HitInfo(); // No intersection

        float t = (tNear > 0) ? tNear : tFar;
        DirectX::XMVECTOR hitPoint = DirectX::XMVectorAdd(ray.Origin, DirectX::XMVectorScale(ray.Direction, t));

        // Calculate normal based on which face was hit
        DirectX::XMVECTOR center = DirectX::XMVectorScale(DirectX::XMVectorAdd(minPoint, maxPoint), 0.5f);
        DirectX::XMVECTOR relativePos = DirectX::XMVectorSubtract(hitPoint, center);
        DirectX::XMVECTOR size = DirectX::XMVectorSubtract(maxPoint, minPoint);

        DirectX::XMVECTOR normal = DirectX::XMVectorZero();
        float maxComponent = 0;

        // Find the dominant axis to determine normal
        for (int i = 0; i < 3; i++)
        {
            float sizeComponent = DirectX::XMVectorGetByIndex(size, i);
            if (sizeComponent > 0.0001f) // Avoid division by zero
            {
                float component = abs(DirectX::XMVectorGetByIndex(relativePos, i) / sizeComponent);
                if (component > maxComponent)
                {
                    maxComponent = component;
                    normal = DirectX::XMVectorZero();
                    float normalValue = (DirectX::XMVectorGetByIndex(relativePos, i) > 0.0f) ? 1.0f : -1.0f;
                    normal = DirectX::XMVectorSetByIndex(normal, normalValue, i);
                }
            }
        }

        return HitInfo(true, t, hitPoint, normal, objectPtr);
    }

    // Helper function for optimized mesh intersection using hierarchical testing
    HitInfo RayIntersection::IntersectMeshOptimized(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr)
    {
        if (!meshResource || !meshResource->IsValid())
            return HitInfo();

        // First test against bounding sphere for quick rejection
        HitInfo sphereHit = IntersectMeshBoundingSphere(ray, meshResource, modelMatrix, nullptr);
        if (!sphereHit.Hit)
            return HitInfo();

        // Then test against bounding box for better culling
        HitInfo boxHit = IntersectMeshBoundingBox(ray, meshResource, modelMatrix, nullptr);
        if (!boxHit.Hit)
            return HitInfo();

        // Finally do precise mesh intersection
        return IntersectMesh(ray, meshResource, modelMatrix, objectPtr);
    }
}