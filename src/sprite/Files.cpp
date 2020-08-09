#include <sstream>
#include <exception>
#include "SDL.h"
#include "Files.h"

std::stringstream trippin::readFile(const char *fileName) {
    auto file = SDL_RWFromFile(fileName, "rb");
    if (file == nullptr) {
        SDL_Log("Warning: Unable to open file! SDL Error: %s", SDL_GetError());
        std::terminate();
    }

    std::stringstream contents;
    char buffer[fileBufferSize];
    int n;
    while ((n = SDL_RWread(file, buffer, sizeof(char), fileBufferSize)) > 0) {
        contents.write(buffer, n);
    }

    SDL_RWclose(file);

    return contents;
}
