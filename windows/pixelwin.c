// to compile in wondows use:
// x86_64-w64-mingw32-gcc -o pixelwin.exe pixelwin.c -lgdi32
//
// this is just some gpt generated code using the linux example. not tested yet...
//
// for smaller binary:
//  x86_64-w64-mingw32-gcc -mwindows -s -o pixelwin.exe pixelwin.c -lgdi32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int win_width = 800;
static int win_height = 600;
static int running = 1;
static int fullscreen = 0;

static BITMAPINFO bmi;
static uint32_t *framebuffer = NULL;
static HWND hwnd;
static HDC hdcMem;

LONGLONG now_ms() {
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (t.QuadPart * 1000) / freq.QuadPart;
}

void draw(float dt, uint32_t *pixels) {
    // Example: fill with animated color
    static float hue = 0;
    hue += dt * 50;
    if (hue > 360) hue -= 360;

    for (int y = 0; y < win_height; y++) {
        for (int x = 0; x < win_width; x++) {
            uint8_t r = (uint8_t)((x ^ y) & 255);
            uint8_t g = (uint8_t)((int)hue & 255);
            uint8_t b = (uint8_t)(y & 255);
            pixels[y * win_width + x] = (r) | (g << 8) | (b << 16);
        }
    }
}

void toggle_fullscreen(HWND hwnd) {
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        running = 0;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == 'Q' || wParam == VK_ESCAPE) {
            running = 0;
            PostQuitMessage(0);
        }
        if (wParam == 'F') {
            toggle_fullscreen(hwnd);
        }
        return 0;
    case WM_SIZE:
        win_width = LOWORD(lParam);
        win_height = HIWORD(lParam);

        if (framebuffer) free(framebuffer);
        framebuffer = malloc(win_width * win_height * 4);

        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = win_width;
        bmi.bmiHeader.biHeight = -win_height; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MyWin32Window";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hwnd = CreateWindow("MyWin32Window", "Pixel Window",
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        win_width, win_height,
                        NULL, NULL, hInstance, NULL);

    HDC hdc = GetDC(hwnd);

    // Initialize framebuffer
    framebuffer = malloc(win_width * win_height * 4);
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = win_width;
    bmi.bmiHeader.biHeight = -win_height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

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

        draw(dt, framebuffer);

        StretchDIBits(hdc, 0, 0, win_width, win_height,
                      0, 0, win_width, win_height,
                      framebuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);

        Sleep(2);
    }

    free(framebuffer);
    ReleaseDC(hwnd, hdc);
    return 0;
}
