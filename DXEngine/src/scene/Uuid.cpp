#include "dxpch.h"
#include "Uuid.h"
#include <random>

namespace DXEngine
{
    Uuid Uuid::Generate()
    {
        // one generator per thread, seeded from the OS: ids differ between runs and between threads
        thread_local std::mt19937_64 rng{ std::random_device{}() ^ (static_cast<uint64_t>(std::random_device{}()) << 32) };
        for (;;)
        {
            const uint64_t v = rng();
            if (v != 0) return Uuid{ v };
        }
    }

    std::string Uuid::ToString() const
    {
        static const char* digits = "0123456789abcdef";
        std::string s(16, '0');
        for (int i = 15; i >= 0; --i)
            s[static_cast<size_t>(i)] = digits[(value >> ((15 - i) * 4)) & 0xF];
        return s;
    }

    bool Uuid::Parse(const std::string& text, Uuid& out)
    {
        if (text.size() != 16) return false;
        uint64_t v = 0;
        for (char c : text)
        {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            v = (v << 4) | static_cast<uint64_t>(d);
        }
        out.value = v;
        return true;
    }
}
