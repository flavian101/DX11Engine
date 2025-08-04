#pragma once
#include "Ray.h"
#include <DirectXMath.h>

namespace DXEngine {

	class IPickable
	{
	public:
		virtual ~IPickable() = default;
		virtual HitInfo TestRayIntersection(const Ray& ray) = 0;

		virtual const DirectX::XMMATRIX& GetModelMatrix() const = 0;

		virtual void OnPicked() {}

		virtual void OnUnpicked() {}

		virtual bool IsPickable() const { return m_isPickable; }

		virtual void SetPickable(bool pickable) { m_isPickable = pickable; }

	protected:
		bool m_isPickable = true;
	};
}