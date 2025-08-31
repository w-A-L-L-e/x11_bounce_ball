
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <GL/gl.h>
#pragma comment(lib, "opengl32.lib")

// Global state
static int win_width = 800;
static int win_height = 600;
static int running = 1;
static int fullscreen = 0;

static HDC hDC;
static HGLRC hRC;
static HWND hwnd;
static uint32_t *framebuffer = NULL;
static GLuint tex = 0;

// Timing
LONGLONG now_ms() {
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (t.QuadPart * 1000) / freq.QuadPart;
}

// User drawing (CPU rasterizer)
void drawFrame(float dt) {
    static float hue = 0;
    hue += dt * 50;
    if (hue > 255) hue -= 255;

    for (int y = 0; y < win_height; y++) {
        uint32_t *row = framebuffer + y * win_width;
        for (int x = 0; x < win_width; x++) {
            uint8_t r = (x ^ y) & 255;
            uint8_t g = (int)hue & 255;
            uint8_t b = y & 255;
            row[x] = 0xFF000000 | (r) | (g << 8) | (b << 16);
        }
    }
}

// Toggle fullscreen
void toggle_fullscreen() {
    static WINDOWPLACEMENT prev = { sizeof(prev) };
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (!fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &prev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen = 1;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &prev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen = 0;
    }
}

// Handle Win32 messages
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY: running = 0; PostQuitMessage(0); return 0;
    case WM_SIZE: {
        win_width = LOWORD(lParam);
        win_height = HIWORD(lParam);
        if (framebuffer) free(framebuffer);
        framebuffer = malloc(win_width * win_height * 4);
        glViewport(0, 0, win_width, win_height);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_width, win_height, 0,
                     GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == 'Q' || wParam == VK_ESCAPE) {
            running = 0;
            PostQuitMessage(0);
        }
        if (wParam == 'F') toggle_fullscreen();
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Setup GL
void initGL() {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0,0,0,0,0,0,0,0,0,0,0,0,0,
        24, 0, 0,
        PFD_MAIN_PLANE, 0, 0,0,0
    };
    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    framebuffer = malloc(win_width * win_height * 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_width, win_height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
}

// Render loop
void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, win_width, win_height,
                    GL_BGRA_EXT, GL_UNSIGNED_BYTE, framebuffer);

    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f( 1, -1);
        glTexCoord2f(0, 1); glVertex2f(-1,  1);
        glTexCoord2f(1, 1); glVertex2f( 1,  1);
    glEnd();

    SwapBuffers(hDC);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "GLWin";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hwnd = CreateWindow("GLWin", "OpenGL Pixel Window",
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        win_width, win_height,
                        NULL, NULL, hInst, NULL);
    hDC = GetDC(hwnd);
    initGL();

    MSG msg;
    LONGLONG last = now_ms();

    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LONGLONG t = now_ms();
        float dt = (t - last) / 1000.0f;
        last = t;

        drawFrame(dt);
        render();
        Sleep(1);
    }

    if (framebuffer) free(framebuffer);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hwnd, hDC);
    DestroyWindow(hwnd);
    return 0;
}
