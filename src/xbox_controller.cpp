#include "xbox_controller.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

#ifdef __APPLE__
#include <IOKit/hid/IOHIDKeys.h>
#endif

XboxController::XboxController(int controllerIndex)
    : controllerIndex_(controllerIndex)
{
#ifdef _WIN32
    ZeroMemory(&xinputState_, sizeof(XINPUT_STATE));
#elif __APPLE__
    hidManager_ = nullptr;
    deviceSet_ = nullptr;
#else
    joystickFd_ = -1;
#endif
    currentState_.reset();
}

XboxController::~XboxController()
{
#ifdef __APPLE__
    if (hidManager_)
    {
        IOHIDManagerClose(hidManager_, kIOHIDOptionsTypeNone);
        CFRelease(hidManager_);
    }
    if (deviceSet_)
    {
        CFRelease(deviceSet_);
    }
#elif !defined(_WIN32)
    if (joystickFd_ != -1)
    {
        close(joystickFd_);
    }
#endif
}

void XboxControllerState::reset()
{
    leftStickX = 0;
    leftStickY = 0;
    rightStickX = 0;
    rightStickY = 0;
    leftTrigger = 0;
    rightTrigger = 0;
    buttonA = false;
    buttonB = false;
    buttonX = false;
    buttonY = false;
    buttonLB = false;
    buttonRB = false;
    buttonBack = false;
    buttonStart = false;
    buttonLeftStick = false;
    buttonRightStick = false;
    dpadUp = false;
    dpadDown = false;
    dpadLeft = false;
    dpadRight = false;
    connected = false;
}

bool XboxController::initialize()
{
#ifdef _WIN32
    // Windows uses XInput, no explicit initialization needed
    return true;
#elif __APPLE__
    return setupAppleHIDManager();
#else
    return findXboxController();
#endif
}

bool XboxController::update()
{
#ifdef _WIN32
    // Windows implementation using XInput
    DWORD result = XInputGetState(controllerIndex_, &xinputState_);
    currentState_.connected = (result == ERROR_SUCCESS);

    if (!currentState_.connected)
    {
        currentState_.reset();
        return false;
    }

    // Read analog sticks
    currentState_.leftStickX = xinputState_.Gamepad.sThumbLX;
    currentState_.leftStickY = xinputState_.Gamepad.sThumbLY;
    currentState_.rightStickX = xinputState_.Gamepad.sThumbRX;
    currentState_.rightStickY = xinputState_.Gamepad.sThumbRY;

    // Read triggers
    currentState_.leftTrigger = xinputState_.Gamepad.bLeftTrigger;
    currentState_.rightTrigger = xinputState_.Gamepad.bRightTrigger;

    // Read buttons
    currentState_.buttonA = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
    currentState_.buttonB = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
    currentState_.buttonX = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
    currentState_.buttonY = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
    currentState_.buttonLB = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    currentState_.buttonRB = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    currentState_.buttonBack = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    currentState_.buttonStart = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
    currentState_.buttonLeftStick = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
    currentState_.buttonRightStick = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;

    // Read D-pad
    currentState_.dpadUp = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    currentState_.dpadDown = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    currentState_.dpadLeft = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    currentState_.dpadRight = (xinputState_.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;

    return true;
#elif __APPLE__
    // macOS uses callback-based HID, so we just return the current state
    // The state is updated automatically via callbacks
    return currentState_.connected;
#else
    // Linux implementation
    if (joystickFd_ == -1)
    {
        currentState_.connected = false;
        return false;
    }

    js_event event;
    fd_set set;
    // struct timeval timeout;

    // FD_ZERO(&set);
    // FD_SET(joystickFd_, &set);
    // timeout.tv_sec = 0;
    // timeout.tv_usec = 0;

    // if (select(joystickFd_ + 1, &set, NULL, NULL, &timeout) > 0)
    // {
    //     ssize_t bytes = read(joystickFd_, &event, sizeof(event));
    //     if (bytes == sizeof(event))
    //     {
    //         return parseLinuxEvent(event);
    //     }
    // }
    while (read(joystickFd_, &event, sizeof(event)) > 0)
    {
        parseLinuxEvent(event);
    }

    return true;
#endif
}

#ifdef __APPLE__
bool XboxController::setupAppleHIDManager()
{
    hidManager_ = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!hidManager_)
    {
        std::cerr << "Failed to create HID manager" << std::endl;
        return false;
    }

    // Set up device matching criteria for Xbox controllers
    CFMutableArrayRef matchingArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    // Xbox controller vendor/product IDs
    const int vendorIDs[] = {0x045e}; // Microsoft
    const int productIDs[] = {0x028e, 0x028f, 0x02d1, 0x02dd, 0x02e0, 0x02ea, 0x0b00, 0x0b05, 0x0b06, 0x0b12, 0x0b13};

    for (size_t i = 0; i < sizeof(vendorIDs) / sizeof(vendorIDs[0]); ++i)
    {
        for (size_t j = 0; j < sizeof(productIDs) / sizeof(productIDs[0]); ++j)
        {
            CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                    &kCFTypeDictionaryKeyCallBacks,
                                                                    &kCFTypeDictionaryValueCallBacks);

            CFNumberRef vendorID = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendorIDs[i]);
            CFNumberRef productID = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &productIDs[j]);

            CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vendorID);
            CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), productID);

            CFArrayAppendValue(matchingArray, dict);

            CFRelease(vendorID);
            CFRelease(productID);
            CFRelease(dict);
        }
    }

    IOHIDManagerSetDeviceMatchingMultiple(hidManager_, matchingArray);
    CFRelease(matchingArray);

    // Set callbacks
    IOHIDManagerRegisterDeviceMatchingCallback(hidManager_, deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(hidManager_, deviceRemovedCallback, this);
    IOHIDManagerRegisterInputValueCallback(hidManager_, inputValueCallback, this);

    // Open HID manager
    IOReturn result = IOHIDManagerOpen(hidManager_, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess)
    {
        std::cerr << "Failed to open HID manager: " << result << std::endl;
        return false;
    }

    // Schedule with run loop
    IOHIDManagerScheduleWithRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);

    std::cout << "HID manager initialized for Xbox controllers" << std::endl;
    return true;
}

