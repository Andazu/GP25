#include <stdlib.h>
#include <SDL3/SDL.h>

struct SDLApplication { // This struct should hold all our states
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
        SDL_Event event; // Struct for all events in SDL (Inputs like mouse/keyboard, OS events and more)
        const bool *keys = SDL_GetKeyboardState(nullptr);

        // Just to have the application run forever
        while (running) {
            while (SDL_PollEvent(&event)) {
                // Basically takes an event from the event queue and handles it

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
    }

    void MainLoop() {
        while (running) {
            Tick();
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
