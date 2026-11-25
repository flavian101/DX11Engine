#include "DXEngine.h"
#include "Main.h"

using namespace DXEngine;

// The smallest application on the engine: a lit cube on a plane, a fly camera, nothing else (no UI, no editor).
// Close the window to quit. It links DXEngineCore only; see Game/CMakeLists.txt.
//
// The cube's spin is GAMEPLAY, so it is a system of the GameRuntime (the same kind the editor runs between Play and Stop), not
// code in the layer: the layer only owns the scene and the frame's order (gameplay, animation, transforms, drawing).
class SpinSystem : public IGameSystem
{
public:
    void OnStart(Scene& scene) override { m_Cube = scene.FindByName("Cube"); m_Angle = 0.0f; }
    void OnUpdate(Scene&, float dt) override
    {
        if (!m_Cube.IsValid()) return;
        m_Angle += dt;
        m_Cube.Get<TransformComponent>().SetEulerDegrees(0.0f, m_Angle * 45.0f, 0.0f);       // 45 degrees per second
    }
    void OnStop(Scene&) override { m_Cube = Entity(); }

private:
    Entity m_Cube;
    float m_Angle = 0.0f;
};

class GameLayer : public Layer
{
public:
    GameLayer() : Layer("GameLayer") {}

    void OnAttach() override
    {
        Renderer::Get().SetEnvironment(RenderEnvironment());
        Renderer::Get().SetClearColor(0.1f, 0.1f, 0.16f);

        m_Camera = std::make_shared<CameraController>();
        m_Camera->GetCamera()->SetPosition({ 0.0f, 3.0f, 9.0f });
        m_Camera->GetCamera()->SetRotation({ -0.2f, 0.0f, 0.0f });

        auto device = Renderer::Get().GetDevice();
        auto ground = MaterialFactory::CreateLitMaterial("Ground");
        ground->SetDiffuseColor({ 0.35f, 0.4f, 0.35f, 1.0f });
        SceneFactory::CreatePlane(m_Scene, device, "Ground", 40.0f, 40.0f, ground).Get<MeshRendererComponent>().castsShadows = false;

        auto metal = MaterialFactory::CreateLitMaterial("Cube");
        metal->SetDiffuseColor({ 0.8f, 0.45f, 0.2f, 1.0f });
        Entity cube = SceneFactory::CreateCube(m_Scene, device, "Cube", 2.0f, metal);
        cube.Get<TransformComponent>().position = { 0.0f, 1.0f, 0.0f };

        Entity sun = SceneFactory::CreateDirectionalLight(m_Scene, "Sun", { -0.4f, -0.8f, -0.3f }, { 1.0f, 0.95f, 0.85f }, 3.0f);
        sun.Get<LightComponent>().castsShadows = true;

        m_Game.Add(std::make_unique<SpinSystem>());
        m_Game.Start(m_Scene);                                      // a game runs from the first frame to the last
    }

    void OnDetach() override { m_Game.Stop(m_Scene); }

    void OnUpdate(float dt) override
    {
        m_Game.Update(m_Scene, dt);                                 // gameplay first: it writes components, the systems below derive the rest
        m_Camera->Update(dt);
        AnimationSystem::Update(m_Scene, dt);
        TransformSystem::Update(m_Scene);

        Renderer::Get().BeginScene(m_Camera->GetCamera());
        Renderer::Get().Submit(m_Scene);
        Renderer::Get().EndScene();
    }

    void OnEvent(Event& event) override { m_Camera->OnEvent(event); }

private:
    Scene m_Scene;
    GameRuntime m_Game;
    std::shared_ptr<CameraController> m_Camera;
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
