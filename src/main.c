#include <SDL_render.h>
#include <SDL_surface.h>
#include <SDL_video.h>
#include <errno.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

#define MEMORY_MAX 4096
#define VARIABLE_MAX 16

#define DISPLAY_SIZE 64 * 32

typedef uint16_t addr_t;

typedef struct {
	uint8_t ram[MEMORY_MAX];
} memory_t;

memory_t memory_new() {
	return (memory_t){ .ram = { 0 } };
}

void memory_write(memory_t *memory, size_t size, const uint8_t *data) {
	if (memory == NULL) {
		printf("Error: invalid pointer");
	}
	
	int offset = 0x200;
	for (size_t i = 0; i < size;  i++) {
		memory->ram[offset] = data[i];
		offset++;
	}
}

typedef struct {
	uint8_t fb[32][64];
} display_t;

display_t display_new() {
	return (display_t){ .fb = { 0 }};
}

typedef struct {
	uint16_t pc;
	uint8_t  sp;

	uint16_t i;
	
	uint8_t delay_timer;
	uint8_t sound_timer;

	uint8_t v[VARIABLE_MAX];

	addr_t stack[100];
} reg_t;

reg_t register_new() {
	return (reg_t){
		.pc = (uint16_t)0x200,
		.sp = 0,
		.i  = 0,
		.delay_timer = 0,
		.sound_timer = 0,
		.v = { 0 },
		.stack = { 0 },
	};
}

typedef struct {
	reg_t reg;
	memory_t memory;
	display_t display;
} cpu_t;

cpu_t cpu_new() {
	return (cpu_t){ 
		.reg     = register_new(),
		.memory  = memory_new(),
		.display = display_new(),
	};
}

uint16_t cpu_fetch(cpu_t *cpu) {
	return (cpu->memory.ram[cpu->reg.pc] << 8) | (cpu->memory.ram[cpu->reg.pc + 1]);
}

void cpu_decode(cpu_t *cpu, uint16_t opcode) {
	printf("%.4X %.2X %-6.2X", cpu->reg.pc, opcode >> 8, opcode & 0x00FF);
	
	uint16_t addr = opcode & 0x0FFF;
	uint8_t nibble = opcode & 0x000F;
	uint8_t x = (opcode >> 8) & 0x000F;
	uint8_t y = (opcode >> 4) & 0x000F;
	uint8_t byte = opcode & 0x0FFF;

	switch (opcode & 0xF000) {
		case 0x0000:
			switch(opcode & 0x00FF) {
				case 0xE0:
					printf("CLS\n");
					memset(cpu->display.fb, 0, sizeof(cpu->display.fb[0][0]) * DISPLAY_SIZE);
					cpu->reg.pc += 2;
					break;
				default:
					printf("not reachable\n");
			}
			break;
		case 0x1000:
			printf("JP 0x%.3X\n", addr);
			cpu->reg.pc = addr;
			break;
		case 0x6000:
			printf("LD V[%d], 0x%.3X\n", x, byte);
			cpu->reg.v[x] = byte;
			cpu->reg.pc += 2;
			break;
		case 0x7000:
			printf("ADD V[%d], 0x%.3X\n", x, byte);
			cpu->reg.v[x] += byte;
			cpu->reg.pc += 2;
			break;
		case 0xA000:
			printf("LD I, 0x%.3X\n", addr);
			cpu->reg.i = addr;
			cpu->reg.pc += 2;
			break;
		case 0xD000:
			printf("DRW V[0x%X], V[0x%X], 0x%X\n", x, y, nibble);
			uint8_t sprite_x = cpu->reg.v[x] % 64;
			uint8_t sprite_y = cpu->reg.v[y] % 32;

			for (int height = 0; height < nibble; height++) {
				uint8_t sprite_row = cpu->memory.ram[cpu->reg.i + height];
				
				for (int width = 0; width <= 7; width++) {
					uint8_t pixel = (((sprite_row<<width) & 0x80) >> 7);
						
					cpu->display.fb[height + sprite_y][width + sprite_x] ^= pixel;
					if (cpu->display.fb[sprite_y][sprite_x] == 1) {
						cpu->reg.v[0xF]	= 0x1;
					} else { 
						cpu->reg.v[0xF]	= 0x0;
					}

					// x++;
				}
				// y++;
			}

			cpu->reg.pc += 2;

			break;
		default:
			printf("not reachable\n");
	}
}

long fsize(FILE *file) {
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	return size;
}

int load_rom(const char *rom, cpu_t *cpu) {
	if (rom == NULL) {
		errno = ENOENT;
		return EXIT_FAILURE;
	}

	FILE *fp = fopen(rom, "r");
	if (fp == NULL) {
		return EXIT_FAILURE;
	}

	long size = fsize(fp);

	uint8_t bytes[size];
	const size_t count = fread(bytes, sizeof(bytes[0]), size, fp);

	if (count == size) {
		memory_write(&cpu->memory, ARRAY_LENGTH(bytes), bytes);
	} else {
		if (feof(fp)) {
			printf("Error reading %s: unexpected eof\n", rom);
		}
	}

	fclose(fp);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	// if (argc < 2) {
	// 	printf("pixel8 ./ROM_FILE.c8\n");
	// 	return EXIT_FAILURE;
	// }

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Pixel-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }


    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

	SDL_RenderSetScale(renderer, WINDOW_WIDTH / 64, WINDOW_HEIGHT / 32);

	cpu_t cpu = cpu_new();
	
	int error = load_rom("../IBMLOGO.c8", &cpu);
	if (error == EXIT_FAILURE) {
		printf("Failed to load file");
	}

	SDL_Event e;
	
	while (true) {
		if (SDL_WaitEvent(&e)) {
			if (e.type == SDL_QUIT) {
				break;
			}
		}

		uint16_t opcode = cpu_fetch(&cpu);

		cpu_decode(&cpu, opcode);

		SDL_RenderClear(renderer);
		
		for (int i = 0; i < 32; i++) {
			for (int j = 0; j < 64; j++) {
				SDL_Rect rect = { .x = j , .y = i, .h = 10, .w = 10};

				if (cpu.display.fb[i][j] == 1) {
					SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
					SDL_RenderFillRect(renderer, &rect);
				} else if (cpu.display.fb[i][j] == 0) {
					SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
					SDL_RenderFillRect(renderer, &rect);
				}
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

		SDL_RenderPresent(renderer);
	}

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

	return EXIT_SUCCESS;
}
