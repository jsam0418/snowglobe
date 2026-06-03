// Snowglobe WebSocket server entry point.
//
// v1 scaffold: stands up the uWebSockets app/event loop on the I/O thread and
// echoes client messages. The simulation thread, the SpscQueue hand-off
// (snowglobe/core/spsc_queue.hpp), and state broadcasting are wired in as the
// engine lands — see CLAUDE.md for the intended threading model.

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <uwebsockets/App.h>

namespace {

std::uint16_t listen_port() {
    // Read once at startup, before any threads are spawned, so getenv is safe.
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    if (const char* env = std::getenv("SNOWGLOBE_PORT")) {
        const std::string_view text{env};
        std::uint16_t value{};
        const auto* end = text.data() + text.size();
        if (std::from_chars(text.data(), end, value).ec == std::errc{}) {
            return value;
        }
    }
    return 9001;
}

// Per-connection state lives here once sessions carry simulation context.
struct ClientSession {};

} // namespace

int main() {
    // Flush stdout on every write so logs surface promptly in `kubectl logs`
    // (a pipe to the container runtime is fully buffered by default).
    std::cout << std::unitbuf;

    const std::uint16_t port = listen_port();

    uWS::App()
        .ws<ClientSession>("/*",
                           {
                               .open =
                                   [](auto* ws) {
                                       ws->subscribe("world");
                                       ws->send("snowglobe: connected", uWS::OpCode::TEXT);
                                   },
                               .message =
                                   [](auto* ws, std::string_view message, uWS::OpCode opcode) {
                                       ws->send(message, opcode); // echo until the sim feeds us
                                   },
                           })
        .listen(port,
                [port](auto* token) {
                    if (token) {
                        std::cout << "snowglobe listening on :" << port << '\n';
                    } else {
                        std::cerr << "snowglobe failed to bind :" << port << '\n';
                    }
                })
        .run();

    return 0;
}
