#define TESLA_INIT_IMPL // only in this file
#include <tesla.hpp>

#include <ispotifai/version.hpp>

class MainGui : public tsl::Gui {
public:
    MainGui() = default;

    virtual tsl::elm::Element *createUI() override {
        auto frame = new tsl::elm::OverlayFrame(ispotifai::kName, ispotifai::kVersion);
        auto list  = new tsl::elm::List();

        list->addItem(new tsl::elm::ListItem("Hello from the overlay"));

        frame->setContent(list);
        return frame;
    }

    virtual void update() override {}

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos,
                             HidAnalogStickState joyStickPosLeft,
                             HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};

class Overlay : public tsl::Overlay {
public:
    // libtesla already inits fs, hid, pl, pmdmnt, hid:sys and set:sys
    virtual void initServices() override {}
    virtual void exitServices() override {}

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<Overlay>(argc, argv);
}