void XboxController::deviceAddedCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
    XboxController *controller = static_cast<XboxController *>(context);
    if (controller)
    {
        controller->currentState_.connected = true;
        std::cout << "Xbox controller connected" << std::endl;
    }
}

void XboxController::deviceRemovedCallback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
    XboxController *controller = static_cast<XboxController *>(context);
    if (controller)
    {
        controller->currentState_.connected = false;
        controller->currentState_.reset();
        std::cout << "Xbox controller disconnected" << std::endl;
    }
}

void XboxController::inputValueCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    XboxController *controller = static_cast<XboxController *>(context);
    if (controller)
    {
        controller->processAppleInput(value);
    }
}

void XboxController::processAppleInput(IOHIDValueRef value)
{
    IOHIDElementRef element = IOHIDValueGetElement(value);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    CFIndex intValue = IOHIDValueGetIntegerValue(value);

    // Map HID elements to Xbox controller state
    switch (usagePage)
    {
    case kHIDPage_GenericDesktop:
        switch (usage)
        {
        case kHIDUsage_GD_X:
            currentState_.leftStickX = intValue;
            break;
        case kHIDUsage_GD_Y:
            currentState_.leftStickY = intValue;
            break;
        case kHIDUsage_GD_Z:
            currentState_.rightStickX = intValue;
            break;
        case kHIDUsage_GD_Rz:
            currentState_.rightStickY = intValue;
            break;
        case kHIDUsage_GD_Hatswitch:
            // D-pad handling
            currentState_.dpadUp = (intValue == 0 || intValue == 1 || intValue == 7);
            currentState_.dpadRight = (intValue == 1 || intValue == 2 || intValue == 3);
            currentState_.dpadDown = (intValue == 3 || intValue == 4 || intValue == 5);
            currentState_.dpadLeft = (intValue == 5 || intValue == 6 || intValue == 7);
            break;
        }
        break;

    case kHIDPage_Button:
        switch (usage)
        {
        case 1:
            currentState_.buttonA = (intValue != 0);
            break;
        case 2:
            currentState_.buttonB = (intValue != 0);
            break;
        case 3:
            currentState_.buttonX = (intValue != 0);
            break;
        case 4:
            currentState_.buttonY = (intValue != 0);
            break;
        case 5:
            currentState_.buttonLB = (intValue != 0);
            break;
        case 6:
            currentState_.buttonRB = (intValue != 0);
            break;
        case 7:
            currentState_.buttonBack = (intValue != 0);
            break;
        case 8:
            currentState_.buttonStart = (intValue != 0);
            break;
        case 9:
            currentState_.buttonLeftStick = (intValue != 0);
            break;
        case 10:
            currentState_.buttonRightStick = (intValue != 0);
            break;
        }
        break;

    case kHIDPage_Simulation:
        switch (usage)
        {
        case kHIDUsage_Sim_Accelerator:
            currentState_.rightTrigger = intValue;
            break;
        case kHIDUsage_Sim_Brake:
            currentState_.leftTrigger = intValue;
            break;
        }
        break;
    }
}
#endif

