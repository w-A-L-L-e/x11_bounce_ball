
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

int win_width = 800;
int win_height = 600;
uint32_t *framebuffer = NULL;

long long now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

// called each frame by Cocoa layer
void drawFrame(float dt) {
    if (!framebuffer) return;
    static float hue = 0;
    hue += dt * 50;
    if (hue > 255) hue -= 255;

    for (int y = 0; y < win_height; y++) {
        for (int x = 0; x < win_width; x++) {
            uint8_t r = (x ^ y) & 255;
            uint8_t g = ((int)hue) & 255;
            uint8_t b = y & 255;
            framebuffer[y * win_width + x] = r | (g << 8) | (b << 16);
        }
    }
}
