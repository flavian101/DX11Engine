#pragma once
#include "models/Model.h"
#include "renderer/RendererCommand.h"


namespace DXEngine {
	class Board : public Model {
	public:
		Board();
		~Board();

	private:
	};

	class BoardController {
	public:
		BoardController();
		~BoardController();

		DirectX::XMFLOAT3 Update(float deltaTime);


	private:
		float m_Pitch;
		float m_Roll;
		float m_TiltSpeed;
		float m_MaxTilt;
		DirectX::XMFLOAT3 m_Orientation;
	};
}