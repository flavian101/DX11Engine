#pragma once
#include "Buffer.h"

#include <DirectXMath.h>
#include <DirectXCollision.h>


namespace DXEngine {

	class Light
	{
	public:
		enum class Type { Directional, Point, Spot };

		Light(Type type) : m_Type(type), m_Enabled(true), m_CastShadows(false) {}
		virtual ~Light() = default;

		//core properties
		Type GetType()const { return m_Type; }
		bool IsEnabled()const { return m_Enabled; }
		void SetEnabled(bool enable) { m_Enabled = enable; }

		const DirectX::XMFLOAT3& GetColor() const { return m_Color; }
		void SetColor(const DirectX::XMFLOAT3& color) { m_Color = color; m_Dirty = true; }

		float GetIntensity() const { return m_Intensity; }
		void SetIntensity(float intensity) { m_Intensity = intensity; m_Dirty = true; }

		bool CastsShadows() const { return m_CastShadows; }
		void SetCastShadows(bool castShadows) { m_CastShadows = castShadows; m_Dirty = true; }

		bool IsDirty() const { return m_Dirty; }
		void ClearDirty() { m_Dirty = false; }

		virtual void UpdateGPUData() = 0;
		virtual float GetBoundingRadius() const = 0;
		virtual bool IsInFrustum(const DirectX::BoundingFrustum& frustum) const = 0;


	protected:
		Type m_Type;
		bool m_Enabled;
		bool m_CastShadows;
		bool m_Dirty = true;

		DirectX::XMFLOAT3 m_Color = { 1.0f, 1.0f, 1.0f };
		float m_Intensity = 1.0f;
	};

	

	class DirectionalLight : public Light
	{
	public:
		DirectionalLight() : Light(Type::Directional) {}

		const DirectX::XMFLOAT3& GetDirection() const { return m_Direction; }
		void SetDirection(const DirectX::XMFLOAT3& direction)
		{
			m_Direction = direction;
			DirectX::XMStoreFloat3(&m_Direction, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction)));
			m_Dirty = true;
		}

		// CSM support
		void SetCascadeCount(uint32_t count) { m_CascadeCount = std::min(count, 4u); m_Dirty = true; }
		uint32_t GetCascadeCount() const { return m_CascadeCount; }

		void UpdateGPUData() override;
		float GetBoundingRadius() const override { return FLT_MAX; } // Infinite range
		bool IsInFrustum(const DirectX::BoundingFrustum& frustum) const override { return true; }

		DirectionalLightGPU& GetGPUData() { return m_GPUData; }

	private:
		DirectX::XMFLOAT3 m_Direction = { 0.0f, -1.0f, 0.0f };
		uint32_t m_CascadeCount = 4;
		DirectionalLightGPU m_GPUData;
	};

	class PointLight : public Light
	{
	public:
		PointLight() : Light(Type::Point) {}

		const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
		void SetPosition(const DirectX::XMFLOAT3& position) { m_Position = position; m_Dirty = true; }

		float GetRadius() const { return m_Radius; }
		void SetRadius(float radius) { m_Radius = radius; m_Dirty = true; }

		const DirectX::XMFLOAT3& GetAttenuation() const { return m_Attenuation; }
		void SetAttenuation(const DirectX::XMFLOAT3& attenuation) { m_Attenuation = attenuation; m_Dirty = true; }

		void UpdateGPUData() override;
		float GetBoundingRadius() const override { return m_Radius; }
		bool IsInFrustum(const DirectX::BoundingFrustum& frustum) const override;

		PointLightGPU& GetGPUData() { return m_GPUData; }

	private:
		DirectX::XMFLOAT3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Radius = 10.0f;
		DirectX::XMFLOAT3 m_Attenuation = { 1.0f, 0.09f, 0.032f }; // Modern attenuation values
		PointLightGPU m_GPUData;
	};

	class SpotLight : public Light
	{
	public:
		SpotLight() : Light(Type::Spot) {}

		const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
		void SetPosition(const DirectX::XMFLOAT3& position) { m_Position = position; m_Dirty = true; }

		const DirectX::XMFLOAT3& GetDirection() const { return m_Direction; }
		void SetDirection(const DirectX::XMFLOAT3& direction)
		{
			DirectX::XMStoreFloat3(&m_Direction, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction)));
			m_Dirty = true;
		}

		float GetInnerCone() const { return m_InnerCone; }
		void SetInnerCone(float angle) { m_InnerCone = angle; m_Dirty = true; }

		float GetOuterCone() const { return m_OuterCone; }
		void SetOuterCone(float angle) { m_OuterCone = angle; m_Dirty = true; }

		float GetRange() const { return m_Range; }
		void SetRange(float range) { m_Range = range; m_Dirty = true; }

		void UpdateGPUData() override;
		float GetBoundingRadius() const override { return m_Range; }
		bool IsInFrustum(const DirectX::BoundingFrustum& frustum) const override;

		SpotLightGPU& GetGPUData() { return m_GPUData; }

	private:
		DirectX::XMFLOAT3 m_Position = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 m_Direction = { 0.0f, -1.0f, 0.0f };
		float m_Range = 10.0f;
		float m_InnerCone = DirectX::XM_PI / 8.0f; // 22.5 degrees
		float m_OuterCone = DirectX::XM_PI / 4.0f; // 45 degrees
		DirectX::XMFLOAT3 m_Attenuation = { 1.0f, 0.09f, 0.032f };
		SpotLightGPU m_GPUData;
	};

	class LightManager
	{
	public:
		LightManager();
		~LightManager() = default;

		//light managment
		std::shared_ptr<DirectionalLight> CreateDirectionalLight();
		std::shared_ptr<PointLight> CreatePointLight();
		std::shared_ptr<SpotLight> CreateSpotLight();


		void RemoveLight(std::shared_ptr<Light> light);

		//Global Lighting parameters
		void SetAmbientLight(const DirectX::XMFLOAT3& color, float intensity);
		void SetExposure(float exposure) { m_SceneData.exposure = exposure; m_Dirty = true; }
		void SetGamma(float gamma) { m_SceneData.gamma = gamma; m_Dirty = true; }
		void SetIBLIntensity(float intensity) { m_SceneData.iblIntensity = intensity; m_Dirty = true; }


		// Culling and optimization
		void CullLights(const DirectX::BoundingFrustum& frustum);
		void UpdateLightData();

		// GPU binding
		void BindLightData();

		// Statistics
		uint32_t GetVisibleLightCount() const;
		std::string GetDebugInfo() const;

	private:
		void UpdateSceneLightData();

		std::vector<std::shared_ptr<DirectionalLight>> m_DirectionalLights;
		std::vector<std::shared_ptr<PointLight>> m_PointLights;
		std::vector<std::shared_ptr<SpotLight>> m_SpotLights;

		// Culled light indices for current frame
		std::vector<uint32_t> m_VisiblePointLights;
		std::vector<uint32_t> m_VisibleSpotLights;

		SceneLightData m_SceneData;
		ConstantBuffer<SceneLightData> m_LightBuffer;

		bool m_Dirty = true;
		bool m_BufferInitialized = false;
	};
}