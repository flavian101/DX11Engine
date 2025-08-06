#include "dxpch.h"
#include "PickingManager.h"


namespace DXEngine {

    void PickingManager::RegisterPickable(std::shared_ptr<InterfacePickable> pickable)
    {
        if (pickable)
        {
            m_Pickables.push_back(pickable);
        }
    }

    void PickingManager::UnRegisterPickable(std::shared_ptr<InterfacePickable> pickable)
    {
        m_Pickables.erase(
            std::remove_if(m_Pickables.begin(), m_Pickables.end(),
                [&pickable](const std::weak_ptr<InterfacePickable>& weak) {
                    return weak.expired() || weak.lock() == pickable;
                }),
            m_Pickables.end()
        );
    }


    void PickingManager::ClearPickables()
    {
        ClearSelection();
        m_Pickables.clear();
    }

    HitInfo PickingManager::Pick(float screenX, float screenY, int screenWidth, int screenHeight, const Camera& camera)
    {
        CleanupExpiredPointers();

        // Create ray from screen coordinates
        Ray ray = RayIntersection::ProjectRay(camera, screenX, screenY, screenWidth, screenHeight);

        HitInfo closestHit;
        float closestDistance = FLT_MAX;
        std::shared_ptr<InterfacePickable> hitObject = nullptr;

        // Test ray against all pickable objects
        for (auto& weakPickable : m_Pickables)
        {
            if (auto pickable = weakPickable.lock())
            {
                if (!pickable->IsPickable())
                    continue;

                HitInfo hit = pickable->TestRayIntersection(ray);

                if (hit.Hit && hit.Distance < closestDistance)
                {
                    closestDistance = hit.Distance;
                    closestHit = hit;
                    hitObject = pickable;
                }
            }
        }

        // Update current selection
        if (hitObject != m_CurrentPicked.lock())
        {
            // Clear previous selection
            if (auto prevPicked = m_CurrentPicked.lock())
            {
                prevPicked->OnUnpicked();
            }

            // Set new selection
            m_CurrentPicked = hitObject;
            if (hitObject)
            {
                hitObject->OnPicked();
            }
        }

        return closestHit;
    }

    void PickingManager::SetPickedObject(std::shared_ptr<InterfacePickable> pickable)
    {
        if (auto current = m_CurrentPicked.lock())
        {
            current->OnUnpicked();
        }

        m_CurrentPicked = pickable;

        if (pickable)
        {
            pickable->OnPicked();
        }
    }

    void PickingManager::ClearSelection()
    {
        if (auto current = m_CurrentPicked.lock())
        {
            current->OnUnpicked();
        }
        m_CurrentPicked.reset();
    }

    void PickingManager::CleanupExpiredPointers()
    {
        m_Pickables.erase(
            std::remove_if(m_Pickables.begin(), m_Pickables.end(),
                [](const std::weak_ptr<InterfacePickable>& weak) {
                    return weak.expired();
                }),
            m_Pickables.end()
        );
    }
}