#pragma once

#include <switch.h>

namespace ispotifai {

// Takes a nifm request and holds it for as long as the server runs. Without a
// submitted request the system assumes nobody needs the connection and is free
// to tear the interface down under us, which kills every open socket.
//
// Blocks until the console has an address. Returns false if the request could
// not be taken or no address showed up, in which case the caller should back
// off and try again.
bool networkAcquire(void);

// Cancels and closes the request. Safe to call when nothing is held.
void networkRelease(void);

// True while the held request is still Available and the console still has an
// address.
bool networkIsUp(void);

// Current IPv4 address in network byte order, 0 if there is none.
u32 networkIp(void);

// Ties a socket to the held request so the system keeps it alive across sleep.
// Failures are logged and ignored: the server still works, it just does not
// survive a suspend.
void networkRegisterSocket(int fd);
void networkUnregisterSocket(int fd);

} // namespace ispotifai
