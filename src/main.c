#include <errno.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

#define MEMORY_MAX 4096
#define VARIABLE_MAX 0xF

#define DISPLAY_SIZE 64 * 32

typedef uint16_t addr_t;

typedef struct {
	uint8_t ram[MEMORY_MAX];
} memory_t;

memory_t memory_new() {
	memory_t m = { .ram = { 0 } };
	return m;
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
	uint8_t fb[DISPLAY_SIZE];
} display_t;

display_t display_new() {
	display_t d = { .fb = { 0 }};
	return d;
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
	reg_t r = {
		.pc = (uint16_t)0x200,
		.sp = 0,
		.i  = 0,
		.delay_timer = 0,
		.sound_timer = 0,
		.v = { 0 },
		.stack = { 0 },
	};
	return r;
}

typedef struct {
	reg_t reg;
	memory_t memory;
	display_t display;
} cpu_t;

cpu_t cpu_new() {
	cpu_t c = { 
		.reg     = register_new(),
		.memory  = memory_new(),
		.display = display_new(),
	};
	return c;
}

uint16_t cpu_fetch(cpu_t *cpu) {
	uint16_t instruction = (cpu->memory.ram[cpu->reg.pc] << 8) | (cpu->memory.ram[cpu->reg.pc + 1]);
	return instruction;
}

void cpu_decode(cpu_t *cpu, uint16_t opcode) {
	printf("[PC: 0x%.4d OP: 0x%.4X] ", cpu->reg.pc, opcode);
	
	uint16_t addr = opcode & 0x0FFF;
	uint8_t nibble = opcode & 0x000F;
	uint8_t x = (opcode >> 8) & 0x0F;
	uint8_t y = (opcode >> 4) & 0x00F;
	uint8_t byte = opcode & 0x00FF;

	switch (opcode & 0xF000) {
		case 0x0000:
			switch(opcode & 0x00FF) {
				case 0xE0:
					printf("CLS\n");
					cpu->reg.pc += 2;
					break;
				default:
					printf("not reachable\n");
			}
			break;
		case 0x1000:
			printf("jP 0x%.3X\n", addr);
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
			printf("draw\n");
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

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Pixel-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

	cpu_t cpu = cpu_new();
	
	int error = load_rom("../IBMLOGO.c8", &cpu);
	if (error == EXIT_FAILURE) {
		printf("Failed to load file");
	}

	while (true) {
		SDL_Event e;
		if (SDL_WaitEvent(&e)) {
			if (e.type == SDL_QUIT) {
				break;
			}
		}
		uint16_t opcode = cpu_fetch(&cpu);
		cpu_decode(&cpu, opcode);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

	return EXIT_SUCCESS;
}
