#include "dxpch.h"
#include "GameRuntime.h"
#include <exception>

namespace DXEngine
{
    GameRuntime::GameRuntime() = default;
    GameRuntime::~GameRuntime() = default;

    bool GameRuntime::Add(std::unique_ptr<IGameSystem> system)
    {
        if (!system || m_Running) return false;
        m_Systems.push_back(std::move(system));
        return true;
    }

    bool GameRuntime::Start(Scene& scene)
    {
        if (m_Running) return true;
        m_LastError.clear();

        size_t started = 0;
        try
        {
            for (; started < m_Systems.size(); ++started)
                m_Systems[started]->OnStart(scene);
        }
        catch (const std::exception& e)
        {
            m_LastError = e.what();
            StopStarted(scene, started);                    // the system that threw never finished starting: it is not stopped
            return false;
        }
        m_Running = true;
        return true;
    }

    bool GameRuntime::Update(Scene& scene, float deltaSeconds)
    {
        if (!m_Running) return true;
        try
        {
            for (auto& system : m_Systems)
                system->OnUpdate(scene, deltaSeconds);
        }
        catch (const std::exception& e)
        {
            const std::string what = e.what();
            Stop(scene);
            m_LastError = what;                             // the failure that ended the game, not one of the OnStop calls
            return false;
        }
        return true;
    }

    void GameRuntime::Stop(Scene& scene)
    {
        if (!m_Running) return;
        m_Running = false;
        StopStarted(scene, m_Systems.size());
    }

    void GameRuntime::StopStarted(Scene& scene, size_t started)
    {
        for (size_t i = started; i-- > 0;)
        {
            try { m_Systems[i]->OnStop(scene); }
            catch (const std::exception& e) { m_LastError = e.what(); }      // the others still stop
        }
    }
}
