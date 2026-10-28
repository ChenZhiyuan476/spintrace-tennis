#include "game.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>

int main(int, char **) {
    SDL_SetAppMetadata("Tennis Game", "1.0.0", "com.tennisgame.app");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Tennis Game - C++ / Metal",
        Game::Width,
        Game::Height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, "metal");
    if (!renderer) {
        std::fprintf(stderr, "Metal renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // Fill the complete display while preserving the 16:9 game aspect. On
    // taller Mac fullscreen surfaces this crops a small amount from the far
    // left/right surround instead of creating unpaintable top/bottom bars.
    SDL_SetRenderLogicalPresentation(renderer, Game::Width, Game::Height,
                                     SDL_LOGICAL_PRESENTATION_OVERSCAN);
    SDL_SetRenderVSync(renderer, 1);
    std::printf("Renderer: %s\n", SDL_GetRendererName(renderer));

    {
        // Keep Game (and its audio stream) inside the SDL lifetime.
        Game game;
        bool running = true;
        constexpr double fixedStep = 1.0 / 120.0;
        double accumulator = 0.0;
        Uint64 previousTime = SDL_GetTicksNS();

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                    continue;
                }
                SDL_ConvertEventToRenderCoordinates(renderer, &event);
                game.handleEvent(event);
            }
            if (game.wantsToQuit()) break;

            const Uint64 currentTime = SDL_GetTicksNS();
            const double frameTime = std::clamp((currentTime - previousTime) / 1'000'000'000.0, 0.0, 0.05);
            previousTime = currentTime;
            accumulator += frameTime;

            const bool *keys = SDL_GetKeyboardState(nullptr);
            while (accumulator >= fixedStep) {
                game.update(fixedStep, keys);
                accumulator -= fixedStep;
            }

            game.render(renderer);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
