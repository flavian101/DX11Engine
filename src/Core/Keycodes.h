#pragma once

#include <Windows.h>
#include <cstdint>
namespace DXEngine {


    enum class KeyCode : uint16_t {
        UNKNOWN = 0x00,
        // Letters
        A = 0x41,  // 'A' key
        B = 0x42,
        C = 0x43,
        D = 0x44,
        E = 0x45,
        F = 0x46,
        G = 0x47,
        H = 0x48,
        I = 0x49,
        J = 0x4A,
        K = 0x4B,
        L = 0x4C,
        M = 0x4D,
        N = 0x4E,
        O = 0x4F,
        P = 0x50,
        Q = 0x51,
        R = 0x52,
        S = 0x53,
        T = 0x54,
        U = 0x55,
        V = 0x56,
        W = 0x57,
        X = 0x58,
        Y = 0x59,
        Z = 0x5A,

        // Digits
        DIGIT0 = 0x30,  // '0'
        DIGIT1 = 0x31,
        DIGIT2 = 0x32,
        DIGIT3 = 0x33,
        DIGIT4 = 0x34,
        DIGIT5 = 0x35,
        DIGIT6 = 0x36,
        DIGIT7 = 0x37,
        DIGIT8 = 0x38,
        DIGIT9 = 0x39,

        // Function keys
        F1 = VK_F1,  // 0x70
        F2 = VK_F2,
        F3 = VK_F3,
        F4 = VK_F4,
        F5 = VK_F5,
        F6 = VK_F6,
        F7 = VK_F7,
        F8 = VK_F8,
        F9 = VK_F9,
        F10 = VK_F10,
        F11 = VK_F11,
        F12 = VK_F12,

        // Control keys
        ESCAPE = VK_ESCAPE,     // 0x1B
        SPACE = VK_SPACE,      // 0x20
        LEFT_SHIFT = VK_LSHIFT,     // 0xA0
        RIGHT_SHIFT = VK_RSHIFT,
        LEFT_CONTROL = VK_LCONTROL,   // 0xA2
        RIGHT_CONTROL = VK_RCONTROL,
        LEFT_ALT = VK_LMENU,      // 0xA4
        RIGHT_ALT = VK_RMENU,

        // Arrow keys
        LEFT_ARROW = VK_LEFT,       // 0x25
        UP_ARROW = VK_UP,         // 0x26
        RIGHT_ARROW = VK_RIGHT,      // 0x27
        DOWN_ARROW = VK_DOWN,       // 0x28
    };

}