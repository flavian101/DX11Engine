#pragma once
#include "renderer/RendererCommand.h"
#include "utils/Texture.h"
#include <utils/mesh/Mesh.h>
#include "models/Model.h"


namespace DXEngine {

	class Ball : public Model
	{
	public:
		Ball();
	private:
		void Initialize();

	};

	class BallPhysics
	{
	public:
		BallPhysics(const DirectX::XMFLOAT3& position, float ballRadius, float ballMass)
			:
			m_StartPosition(position),
			m_Position(position),
			m_Velocity({ 0.0f, 0.0f, 0.0f }),
			m_BallRadius(ballRadius),
			m_BallMass(ballMass)
		{
			m_BallArea = DirectX::XM_PI * ballRadius * ballRadius;
		}

		void Reset() {
			m_Velocity = { 0.0f,0.0f, 0.0f };
			m_Position = m_StartPosition;

			///later we can add a smooth reset func to slowly drag the ball to the start pos instead of instant
		}

		DirectX::XMFLOAT3 Update(float deltatime,const DirectX::XMMATRIX& boardWorld)
		{
			const float eps = 0.001f;

			//Board plane + local frame fro the board's world matrix
			// boardWorld = Scale(20,1,20) * Rotation(pitch,roll) * Translation(0,0,0)

			//board center( plane point p0) = translation column
			DirectX::XMVECTOR p0 = boardWorld.r[3];
			//BoardNormal = local +Y carried into world by the Rotate only
			//(using the Inverse-transpose so that the 20x scale does not corrupt the direction)
			DirectX::XMVECTOR det;
			DirectX::XMMATRIX boardWorldInv = DirectX::XMMatrixInverse(&det, boardWorld);
			DirectX::XMVECTOR boardNormal = DirectX::XMVector3Normalize(
				DirectX::XMVector3TransformNormal(
					DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
					DirectX::XMMatrixTranspose(boardWorldInv)));


			//world gravity
			DirectX::XMVECTOR g = DirectX::XMVectorSet(0.0f, -m_Gravity, 0.0f,0.0f);

			// --- Where is the ball relative to the board? ----------------------
			DirectX::XMVECTOR ballPos = DirectX::XMLoadFloat3(&m_Position);

			// Signed distance from ball center to the board plane, along the normal.
			float d = DirectX::XMVectorGetX(
				DirectX::XMVector3Dot(DirectX::XMVectorSubtract(ballPos, p0), boardNormal));


			// Reverse world transform: ball position in the board's LOCAL space.
			// In local space the board is the flat quad [-1,1] x [-1,1] at y=0.
			DirectX::XMVECTOR ballLocal = DirectX::XMVector3TransformCoord(ballPos, boardWorldInv);
			DirectX::XMFLOAT3 bl;
			DirectX::XMStoreFloat3(&bl, ballLocal);

			// Is the contact point within the board's rectangle? (local quad is [-1,1])
			bool overBoard = (fabsf(bl.x) <= 1.0f) && (fabsf(bl.z) <= 1.0f);

			// Resting = near the surface AND above the board AND not moving away.
			bool onGround = overBoard && (d <= m_BallRadius + eps);


			//split gravity into normal + tangential parts
			float gDotN = DirectX::XMVectorGetX(DirectX::XMVector3Dot(g, boardNormal));
			DirectX::XMVECTOR gNormal = DirectX::XMVectorScale(boardNormal, gDotN);
			DirectX::XMVECTOR gTangent = DirectX::XMVectorSubtract(g, gNormal);

			DirectX::XMFLOAT3 gt, gn;
			DirectX::XMStoreFloat3(&gt, gTangent);
			DirectX::XMStoreFloat3(&gn, gNormal);

			float accelX, accelY, accelZ;

			if (onGround)
			{
				accelX = gt.x;
				accelY = gt.y;
				accelZ = gt.z;
			}
			else
			{
				//airbone; full gravity applies
				accelX = 0.0f;
				accelY = -m_Gravity;
				accelZ = 0.0f;

			}
			//calculate air drag

			//speed = distance / time
			//acceleration = change in velocity / time
			//airDragForce = 1/2* density* velocity^2 * dragCoefficient * area
			//Terminal Velocity = sqrt((2 * mass * gravity) / (density * dragCoefficient * area))
			//force = mass * acceleration
			//instantaneous accel = g- (density * dragCoefficient * area * velocity^2) / (2 * mass)
			//velocity = acceleration * time
			float speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.y * m_Velocity.y + m_Velocity.z * m_Velocity.z);
			if (speed > 0.0001 && !onGround)
			{
				float k = 0.5f * m_AirDensity * m_DragCoefficient * m_BallArea / m_BallMass;
				float dragAccel = k * speed * speed;
				float invSpeed = 1.0f / speed;

				accelX -= dragAccel * m_Velocity.x * invSpeed;
				accelY -= dragAccel * m_Velocity.y * invSpeed;
				accelZ -= dragAccel * m_Velocity.z * invSpeed;
			}

