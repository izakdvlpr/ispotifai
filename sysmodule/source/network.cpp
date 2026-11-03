#include "network.hpp"

#include "log.hpp"

namespace ispotifai {

namespace {

// Submitting the request only means the system accepted it, not that DHCP is
// done, so the address is polled afterwards.
constexpr int kIpTries      = 30;
constexpr u64 kIpPollNs     = 2ull * 1000000000ull; // 2s

NifmRequest g_request = {};
bool g_held           = false;

} // namespace

void networkRelease(void) {
    if (!g_held)
        return;

    nifmRequestCancel(&g_request); // fails harmlessly if never submitted
    nifmRequestClose(&g_request);

    g_request = NifmRequest{};
    g_held    = false;
}

bool networkAcquire(void) {
    networkRelease();

    Result rc = nifmCreateRequest(&g_request, true);
    if (R_FAILED(rc)) {
        logf("nifmCreateRequest failed, rc=0x%x", rc);
        return false;
    }

    g_held = true;

    // Only valid before the request is submitted. Not fatal on its own: it just
    // means the connection drops when the console sleeps.
    rc = nifmRequestSetKeptInSleep(&g_request, true);
    if (R_FAILED(rc))
        logf("nifmRequestSetKeptInSleep failed, rc=0x%x", rc);

    rc = nifmRequestSubmitAndWait(&g_request);
    if (R_FAILED(rc)) {
        logf("nifmRequestSubmitAndWait failed, rc=0x%x", rc);
        networkRelease();
        return false;
    }

    for (int tries = 0; tries < kIpTries; tries++) {
        u32 ip = 0;
        if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0)
            return true;

        if (tries == 0)
            logf("waiting for network...");

        svcSleepThread(kIpPollNs);
    }

    logf("no address after %d tries, retrying the request", kIpTries);
    networkRelease();
    return false;
}

bool networkIsUp(void) {
    if (!g_held)
        return false;

    NifmRequestState state = NifmRequestState_Invalid;
    if (R_FAILED(nifmGetRequestState(&g_request, &state)) || state != NifmRequestState_Available)
        return false;

    u32 ip = 0;
    return R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0;
}

u32 networkIp(void) {
    u32 ip = 0;
    if (R_FAILED(nifmGetCurrentIpAddress(&ip)))
        return 0;

    return ip;
}

void networkRegisterSocket(int fd) {
    if (!g_held)
        return;

    Result rc = nifmRequestRegisterSocketDescriptor(&g_request, fd);
    if (R_FAILED(rc))
        logf("nifm register fd %d failed, rc=0x%x", fd, rc);
}

void networkUnregisterSocket(int fd) {
    if (!g_held)
        return;

    nifmRequestUnregisterSocketDescriptor(&g_request, fd);
}

} // namespace ispotifai
