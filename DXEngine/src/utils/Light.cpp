#include "dxpch.h"
#include "Light.h"
#include <algorithm>
#include "renderer/RendererCommand.h"

namespace DXEngine {

	void DirectionalLight::UpdateGPUData()
	{
		if (m_Dirty)
		{
			m_GPUData.direction = m_Direction;
			m_GPUData.color = m_Color;
			m_GPUData.intensity = m_Enabled ? m_Intensity : 0.0f;
			m_GPUData.shadowMapIndex = m_CastShadows ? 0.0f : -1.0f; //set by the shadow system

			// Initialize cascade splits for CSM
			float cascadeSplits[4] = { 0.1f, 0.25f, 0.5f, 1.0f };
			m_GPUData.cascadeSplits = DirectX::XMFLOAT4(cascadeSplits[0], cascadeSplits[1],
				cascadeSplits[2], cascadeSplits[3]);

			m_Dirty = false;
		}
	}

    void PointLight::UpdateGPUData()
    {
        if (!m_Dirty) return;

        m_GPUData.position = m_Position;
        m_GPUData.color = m_Color;
        m_GPUData.intensity = m_Enabled ? m_Intensity : 0.0f;
        m_GPUData.radius = m_Radius;
        m_GPUData.attenuation = m_Attenuation;
        m_GPUData.shadowMapIndex = m_CastShadows ? 0.0f : -1.0f;

        m_Dirty = false;
    }

    bool PointLight::IsInFrustum(const DirectX::BoundingFrustum& frustum) const
    {
        DirectX::BoundingSphere lightSphere(m_Position, m_Radius);
        return frustum.Intersects(lightSphere);
    }

    // SpotLight Implementation
    void SpotLight::UpdateGPUData()
    {
        if (!m_Dirty) return;

        m_GPUData.position = m_Position;
        m_GPUData.direction = m_Direction;
        m_GPUData.color = m_Color;
        m_GPUData.intensity = m_Enabled ? m_Intensity : 0.0f;
        m_GPUData.range = m_Range;
        m_GPUData.innerCone = cosf(m_InnerCone);
        m_GPUData.outerCone = cosf(m_OuterCone);
        m_GPUData.attenuation = m_Attenuation;
        m_GPUData.shadowMapIndex = m_CastShadows ? 0.0f : -1.0f;

        m_Dirty = false;
    }

    bool SpotLight::IsInFrustum(const DirectX::BoundingFrustum& frustum) const
    {
        // Simplified frustum check - use bounding sphere for now
        DirectX::BoundingSphere lightSphere(m_Position, m_Range);
        return frustum.Intersects(lightSphere);
    }

    // Light Manager Implementation

    LightManager::LightManager()
    {
        //initialize default scene
        m_SceneData.directionalLightCount = 0;
        m_SceneData.pointLightCount = 0;
        m_SceneData.spotLightCount = 0;
        m_SceneData.ambientColor = { 0.4f,0.4f,0.4f };
        m_SceneData.ambientIntensity = 0.3f;
        m_SceneData.iblIntensity = 1.0f;
        m_SceneData.exposure = 0.8f;
        m_SceneData.gamma = 2.2f;

        // Reserve space for lights
        m_DirectionalLights.reserve(SceneLightData::MAX_DIRECTIONAL_LIGHTS);
        m_PointLights.reserve(SceneLightData::MAX_POINT_LIGHTS);
        m_SpotLights.reserve(SceneLightData::MAX_SPOT_LIGHTS);

        m_VisiblePointLights.reserve(SceneLightData::MAX_POINT_LIGHTS);
        m_VisibleSpotLights.reserve(SceneLightData::MAX_SPOT_LIGHTS);


    }
    std::shared_ptr<DirectionalLight> LightManager::CreateDirectionalLight()
    {
        if (m_DirectionalLights.size() >= SceneLightData::MAX_DIRECTIONAL_LIGHTS)
        {
            OutputDebugStringA("warning: Maximum directional Lights reached\n");
            return nullptr;
        }

        auto light = std::make_shared<DirectionalLight>();
        m_DirectionalLights.push_back(light);
        m_Dirty = true;
        return light;
    }

