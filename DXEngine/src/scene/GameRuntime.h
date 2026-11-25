#pragma once
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Scene;

    // ==================================================================
    // IGameSystem: one piece of gameplay. It reads and writes the components of the live scene and nothing else; drawing is the
    // renderer's business (principle 4). It runs only while the game runs.
    // ==================================================================
    class IGameSystem
    {
    public:
        virtual ~IGameSystem() = default;

        virtual void OnStart(Scene& scene) { (void)scene; }          // the game begins: find what you need, set your own state up
        virtual void OnUpdate(Scene& scene, float deltaSeconds) = 0; // once per frame, before animation and transforms are derived
        virtual void OnStop(Scene& scene) { (void)scene; }           // the game ends: let go of entities you hold
    };

    // ==================================================================
    // GameRuntime: the game layer's engine, the systems and the rule for when they run. A game runs it from the start; the editor
    // runs it between Play and Stop, with the scene put back afterwards (SceneSnapshot), so a system may change the scene however
    // it likes and never has to undo anything.
    //
    //     GameRuntime game;
    //     game.Add(std::make_unique<MySystem>());      // systems run in the order they were added
    //     game.Start(scene);                           // OnStart of each, in that order
    //     ... every frame, BEFORE AnimationSystem and TransformSystem:   game.Update(scene, dt);
    //     game.Stop(scene);                            // OnStop of each, in the opposite order
    //
    // A system that throws does not take the application down: the runtime stops (every system gets its OnStop), Update returns
    // false and LastError() says what happened. The host decides what that means (the editor ends the play session).
    // ==================================================================
    class GameRuntime
    {
    public:
        GameRuntime();
        ~GameRuntime();
        GameRuntime(const GameRuntime&) = delete;
        GameRuntime& operator=(const GameRuntime&) = delete;

        // Systems can be added while the game is not running. False (nothing added) while it runs, or for a null system.
        bool Add(std::unique_ptr<IGameSystem> system);
        size_t SystemCount() const { return m_Systems.size(); }

        // Starts the game: OnStart of every system. Does nothing when it already runs. A system that throws in OnStart stops the
        // start (the ones started so far get their OnStop) and the runtime stays stopped; false then, and LastError() says why.
        bool Start(Scene& scene);

        // One frame of the game. Does nothing (true) when it is not running; false when a system threw and the game stopped.
        bool Update(Scene& scene, float deltaSeconds);

        // Ends the game: OnStop of every system, last started first. Does nothing when it is not running. A throwing OnStop is
        // noted (LastError) and does not keep the others from stopping.
        void Stop(Scene& scene);

        bool IsRunning() const { return m_Running; }
        const std::string& LastError() const { return m_LastError; }     // the last failure ("" = none since Start)

    private:
        void StopStarted(Scene& scene, size_t started);

        std::vector<std::unique_ptr<IGameSystem>> m_Systems;
        bool m_Running = false;
        std::string m_LastError;
    };
}
