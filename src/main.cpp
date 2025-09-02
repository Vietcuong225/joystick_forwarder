#include "xbox_controller.h"
#include <zenoh.hxx>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include <jsoncpp/json/json.h>

std::atomic<bool> running(true);

void signalHandler(int signal)
{
    running = false;
}

int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Peer mode, UDP multicast, use discovery process
    zenoh::Config config = zenoh::Config::create_default();
    std::unique_ptr<zenoh::Session> session;
    try
    {
        session = std::make_unique<zenoh::Session>(zenoh::Session::open(std::move(config)));
        std::cout << "Zenoh session opened" << std::endl;
    }
    catch (const zenoh::ZException::exception e)
    {
        std::cerr << "Zenoh session failed: " << e.what() << std::endl;
        return 1;
    }

    XboxController controller(0);

    if (!controller.initialize())
    {
        std::cerr << "Failed to initialize Xbox controller!" << std::endl;
        return 1;
    }

    std::cout << "Xbox Joystick Forwarder started. Press Ctrl+C to exit." << std::endl;
    std::cout << "Publishing controller data on topic spidercam/joystick" << std::endl;

#ifdef __APPLE__
    std::cout << "macOS detected: Using HID manager with callbacks" << std::endl;
    // On macOS, we need to run the main run loop for callbacks
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
#endif

    while (running)
    {
        if (controller.update())
        {
            XboxControllerState state = controller.getState();
            Json::Value jsonData = state.toJson();

            Json::StreamWriterBuilder writer;
            std::string jsonString = Json::writeString(writer, jsonData);

            std::cout << jsonString << std::endl;

            try
            {
                // zmq::message_t message(jsonString.size());
                // memcpy(message.data(), jsonString.c_str(), jsonString.size());
                session->put(zenoh::KeyExpr("spidercam/joystick"), zenoh::Bytes(jsonString));

                // Optional: Print for debugging
                // std::cout << jsonString << std::endl;
            }
            catch (const zenoh::ZException::exception e)
            {
                std::cerr << "ZeroMQ send error: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "Controller not connected. Waiting..." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));

#ifdef __APPLE__
        // Process macOS events
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, false);
#endif
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}