    std::shared_ptr<PointLight> LightManager::CreatePointLight()
    {
        if (m_PointLights.size() >= SceneLightData::MAX_POINT_LIGHTS)
        {
            OutputDebugStringA("Warning: Maximum point lights reached\n");
            return nullptr;
        }

        auto light = std::make_shared<PointLight>();
        m_PointLights.push_back(light);
        m_Dirty = true;
        return light;
    }
    std::shared_ptr<SpotLight> LightManager::CreateSpotLight()
    {
        if (m_SpotLights.size() >= SceneLightData::MAX_SPOT_LIGHTS)
        {
            OutputDebugStringA("Warning: Maximum spot lights reached\n");
            return nullptr;
        }

        auto light = std::make_shared<SpotLight>();
        m_SpotLights.push_back(light);
        m_Dirty = true;
        return light;
    }
    void LightManager::RemoveLight(std::shared_ptr<Light> light)
    {
        if (!light) return;

        switch (light->GetType())
        {
        case Light::Type::Directional:
            m_DirectionalLights.erase(
                std::remove_if(m_DirectionalLights.begin(), m_DirectionalLights.end(),
                    [&](const std::weak_ptr<DirectionalLight>& weak) {
                        return weak.lock() == light;
                    }),
                m_DirectionalLights.end());
            break;

        case Light::Type::Point:
            m_PointLights.erase(
                std::remove_if(m_PointLights.begin(), m_PointLights.end(),
                    [&](const std::weak_ptr<PointLight>& weak) {
                        return weak.lock() == light;
                    }),
                m_PointLights.end());
            break;

        case Light::Type::Spot:
            m_SpotLights.erase(
                std::remove_if(m_SpotLights.begin(), m_SpotLights.end(),
                    [&](const std::weak_ptr<SpotLight>& weak) {
                        return weak.lock() == light;
                    }),
                m_SpotLights.end());
            break;
        }

        m_Dirty = true;

    }
    void LightManager::SetAmbientLight(const DirectX::XMFLOAT3& color, float intensity)
    {
        m_SceneData.ambientColor = color;
        m_SceneData.ambientIntensity = intensity;
        m_Dirty = true;
    }
    void LightManager::CullLights(const DirectX::BoundingFrustum& frustum)
    {
        ///clear previous frame's  visible lights
        m_VisiblePointLights.clear();
        m_VisibleSpotLights.clear();

        //cull point lights
        for (uint32_t i = 0; i < m_PointLights.size(); i++)
        {
            if (m_PointLights[i]->IsEnabled() && m_PointLights[i]->IsInFrustum(frustum))
            {
                m_VisiblePointLights.push_back(i);
            }
        }

        //cull spotLight
        for (uint32_t i = 0; i < m_SpotLights.size(); i++)
        {
            if (m_SpotLights[i]->IsEnabled() && m_SpotLights[i]->IsInFrustum(frustum))
            {
                m_VisibleSpotLights.push_back(i);
            }
        }

        //sort lightd by distance 
        auto camera = RenderCommand::GetCamera();
        if (camera)
        {
            DirectX::XMVECTOR cameraPos = camera->GetPos();

            //sort point ligts by distance
            std::sort(m_VisiblePointLights.begin(), m_VisiblePointLights.end(),
                [&](uint32_t a, uint32_t b)
                {
                    DirectX::XMVECTOR posA = DirectX::XMLoadFloat3(&m_PointLights[a]->GetPosition());
                    DirectX::XMVECTOR posB = DirectX::XMLoadFloat3(&m_PointLights[b]->GetPosition());
                    float distA = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(posA, cameraPos)));
                    float distB = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(posB, cameraPos)));
                    return distA < distB;
                });

            // Sort spot lights by distance
            std::sort(m_VisibleSpotLights.begin(), m_VisibleSpotLights.end(),
                [&](uint32_t a, uint32_t b) {
                    DirectX::XMVECTOR posA = DirectX::XMLoadFloat3(&m_SpotLights[a]->GetPosition());
                    DirectX::XMVECTOR posB = DirectX::XMLoadFloat3(&m_SpotLights[b]->GetPosition());
                    float distA = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(posA, cameraPos)));
                    float distB = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(posB, cameraPos)));
                    return distA < distB;
                });
        }

    }
    void LightManager::UpdateLightData()
    {
        bool needsUpdate = m_Dirty;
        //check of any lights are dirty
        for (const auto& light : m_DirectionalLights)
        {
            if (light->IsDirty())
            {
                light->UpdateGPUData();
                needsUpdate = true;
            }
        }

        for (const auto& light : m_PointLights)
        {
            if (light->IsDirty())
            {
                light->UpdateGPUData();
                needsUpdate = true;
            }
        }
        for (const auto& light : m_SpotLights)
        {
            if (light->IsDirty())
            {
                light->UpdateGPUData();
                needsUpdate = true;
            }
        }

        if (needsUpdate)
        {
            UpdateSceneLightData();
            m_Dirty = false;
        }
    }
    void LightManager::UpdateSceneLightData()
    {
        //uodate light counts
        m_SceneData.directionalLightCount = static_cast<uint32_t>(m_DirectionalLights.size());
        m_SceneData.pointLightCount = static_cast<uint32_t>(m_VisiblePointLights.size());
        m_SceneData.spotLightCount = static_cast<uint32_t>(m_VisibleSpotLights.size());

        //copy directional Lights (always visible)
        for (size_t i = 0; i < m_DirectionalLights.size(); i++)
        {
            m_SceneData.directionalLights[i] = m_DirectionalLights[i]->GetGPUData();
        }

        // Copy visible point lights
        for (size_t i = 0; i < m_VisiblePointLights.size(); ++i)
        {
            uint32_t lightIndex = m_VisiblePointLights[i];
            m_SceneData.pointLights[i] = m_PointLights[lightIndex]->GetGPUData();
        }

        // Copy visible spot lights
        for (size_t i = 0; i < m_VisibleSpotLights.size(); ++i)
        {
            uint32_t lightIndex = m_VisibleSpotLights[i];
            m_SceneData.spotLights[i] = m_SpotLights[lightIndex]->GetGPUData();
        }


    }
    void LightManager::BindLightData()
    {
        if (!m_BufferInitialized)
        {
            m_LightBuffer.Initialize(&m_SceneData);
            m_BufferInitialized = true;
        }

        m_LightBuffer.Update(m_SceneData);
        RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Scene_Lights, 1, m_LightBuffer.GetAddressOf());
    }
    uint32_t LightManager::GetVisibleLightCount() const
    {
        return static_cast<uint32_t>(m_DirectionalLights.size() + m_VisiblePointLights.size() + m_VisibleSpotLights.size());
    }
    std::string LightManager::GetDebugInfo() const
    {
        std::string info = "=== Light System Debug ===\n";
        info += "Directional Lights: " + std::to_string(m_DirectionalLights.size()) + "\n";
        info += "Point Lights: " + std::to_string(m_PointLights.size()) + " (Visible: " + std::to_string(m_VisiblePointLights.size()) + ")\n";
        info += "Spot Lights: " + std::to_string(m_SpotLights.size()) + " (Visible: " + std::to_string(m_VisibleSpotLights.size()) + ")\n";
        info += "Total Visible: " + std::to_string(GetVisibleLightCount()) + "\n";
        info += "Ambient: (" + std::to_string(m_SceneData.ambientColor.x) + ", "
            + std::to_string(m_SceneData.ambientColor.y) + ", "
            + std::to_string(m_SceneData.ambientColor.z) + ") * "
            + std::to_string(m_SceneData.ambientIntensity) + "\n";
        return info;
    }

}