#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace DXEngine
{
    // ==================================================================
    // Uuid: a 64-bit random identifier that belongs to an entity for its whole life and survives saving and loading. It is
    // what a saved scene (and, later, a prefab or a script) uses to say "that entity". An entt::entity cannot: it is an
    // index that changes every run. 0 is "no id".
    //
    // 64 random bits: two entities of one scene meeting by chance is about 1 in 4e9 for a million of them, and Scene still
    // checks, so a clash is retried, never stored. (The name is not UUID: that is a Windows type.)
    // ==================================================================
    struct Uuid
    {
        uint64_t value = 0;

        bool IsNull() const { return value == 0; }
        explicit operator bool() const { return value != 0; }

        static Uuid Generate();                          // never null

        std::string ToString() const;                    // 16 lower-case hex digits
        static bool Parse(const std::string& text, Uuid& out);   // exactly 16 hex digits (either case); nothing is written on failure

        bool operator==(const Uuid& o) const { return value == o.value; }
        bool operator!=(const Uuid& o) const { return value != o.value; }
        bool operator<(const Uuid& o) const { return value < o.value; }
    };
}

template<> struct std::hash<DXEngine::Uuid>
{
    size_t operator()(const DXEngine::Uuid& id) const noexcept { return std::hash<uint64_t>()(id.value); }
};
