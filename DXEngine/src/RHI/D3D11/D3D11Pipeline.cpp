#include "dxpch.h"
#include "D3D11Pipeline.h"

namespace DXEngine::RHI
{
	D3D11Pipeline::D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc)
		:
		m_Desc(desc)
	{
	}
}