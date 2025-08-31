
// main.c
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

int win_width = 800;
int win_height = 600;
uint32_t *framebuffer = NULL;   // ARGB little-endian (BGRA for GL upload path)

// High-res monotonic-ish timer (ms)
static long long now_ms() {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

// Called every frame by the Cocoa/GL layer
void drawFrame(float dt) {
    if (!framebuffer) return;

    static float hue = 0;
    hue += dt * 50.0f;
    if (hue > 255.0f) hue -= 255.0f;

    for (int y = 0; y < win_height; ++y) {
        uint32_t *row = framebuffer + (size_t)y * win_width;
        for (int x = 0; x < win_width; ++x) {
            uint8_t r = (uint8_t)((x ^ y) & 255);
            uint8_t g = (uint8_t)hue;
            uint8_t b = (uint8_t)(y & 255);
            // ARGB little-endian in memory == BGRA bytes
            row[x] = (0xFFu << 24) | (b << 16) | (g << 8) | (r);
        }
    }
}
