#pragma once
#include "InterfacePickable.h"
#include "Ray.h"


namespace DXEngine {
	class Graphics;
	class Camera;

	class PickingManager
	{
	public:
		PickingManager() = default;

		void RegisterPickable(std::shared_ptr<InterfacePickable> pickable);
		void UnRegisterPickable(std::shared_ptr<InterfacePickable> pickable);

		void ClearPickables();

		HitInfo Pick(float screenX, float screenY, int screenWidth, int screenHeight, const Camera& camera);

		std::shared_ptr<InterfacePickable> GetPickedObject() const { return m_CurrentPicked.lock(); }


		void SetPickedObject(std::shared_ptr<InterfacePickable> pickable);
		void ClearSelection();
		void CleanupExpiredPointers();


	private:
		std::vector<std::weak_ptr<InterfacePickable>> m_Pickables;
		std::weak_ptr<InterfacePickable> m_CurrentPicked;
	};
}