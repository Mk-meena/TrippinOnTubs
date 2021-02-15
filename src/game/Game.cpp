#include "Game.h"

void trippin::Game::init() {
    initRuntime();
    initWindowSize();
    initWindow();
    initRenderer();
    initConfiguration();
    initScale();
    initSpriteManager();
    initLevel();
}

void trippin::Game::initRuntime() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL could not initialize. SDL Error: %s", SDL_GetError());
        std::terminate();
    }
}

void trippin::Game::initWindowSize() {
    /*
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    windowSize = {displayMode.w, displayMode.h};
    */
    windowSize = {1600, 900};
}

void trippin::Game::initWindow() {
    window = SDL_CreateWindow(
            "Trippin on Tubs",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            windowSize.x,
            windowSize.y,
            SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        SDL_Log("Window could not be created. SDL Error: %s", SDL_GetError());
        std::terminate();
    }
}

void trippin::Game::initRenderer() {
    renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        SDL_Log("Renderer could not be created. SDL Error: %s", SDL_GetError());
        std::terminate();
    }
}

void trippin::Game::initConfiguration() {
    configuration.load(configName);
}

void trippin::Game::initScale() {
    auto &cs = configuration.scales;
    scale = &cs[0];
    for (auto it = cs.rbegin(); it != cs.rend(); it++) {
        if (windowSize.x >= it->minWidth) {
            scale = &(*it);
            break;
        }
    }
    SDL_Log("width=%d, height=%d, scale=%s", windowSize.x, windowSize.y, scale->name.c_str());
}

void trippin::Game::initSpriteManager() {
    spriteManager = std::make_unique<SpriteManager>(renderer, Scale{scale->name, scale->multiplier}, configuration.tickPeriod);
}

void trippin::Game::initLevel() {
    level.setWindowSize(windowSize);
    level.setConfiguration(&configuration);
    level.setScale(scale);
    level.setSpriteManager(spriteManager.get());
    level.init();
}

trippin::Game::Game(std::string configName) : configName(std::move(configName)) {

}

trippin::Game::~Game() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void trippin::Game::start() {
    level.start();
    renderLoop();
}

void trippin::Game::renderLoop() {
    Timer timer("renderer");
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        Level::Input input{};
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    input.spaceKeyDown = true;
                }
                if (e.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                    input.downKeyDown = true;
                }
            } else if (e.type == SDL_KEYUP) {
                if (e.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    input.spaceKeyUp = true;
                }
                if (e.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                    input.downKeyUp = true;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 247, 251, 255, 255);
        SDL_RenderClear(renderer);
        level.render(input);
        SDL_RenderPresent(renderer);
        timer.next();
    }
}
