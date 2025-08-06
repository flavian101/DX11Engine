#pragma once
#include "utils\Vertex.h"
#include "Graphics.h"
#include "utils\VertexBuffer.h"
#include "utils\VertexShader.h"
#include "utils\PixelShader.h"
#include "utils\InputLayout.h"
#include "utils\Topology.h"
#include "utils\IndexBuffer.h"
#include "utils\ConstantBuffer.h"
#include "utils\Sampler.h"
#include "Renderer.h"

namespace DXEngine {

	class RendererCommand
	{

	public:
		inline static void Init();
		
		inline static void SetClearColor();
		inline static void SetDepthClearColor();

		inline static void Clear();

		inline static void DrawIndexd();

		inline static void setViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height);


	private:

		static Renderer s_Renderer;


	};
}
