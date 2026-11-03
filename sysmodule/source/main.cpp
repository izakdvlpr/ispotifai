#include <switch.h>

#include <arpa/inet.h>

#include <cstdio>

#include <ispotifai/version.hpp>

#include "http_server.hpp"
#include "log.hpp"
#include "network.hpp"

// Inner heap. Sysmodules get no applet heap, so malloc/free need this, and the
// memory is taken from the system pool and never given back while the console
// is on. The socket driver's transfer memory comes out of here too, so it has
// to cover the buffers configured below.
#define INNER_HEAP_SIZE 0x100000

// 0x4953 = "IS" in ASCII. Well clear of the usual homebrew ports (ftpd 5000,
// nxlink 28280/28771) and below the ephemeral range.
static constexpr u16 kHttpPort = 18771;

// Small socket buffers: a sysmodule cannot afford the defaults.
static const SocketInitConfig kSocketConfig = {
    .tcp_tx_buf_size     = 0x1000,
    .tcp_rx_buf_size     = 0x1000,
    .tcp_tx_buf_max_size = 0x4000,
    .tcp_rx_buf_max_size = 0x4000,
    .udp_tx_buf_size     = 0,
    .udp_rx_buf_size     = 0,
    .sb_efficiency       = 2,
    // Listening socket + accepted socket at the same time, plus headroom.
    .num_bsd_sessions    = 3,
    .bsd_service_type    = BsdServiceType_User,
};

extern "C" {

// Sysmodules must not use applet*.
u32 __nx_applet_type = AppletType_None;

// One FS session is enough.
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void *fake_heap_start;
    extern void *fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    fsdevMountSdmc();

    rc = nifmInitialize(NifmServiceType_User);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = socketInitialize(&kSocketConfig);
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    smExit();
}

void __appExit(void) {
    socketExit();
    nifmExit();
    fsdevUnmountAll();
    fsExit();
}

} // extern "C"

int main(int argc, char *argv[]) {
    ispotifai::logReset();

    // Each pass takes a fresh network request and builds a fresh listening
    // socket. That is the recovery path: after a sleep or an AP change the old
    // socket is dead, so it gets thrown away and rebuilt instead of being
    // retried forever.
    for (;;) {
        if (!ispotifai::networkAcquire()) {
            svcSleepThread(5ull * 1000000000ull); // 5s
            continue;
        }

        // Relogged every pass because DHCP can hand out a different address.
        struct in_addr addr;
        addr.s_addr = ispotifai::networkIp();
        ispotifai::logf("network up: http://%s:%u/", inet_ntoa(addr), kHttpPort);

        ispotifai::ServerExit why = ispotifai::runHttpServer(kHttpPort);
        ispotifai::networkRelease();

        svcSleepThread(why == ispotifai::ServerExit::SetupFailed ? 5ull * 1000000000ull
                                                                : 2ull * 1000000000ull);
    }

    return 0;
}
