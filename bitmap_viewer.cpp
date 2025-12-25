#include <SDL2/SDL.h>
#include <stdio.h>
#include "bitmap.h"
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Uso: %s imagem.bmp\n", argv[0]);
        return 1;
    }

    // 1) Inicializa SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

	BMP bmp(argv[1]);
    // 2) Carrega BMP (TEMPORÁRIO — depois você troca pelo seu parser)
	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
	    bmp.data.data(),                     // pixels
	    bmp.info_header.width,               // width
	    bmp.info_header.height,              // height
	    bmp.info_header.bit_count,           // depth (32)
	    bmp.info_header.width * 4,           // pitch (bytes por linha)
	    SDL_PIXELFORMAT_BGRA32           // formato
	);
    if (!surface) {
        printf("SDL_LoadBMP error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
	
	int scale = 2;

    // 3) Cria janela do tamanho da imagem
    SDL_Window* window = SDL_CreateWindow(
        "BMP Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        surface->w * scale,
        surface->h * scale,
        0
    );

    // 4) Renderer + texture
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    // 5) Desenha UMA vez
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    // 6) Espera fechar (sem loop de redraw)
    SDL_Event e;
    int done = 0;
    while (!done) {
        if (SDL_WaitEvent(&e)) {
            if (e.type == SDL_QUIT)
                done = 1;
        }
    }

    // 7) Limpa tudo
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
