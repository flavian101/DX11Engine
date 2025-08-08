#pragma once
#include "dxpch.h"
#include "Ray.h"
#include <DirectXMath.h>

namespace DXEngine {

	class InterfacePickable
	{
	public:
		InterfacePickable() = default;
		virtual ~InterfacePickable() = default;

		virtual HitInfo TestRayIntersection(const Ray& ray) = 0; 

		virtual DirectX::XMMATRIX GetModelMatrix() const = 0;

		virtual void OnPicked() = 0;

		virtual void OnUnpicked() = 0;

		virtual bool IsPickable() const { return m_isPickable; }

		virtual void SetPickable(bool pickable) { m_isPickable = pickable; }

	protected:
		bool m_isPickable = true;
	};
}