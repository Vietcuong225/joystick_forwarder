#pragma once

#include <cstdint>
#include <string>
#include <jsoncpp/json/json.h>

#ifdef _WIN32
#include <windows.h>
#include <XInput.h>
#pragma comment(lib, "XInput.lib")
#elif __APPLE__
#include <IOKit/hid/IOHIDLib.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>
#endif

struct XboxControllerState
{
    // Analog sticks (-32768 to 32767)
    int16_t leftStickX;
    int16_t leftStickY;
    int16_t rightStickX;
    int16_t rightStickY;

    // Triggers (0 to 255)
    uint8_t leftTrigger;
    uint8_t rightTrigger;

    // Buttons (0 or 1)
    bool buttonA;
    bool buttonB;
    bool buttonX;
    bool buttonY;
    bool buttonLB;
    bool buttonRB;
    bool buttonBack;
    bool buttonStart;
    bool buttonLeftStick;
    bool buttonRightStick;

    // D-pad
    bool dpadUp;
    bool dpadDown;
    bool dpadLeft;
    bool dpadRight;

    // Controller connected status
    bool connected;

    // Convert to JSON
    Json::Value toJson() const;

    // Reset all values to default
    void reset();
};

class XboxController
{
public:
    XboxController(int controllerIndex = 0);
    ~XboxController();

    bool initialize();
    bool update();
    const XboxControllerState &getState() const;
    bool isConnected() const;

private:
    int controllerIndex_;
    XboxControllerState currentState_;

#ifdef _WIN32
    XINPUT_STATE xinputState_;
#elif __APPLE__
    IOHIDManagerRef hidManager_;
    CFSetRef deviceSet_;

    static void deviceAddedCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device);
    static void deviceRemovedCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device);
    static void inputValueCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value);

    void processAppleInput(IOHIDValueRef value);
    bool setupAppleHIDManager();
#else
    int joystickFd_;
    std::string devicePath_;

    bool findXboxController();
    bool parseLinuxEvent(const js_event &event);
#endif
};