#pragma once

namespace ipc
{
    using Handle = uint32_t;
    using IPCKey = uint32_t;

    enum class MessageType : uint8_t {
        CreateWindow,
        DestroyWindow,
        UpdateWindow,
        SetWindowTitle,
    };

    enum class ResultCode : uint8_t { Success, BadHandle, Failure };
    enum class EventType : uint8_t { KeyDown, KeyUp };

    enum class KeyCode : uint16_t {
        Unknown,
        Tab = 9,
        Enter = '\r',
        Escape = 27,
        Space = ' ',
        n0 = '0', n1, n2, n3, n4, n5, n6, n7, n8, n9,
        A = 'a',
        B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Special = 127,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        LeftArrow, RightArrow, UpArrow, DownArrow,
        LeftControl, LeftShift, LeftAlt, LeftGUI,
        RightControl, RightShift, RightAlt, RightGUI,
    };

    namespace keyModifiers {
        inline constexpr auto LeftShift = 1 << 0;
        inline constexpr auto RightShift = 1 << 1;
        inline constexpr auto LeftControl = 1 << 2;
        inline constexpr auto LeftGUI = 1 << 3;
        inline constexpr auto RightControl = 1 << 4;
        inline constexpr auto LeftAlt = 1 << 5;
        inline constexpr auto RightAlt = 1 << 6;
        inline constexpr auto RightGUI = 1 << 7;
    }

    struct Message {
        MessageType type;
        union {
            struct {
                uint32_t height, width;
                char title[64];
            } createWindow;
            struct DestroyWindow {
                Handle handle;
            } destroyWindow;
            struct UpdateWindow {
                Handle handle;
            } updateWindow;
            struct SetWindowTitle {
                Handle handle;
                char title[64];
            } setWindowTitle;
        } u;
    } __attribute__((packed));

    struct GenericReply {
        ResultCode result;
    };

    struct CreateWindowReply {
        ResultCode result;
        Handle handle;
        IPCKey fbKey;
    } __attribute__((packed));

    struct Event {
        EventType type;
        Handle handle;
        union {
            struct KeyDown {
                KeyCode key;
                int ch;
            } keyDown;
            struct KeyUp {
                KeyCode key;
                int ch;
            } keyUp;
        } u;
    } __attribute__((packed));
} // namespace ipc
