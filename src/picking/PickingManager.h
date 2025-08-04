#pragma once
#include "IPickable.h"
class Graphics;
class Camera;

class PickingManager
{
public:
	PickingManager() = default;

	void RegisterPickable(std::shared_ptr<IPickable> pickable);
	void UnRegisterPickable(std::shared_ptr<IPickable> pickable);

	void ClearPickables();

	HitInfo Pick(float screenX, float screenY, int screenWidth, int screenHeight, const Camera& camera);

	std::shared_ptr<IPickable> GetPickedObject() const { return m_CurrentPicked.lock(); }


	void SetPickedObject(std::shared_ptr<IPickable> pickable);
	void ClearSelection();
	void CleanupExpiredPointers();


private:
	std::vector<std::weak_ptr<IPickable>> m_Pickables;
	std::weak_ptr<IPickable> m_CurrentPicked;
};