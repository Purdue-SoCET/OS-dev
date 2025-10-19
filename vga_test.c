#include <stdint.h>

#define VGA_FB_BASE 0xD0000000
#define VGA_WIDTH 320
#define VGA_HEIGHT 240

volatile uint32_t *vga_fb = (uint32_t *) VGA_FB_BASE;

void draw_test_pattern() {
    for(int y = 0; y < VGA_HEIGHT; y++) {
        for(int x = 0; x < VGA_WIDTH; x++) {
	    vga_fb[y * VGA_WIDTH + x] = 0xFF00000;
	}
    }
}

int main () {
    draw_test_pattern();
    while(1) {}
    return 0;
}
