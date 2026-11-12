#pragma once
#include "Application.h"
#include "renderer/ShaderCache.h"
#include "picking/Ray.h"
#include "models/ModelLoader.h"
#include "utils/Mesh/Mesh.h"
#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "Event/MouseEvent.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "utils/UI/UILayer.h"

#include "Core/LayerStack.h"
#include <FrameTime.h>
#include "Core/Input.h"
#include "Core/ImGuiLayer.h"
#include "editor/RendererPanel.h"

#include "renderer/Renderer.h"

// scene
#include "scene/Scene.h"
#include "scene/SceneFactory.h"
#include "scene/ScenePicking.h"
#include "scene/AnimationSystem.h"
#include "scene/TransformSystem.h"


#define DX_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)


