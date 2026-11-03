#ifdef __SWITCH__
#include <switch.h>
#endif

#include <borealis.hpp>
#include <cstdlib>

#include <ispotifai/version.hpp>

#include "main_activity.hpp"

int main(int argc, char *argv[]) {
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow(ispotifai::kName);

    // START quits the app from any activity
    brls::Application::setGlobalQuit(true);

    brls::Application::pushActivity(new MainActivity());

    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
