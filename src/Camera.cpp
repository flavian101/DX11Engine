#include "Camera.h"

Camera::Camera()
{
	camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);

}

DirectX::XMMATRIX Camera::GetView()
{
	return camView;
}
