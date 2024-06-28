#include <SDL.h>
#include <SDL_image.h>

int main(int argc, char* argv[]) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window = SDL_CreateWindow("My Game", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_Surface* playerSurface = SDL_LoadBMP("player.bmp");
  SDL_Texture* playerTexture = SDL_CreateTextureFromSurface(renderer, playerSurface);
  SDL_Rect playerRect = { 100, 100, playerSurface->w, playerSurface->h };

  bool quit = false;
  SDL_Event e;

  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, playerTexture, NULL, &playerRect);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(playerTexture);
  SDL_FreeSurface(playerSurface);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
```
