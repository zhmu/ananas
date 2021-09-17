#pragma once

#include "types.h"

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
} // namespace ipc
