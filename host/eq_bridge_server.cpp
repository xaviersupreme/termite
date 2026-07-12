#include "host/eq_bridge_server.h"

#include "host/eq_bridge_profile.h"

#include <sddl.h>

namespace termite {
namespace {

bool read_exact(HANDLE pipe, void* buffer, DWORD bytes) noexcept {
    auto* cursor = static_cast<std::byte*>(buffer);
    DWORD remaining = bytes;
    while (remaining > 0) {
        DWORD received{};
        if (!ReadFile(pipe, cursor, remaining, &received, nullptr) || received == 0) return false;
        cursor += received;
        remaining -= received;
    }
    return true;
}

void write_ack(HANDLE pipe, const eq_bridge_snapshot_v1& snapshot, eq_bridge_status status) noexcept {
    eq_bridge_ack_v1 acknowledgement;
    acknowledgement.sequence = snapshot.sequence;
    acknowledgement.status = static_cast<std::uint32_t>(status);
    DWORD written{};
    WriteFile(pipe, &acknowledgement, sizeof(acknowledgement), &written, nullptr);
}

}  // namespace

eq_bridge_server::~eq_bridge_server() { stop(); }

bool eq_bridge_server::start(HWND target_window) {
    if (target_window == nullptr || worker_.joinable()) return false;
    stopping_ = false;
    target_window_ = target_window;
    worker_ = std::jthread([this] { server_loop(); });
    return true;
}

void eq_bridge_server::stop() {
    if (!worker_.joinable()) return;
    stopping_ = true;
    // Wake ConnectNamedPipe.  The server observes stopping_ before reading.
    if (WaitNamedPipeW(eq_bridge_pipe_name, 25)) {
        const auto wake = CreateFileW(eq_bridge_pipe_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (wake != INVALID_HANDLE_VALUE) CloseHandle(wake);
    }
    worker_.join();
    target_window_ = nullptr;
}

std::optional<eq_bridge_snapshot_v1> eq_bridge_server::take_latest() {
    std::scoped_lock lock(latest_mutex_);
    auto result = latest_;
    latest_.reset();
    return result;
}

void eq_bridge_server::publish(eq_bridge_snapshot_v1 snapshot) {
    {
        std::scoped_lock lock(latest_mutex_);
        latest_ = snapshot;
    }
    const auto target = target_window_.load();
    if (target != nullptr) PostMessageW(target, eq_bridge_snapshot_message, 0, 0);
}

void eq_bridge_server::server_loop() {
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.lpSecurityDescriptor = descriptor;

    while (!stopping_) {
        const auto pipe = CreateNamedPipeW(eq_bridge_pipe_name, PIPE_ACCESS_DUPLEX,
                                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                           1, sizeof(eq_bridge_ack_v1), sizeof(eq_bridge_snapshot_v1), 0, &security);
        if (pipe == INVALID_HANDLE_VALUE) break;
        const auto connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected) {
            CloseHandle(pipe);
            continue;
        }
        if (stopping_) {
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
            break;
        }

        eq_bridge_snapshot_v1 snapshot{};
        if (read_exact(pipe, &snapshot, sizeof(snapshot))) {
            if (valid_eq_bridge_snapshot(snapshot)) {
                publish(snapshot);
                write_ack(pipe, snapshot, eq_bridge_status::accepted);
            } else {
                write_ack(pipe, snapshot, eq_bridge_status::rejected);
            }
        }
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    LocalFree(descriptor);
}

}  // namespace termite
