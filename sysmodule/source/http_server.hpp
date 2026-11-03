#pragma once

#include <switch.h>

namespace ispotifai {

// Why the server gave up.
enum class ServerExit {
    SetupFailed, // socket/bind/listen did not come up
    NetworkLost, // the listening socket died under us, rebuild it
};

// Serves GET / on the given port. Returns only when the listening socket has to
// be thrown away; the caller re-acquires the network and calls again.
ServerExit runHttpServer(u16 port);

} // namespace ispotifai
