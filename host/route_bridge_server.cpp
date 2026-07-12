#include "host/route_bridge_server.h"

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

void write_response(HANDLE pipe, const route_bridge_response_v1& response) noexcept {
    DWORD written{};
    WriteFile(pipe, &response, sizeof(response), &written, nullptr);
}

route_bridge_response_v1 rejected_response(const route_bridge_request_v1& request) {
    route_bridge_response_v1 response;
    response.sequence = request.sequence;
    response.status = static_cast<std::uint32_t>(route_bridge_status::rejected);
    wcscpy_s(response.message, L"Termite rejected an invalid routing request.");
    return response;
}

}  // namespace

route_bridge_server::~route_bridge_server() { stop(); }

bool route_bridge_server::start(HWND target_window) {
    if (target_window == nullptr || worker_.joinable()) return false;
    stopping_ = false;
    target_window_ = target_window;
    worker_ = std::jthread([this] { server_loop(); });
    return true;
}

void route_bridge_server::stop() {
    if (!worker_.joinable()) return;
    stopping_ = true;
    if (WaitNamedPipeW(route_bridge_pipe_name, 25)) {
        const auto wake = CreateFileW(route_bridge_pipe_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (wake != INVALID_HANDLE_VALUE) CloseHandle(wake);
    }
    worker_.join();
    target_window_ = nullptr;
}

void route_bridge_server::server_loop() {
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.lpSecurityDescriptor = descriptor;

    while (!stopping_) {
        const auto pipe = CreateNamedPipeW(route_bridge_pipe_name, PIPE_ACCESS_DUPLEX,
                                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                           1, sizeof(route_bridge_response_v1), sizeof(route_bridge_request_v1), 0, &security);
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

        route_bridge_request_v1 request{};
        route_bridge_response_v1 response{};
        if (read_exact(pipe, &request, sizeof(request))) {
            if (valid_route_bridge_request(request)) {
                route_bridge_transaction_v1 transaction;
                transaction.request = request;
                // Routing is deliberately executed by the host's UI thread:
                // it owns the route snapshots that must be restored on exit.
                SendMessageW(target_window_.load(), route_bridge_request_message, 0,
                             reinterpret_cast<LPARAM>(&transaction));
                response = transaction.response;
            } else {
                response = rejected_response(request);
            }
            write_response(pipe, response);
        }
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    LocalFree(descriptor);
}

}  // namespace termite