#ifndef _WIN32
#ifndef __APPLE__
bool XboxController::findXboxController()
{
    const char *devicePaths[] = {
        // "/dev/input/js0",
        // "/dev/input/js1",
        // "/dev/input/js2",
        "/dev/input/js3",
        // "/dev/input/js4",
        nullptr};

    for (int i = 0; devicePaths[i] != nullptr; ++i)
    {
        joystickFd_ = open(devicePaths[i], O_RDONLY | O_NONBLOCK);
        if (joystickFd_ != -1)
        {
            devicePath_ = devicePaths[i];
            std::cout << "Found controller at: " << devicePath_ << std::endl;
            currentState_.connected = true;
            return true;
        }
    }

    std::cerr << "No Xbox controller found!" << std::endl;
    return false;
}

bool XboxController::parseLinuxEvent(const js_event &event)
{
    // Simplified Linux event parsing
    // Actual implementation would need proper axis/button mapping
    switch (event.type)
    {
    case JS_EVENT_BUTTON:
        if (event.number < 11)
        {
            // Map buttons
            bool pressed = (event.value != 0);
            switch (event.number)
            {
            case 0:
                currentState_.buttonA = pressed;
                break;
            case 1:
                currentState_.buttonB = pressed;
                break;
            case 2:
                currentState_.buttonX = pressed;
                break;
            case 3:
                currentState_.buttonY = pressed;
                break;
            case 4:
                currentState_.buttonLB = pressed;
                break;
            case 5:
                currentState_.buttonRB = pressed;
                break;
            case 6:
                currentState_.buttonBack = pressed;
                break;
            case 7:
                currentState_.buttonStart = pressed;
                break;
            case 8:
                currentState_.buttonLeftStick = pressed;
                break;
            case 9:
                currentState_.buttonRightStick = pressed;
                break;
            }
        }
        break;

    case JS_EVENT_AXIS:
        if (event.number < 6)
        {
            // Map axes
            switch (event.number)
            {
            case 0:
                currentState_.leftStickX = event.value;
                break;
            case 1:
                currentState_.leftStickY = event.value;
                break;
            case 3:
                currentState_.rightStickX = event.value;
                break;
            case 4:
                currentState_.rightStickY = event.value;
                break;
            case 2:
                currentState_.leftTrigger = (event.value + 32768) / 256;
                break;
            case 5:
                currentState_.rightTrigger = (event.value + 32768) / 256;
                break;
            }
        }
        break;
    }

    return true;
}
#endif
#endif

const XboxControllerState &XboxController::getState() const
{
    return currentState_;
}

bool XboxController::isConnected() const
{
    return currentState_.connected;
}

Json::Value XboxControllerState::toJson() const
{
    Json::Value json;

    json["left_stick"]["x"] = leftStickX;
    json["left_stick"]["y"] = leftStickY;
    json["right_stick"]["x"] = rightStickX;
    json["right_stick"]["y"] = rightStickY;

    json["triggers"]["left"] = leftTrigger;
    json["triggers"]["right"] = rightTrigger;

    json["buttons"]["A"] = buttonA;
    json["buttons"]["B"] = buttonB;
    json["buttons"]["X"] = buttonX;
    json["buttons"]["Y"] = buttonY;
    json["buttons"]["LB"] = buttonLB;
    json["buttons"]["RB"] = buttonRB;
    json["buttons"]["back"] = buttonBack;
    json["buttons"]["start"] = buttonStart;
    json["buttons"]["left_stick"] = buttonLeftStick;
    json["buttons"]["right_stick"] = buttonRightStick;

    json["dpad"]["up"] = dpadUp;
    json["dpad"]["down"] = dpadDown;
    json["dpad"]["left"] = dpadLeft;
    json["dpad"]["right"] = dpadRight;

    json["connected"] = connected;

    json["timestamp"] = static_cast<Json::UInt64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

    return json;
}