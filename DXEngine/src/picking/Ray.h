// Ray.h - Updated header with proper MeshResource integration
#pragma once
#include <DirectXMath.h>
#include <memory>

namespace DXEngine {

    class MeshResource;
    class Camera;

    struct Ray
    {
        DirectX::XMVECTOR Origin;
        DirectX::XMVECTOR Direction;

        Ray() = default;
        Ray(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& direction)
            : Origin(origin), Direction(direction)
        {
        }
    };

    struct HitInfo
    {
        bool Hit = false;
        float Distance = FLT_MAX;
        DirectX::XMVECTOR Point = DirectX::XMVectorZero();
        DirectX::XMVECTOR Normal = DirectX::XMVectorZero();
        void* ObjectPtr = nullptr;

        HitInfo() = default;
        HitInfo(bool hit, float distance, const DirectX::XMVECTOR& point, const DirectX::XMVECTOR& normal, void* objectPtr = nullptr)
            : Hit(hit), Distance(distance), Point(point), Normal(normal), ObjectPtr(objectPtr)
        {
        }
    };

    class RayIntersection
    {
    public:
        // Ray projection from screen coordinates
        static Ray ProjectRay(const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight);

        // Basic primitive intersection tests
        static HitInfo IntersectSphere(const Ray& ray, const DirectX::XMVECTOR& center, DirectX::XMVECTOR& radius, void* objectPtr = nullptr);

        static HitInfo IntersectTriangle(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr = nullptr);

        static HitInfo IntersectAABB(const Ray& ray, const DirectX::XMVECTOR& minPoint, const DirectX::XMVECTOR& maxPoint, void* objectPtr = nullptr);

        // Mesh intersection methods
        static HitInfo IntersectMesh(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr = nullptr);

        // Bounding volume intersection methods for meshes
        static HitInfo IntersectMeshBoundingBox(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr = nullptr);

        static HitInfo IntersectMeshBoundingSphere(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr = nullptr);

        // Optimized mesh intersection with hierarchical testing
        static HitInfo IntersectMeshOptimized(const Ray& ray, const std::shared_ptr<MeshResource>& meshResource, const DirectX::XMMATRIX& modelMatrix, void* objectPtr = nullptr);

        // Legacy method name for backward compatibility
        static HitInfo IntersectPlane(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr = nullptr)
        {
            return IntersectTriangle(ray, v0, v1, v2, objectPtr);
        }
    };
}