			//integrate velocities
			m_Velocity.x += accelX * deltatime;
			m_Velocity.z += accelZ * deltatime;
			m_Velocity.y += accelY * deltatime;

		
			m_Position.x += m_Velocity.x * deltatime;
			m_Position.z += m_Velocity.z * deltatime;
			m_Position.y += m_Velocity.y * deltatime;

			// --- Collision with the INCLINED board plane ------------------------
			// Recompute d and local after integration.
			ballPos = DirectX::XMLoadFloat3(&m_Position);
			d = DirectX::XMVectorGetX(
				DirectX::XMVector3Dot(DirectX::XMVectorSubtract(ballPos, p0), boardNormal));
			ballLocal = DirectX::XMVector3TransformCoord(ballPos, boardWorldInv);
			DirectX::XMStoreFloat3(&bl, ballLocal);
			overBoard = (fabsf(bl.x) <= 1.0f) && (fabsf(bl.z) <= 1.0f);



			if (overBoard && d < m_BallRadius)
			{
				// Push the ball out of the surface along the normal.
				float penetration = m_BallRadius - d;
				DirectX::XMVECTOR corrected = DirectX::XMVectorAdd(
					ballPos, DirectX::XMVectorScale(boardNormal, penetration));
				DirectX::XMStoreFloat3(&m_Position, corrected);

				// Velocity along the normal (negative = moving into the plane).
				DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&m_Velocity);
				float vN = DirectX::XMVectorGetX(DirectX::XMVector3Dot(vel, boardNormal));

				if (vN < 0.0f)
				{
					// Remove the into-plane velocity; reflect a little for bounce.
					DirectX::XMVECTOR bounce = DirectX::XMVectorScale(boardNormal, -(1.0f + m_Restitution) * vN);
					vel = DirectX::XMVectorAdd(vel, bounce);

					// Tangential (rolling) friction on contact.
					DirectX::XMVECTOR vNormalPart = DirectX::XMVectorScale(boardNormal, DirectX::XMVectorGetX(DirectX::XMVector3Dot(vel, boardNormal)));
					DirectX::XMVECTOR vTangentPart = DirectX::XMVectorSubtract(vel, vNormalPart);
					vTangentPart = DirectX::XMVectorScale(vTangentPart, 1.0f - m_BounceFriction);
					vel = DirectX::XMVectorAdd(vNormalPart, vTangentPart);

					DirectX::XMStoreFloat3(&m_Velocity, vel);
				}
			}
			OutputDebugStringW((L"Ball Position: " + std::to_wstring(m_Position.x) + L", " + std::to_wstring(m_Position.y) + L", " + std::to_wstring(m_Position.z) + L"\n").c_str());
			return m_Position;
		}

	private:
		DirectX::XMFLOAT3 m_StartPosition;
		DirectX::XMFLOAT3 m_Position;
		DirectX::XMFLOAT3 m_Velocity;
		float m_Gravity = 300.8f; //acceleration due to gravity 
		float m_Friction = 1.5f;
		float m_DragCoefficient = 0.47f; //smooth sphere // 0.25-> rough sphere
		float m_AirDensity = 1.225f; //kg/m^3 at sea level
		float m_BallRadius;
		float m_BallMass; //kg
		float m_BallArea; //cross-sectional area of the ball
		float m_BoardSurface = 0.0f;
		float m_Restitution = 0.7f; //coefficient of restitution for bounce
		float m_BounceFriction = 0.1f; //horizontal speed lost on bounce
		float m_RollingFriction = 0.2f;

	};

}