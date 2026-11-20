#pragma once
// A short history of what the importer did, for the Assets panel: one line per event, newest last, capped.
#include <cstddef>
#include <deque>
#include <string>

namespace DXEngine::Assets
{
    enum class LogSeverity { Info, Warning, Error };

    struct LogLine
    {
        LogSeverity severity = LogSeverity::Info;
        std::string text;
    };

    class ImportLog
    {
    public:
        static constexpr size_t kMaxLines = 200;

        void Add(LogSeverity severity, std::string text)
        {
            m_Lines.push_back({ severity, std::move(text) });
            while (m_Lines.size() > kMaxLines) m_Lines.pop_front();
        }
        void Info(std::string text)    { Add(LogSeverity::Info, std::move(text)); }
        void Warning(std::string text) { Add(LogSeverity::Warning, std::move(text)); }
        void Error(std::string text)   { Add(LogSeverity::Error, std::move(text)); }
        void Clear() { m_Lines.clear(); }

        const std::deque<LogLine>& Lines() const { return m_Lines; }
        size_t Count(LogSeverity severity) const
        {
            size_t n = 0;
            for (const LogLine& l : m_Lines) if (l.severity == severity) ++n;
            return n;
        }

    private:
        std::deque<LogLine> m_Lines;
    };
}
