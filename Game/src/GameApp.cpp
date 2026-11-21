#include "DXEngine.h"
#include "Main.h"

using namespace DXEngine;

// The smallest application on the engine: a lit cube on a plane, a fly camera, nothing else (no UI, no editor).
// Close the window to quit. It links DXEngineCore only; see Game/CMakeLists.txt.
class GameLayer : public Layer
{
public:
    GameLayer() : Layer("GameLayer") {}

    void OnAttach() override
    {
        Renderer::SetEnvironment(RenderEnvironment());
        Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

        m_Camera = std::make_shared<CameraController>();
        m_Camera->GetCamera()->SetPosition({ 0.0f, 3.0f, 9.0f });
        m_Camera->GetCamera()->SetRotation({ -0.2f, 0.0f, 0.0f });

        auto device = Renderer::GetDevice();
        auto ground = MaterialFactory::CreateLitMaterial("Ground");
        ground->SetDiffuseColor({ 0.35f, 0.4f, 0.35f, 1.0f });
        SceneFactory::CreatePlane(m_Scene, device, "Ground", 40.0f, 40.0f, ground).Get<MeshRendererComponent>().castsShadows = false;

        auto metal = MaterialFactory::CreateLitMaterial("Cube");
        metal->SetDiffuseColor({ 0.8f, 0.45f, 0.2f, 1.0f });
        m_Cube = SceneFactory::CreateCube(m_Scene, device, "Cube", 2.0f, metal);
        m_Cube.Get<TransformComponent>().position = { 0.0f, 1.0f, 0.0f };

        Entity sun = SceneFactory::CreateDirectionalLight(m_Scene, "Sun", { -0.4f, -0.8f, -0.3f }, { 1.0f, 0.95f, 0.85f }, 3.0f);
        sun.Get<LightComponent>().castsShadows = true;
    }

    void OnUpdate(float dt) override
    {
        m_Angle += dt;
        m_Cube.Get<TransformComponent>().SetEulerDegrees(0.0f, m_Angle * 45.0f, 0.0f);       // 45 degrees per second

        m_Camera->Update(dt);
        AnimationSystem::Update(m_Scene, dt);
        TransformSystem::Update(m_Scene);

        Renderer::BeginScene(m_Camera->GetCamera());
        Renderer::Submit(m_Scene);
        Renderer::EndScene();
    }

    void OnEvent(Event& event) override { m_Camera->OnEvent(event); }

private:
    Scene m_Scene;
    Entity m_Cube;
    std::shared_ptr<CameraController> m_Camera;
    float m_Angle = 0.0f;
};

class GameApp : public Application
{
public:
    GameApp() : Application() { PushLayer(new GameLayer()); }
};

Application* DXEngine::CreateApplication()
{
    return new GameApp();
}
