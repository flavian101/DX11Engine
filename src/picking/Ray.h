#pragma once
#include <DirectXMath.h>
#include "utils/Mesh.h"


namespace DXEngine {


	struct Ray
	{
		DirectX::XMVECTOR Origin;
		DirectX::XMVECTOR Direction;

		Ray() = default;
		Ray(const DirectX::XMVECTOR& origin, DirectX::XMVECTOR& direction)
			:Origin(origin), Direction(direction)
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
		HitInfo(bool hit, float distance, const DirectX::XMVECTOR& point, const DirectX::XMVECTOR& normal,
			void* objectPtr)
			:
			Hit(hit), Distance(distance), Point(point), Normal(normal), ObjectPtr(objectPtr) {}
	};

	class RayIntersection
	{
	public:
		static Ray ProjectRay(const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight);

		static HitInfo IntersectSphere(const Ray& ray, const DirectX::XMVECTOR& center, DirectX::XMVECTOR& radius, void* objectPtr = nullptr);

		static HitInfo IntersectPlane(const Ray& ray, const DirectX::XMVECTOR& v0, const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2, void* objectPtr = nullptr);

		static HitInfo IntersectMesh(const Ray& ray, const std::shared_ptr<MeshResource>& mesh, const DirectX::XMMATRIX& modelMatrix, void* objectPtr = nullptr);

		static HitInfo IntersectAABB(const Ray& ray, const DirectX::XMVECTOR& minPoint, const DirectX::XMVECTOR& maxPoint, void* objectPtr = nullptr);



	};
}