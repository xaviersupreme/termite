#include "host/eq_bridge_server.h"

#include <Windows.h>

#include <cassert>
#include <chrono>
#include <thread>

namespace {

bool write_exact(HANDLE pipe, const void* buffer, DWORD size) {
    DWORD written{};
    return WriteFile(pipe, buffer, size, &written, nullptr) != FALSE && written == size;
}

bool read_exact(HANDLE pipe, void* buffer, DWORD size) {
    DWORD received{};
    return ReadFile(pipe, buffer, size, &received, nullptr) != FALSE && received == size;
}

}  // namespace

int main() {
    const auto message_window = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(message_window != nullptr);
    termite::eq_bridge_server server;
    assert(server.start(message_window));

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 50 && pipe == INVALID_HANDLE_VALUE; ++attempt) {
        if (WaitNamedPipeW(termite::eq_bridge_pipe_name, 20)) {
            pipe = CreateFileW(termite::eq_bridge_pipe_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (pipe == INVALID_HANDLE_VALUE) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(pipe != INVALID_HANDLE_VALUE);

    termite::eq_bridge_snapshot_v1 snapshot;
    snapshot.sequence = 42;
    snapshot.graphic_gains[6] = 6.0F;
    assert(write_exact(pipe, &snapshot, sizeof(snapshot)));
    termite::eq_bridge_ack_v1 acknowledgement;
    assert(read_exact(pipe, &acknowledgement, sizeof(acknowledgement)));
    CloseHandle(pipe);

    assert(acknowledgement.magic == termite::eq_bridge_magic);
    assert(acknowledgement.sequence == snapshot.sequence);
    assert(acknowledgement.status == static_cast<std::uint32_t>(termite::eq_bridge_status::accepted));

    std::optional<termite::eq_bridge_snapshot_v1> accepted;
    for (int attempt = 0; attempt < 50 && !accepted.has_value(); ++attempt) {
        accepted = server.take_latest();
        if (!accepted.has_value()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(accepted.has_value());
    assert(accepted->sequence == snapshot.sequence);
    assert(accepted->graphic_gains[6] == 6.0F);
    server.stop();
    DestroyWindow(message_window);
}
