#pragma once
#include "Application.h"
#include "shaders/ShaderManager.h"
#include "picking/PickingManager.h"
#include "picking/InterfacePickable.h"
#include "picking/Ray.h"
#include "utils/mesh\Mesh.h"
#include "Camera.h"
#include "Event/MouseEvent.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "utils/Light.h"
#include "utils/UI/UILayer.h"

#include "Core/LayerStack.h"
#include <FrameTime.h>
#include "Application.h"
#include "Core/Input.h"

#include "renderer/Renderer.h"


#define DX_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)


