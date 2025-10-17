#include <stdlib.h>
#include <SDL3/SDL.h>
#include <string>

#include "itu_common.hpp"

struct SDLApplication {
    // This struct should hold all our states
    SDL_Window *window;
    // A pointer to an SDL_Window which is a struct defined inside SDL. Holds data like title, size position and OS-specifc handlers
    bool running = true;

    // Constructor - runs automatically when we create the object
    SDLApplication() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            // initializes the SDL subsystems you define in the flags. Here we turn on the video subsystem for SDL
            SDL_Log("Failed to initialize SDL: %s", SDL_GetError()); // Some error handling since SDL_Init is a bool
            return;
        }
        window = SDL_CreateWindow("Anders - SDL3", 320, 240, SDL_WINDOW_RESIZABLE);
        // Create a window, parameters are title, width, height and flags
    }

    // Destructor - runs automatically when the object goes out fo scope (When main() ends)
    ~SDLApplication() {
        SDL_Quit(); // Turn off and clean up the subsystems
    }

    // Abstraction that advances our loop one iteration (One frame in most games)
    void Tick() {
        Input();
        Update();
        Render();
    }

    // Handle input events from I/O or networking devices
    void Input() {
        SDL_Event event; // Struct for all events in SDL (Inputs like mouse/keyboard, OS events and more)
        const bool *keys = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event)) { // Basically takes an event from the event queue and handles it

            // Keyboard and OS events
            if (event.type == SDL_EVENT_QUIT) {
                // If we press the X on the window
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                // If we press a key
                SDL_Log("a key was pressed: %d", event.key.key);
            }

            // Mouse events
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                SDL_Log("Mouse position X = %f, Y = %f", event.motion.x, event.motion.y);
                // Event.motion gives access to the SDL_MouseMotionEvent struct that has things like x and y positions
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                SDL_Log("Mouse button clicked: %d", event.button.clicks);
                // Event.button gives access to the SDL_MouseButtonEvent struct which has "clicks" among other things
            }
        }

        // Another way of checking for key presses is to use GetKeyboardState
        if (keys[SDL_SCANCODE_F1] == true) {
            SDL_Log("Key down\n");
        }
    }

    void Update() {
    }

    void Render() {
    }

    void MainLoop() {
        const Uint64 targetFrameNs = 1'000'000'000ull / 60; // 60 FPS target
        Uint64 fps = 0;
        Uint64 lastFpsUpdateNs = SDL_GetTicksNS();
        Uint64 prevFrameStamp = SDL_GetTicksNS(); // for deltaTime

        while (running) {
            const Uint64 frameStart = SDL_GetTicksNS();

            // delta time in seconds for gameplay updates
            double deltaTime = (frameStart - prevFrameStamp) / 1e9;
            prevFrameStamp = frameStart;

            Tick(); // must be ONE frame of work (poll events, update, render), no inner while

            const Uint64 workTime = SDL_GetTicksNS() - frameStart;
            if (workTime < targetFrameNs) {
                // Sleep most of the remaining time…
                Uint64 remaining = targetFrameNs - workTime;

                // Optional safety margin (1 ms) + busy-wait for tight pacing
                const Uint64 safety = 1'000'000ull; // 1 ms in ns
                if (remaining > safety) {
                    SDL_DelayNS(remaining - safety);
                }

                // Busy-wait the tiny tail to hit the target more closely
                while (SDL_GetTicksNS() - frameStart < targetFrameNs) { /* spin */ }
            }

            // FPS counter once per second (1e9 ns)
            ++fps;
            const Uint64 nowNs = SDL_GetTicksNS();
            if (nowNs - lastFpsUpdateNs >= 1'000'000'000ull) {
                SDL_SetWindowTitle(window, ("Anders cool SDL3 - FPS: " + std::to_string(fps)).c_str());
                fps = 0;
                lastFpsUpdateNs = nowNs;
            }
        }
    }

};

// Entry point
int main() {
    // Abstracted the loop so its more testable
    SDLApplication app;
    app.MainLoop();
    return 0;
}
