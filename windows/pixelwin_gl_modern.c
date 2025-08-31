
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include "GL/wglext.h"
#include "GL/glext.h"

// Link to OpenGL
#pragma comment(lib, "opengl32.lib")

// Globals
static int win_width = 800, win_height = 600;
static int running = 1, fullscreen = 0;
static HWND hwnd; static HDC hDC; static HGLRC hRC;
static uint32_t *framebuffer = NULL;
static GLuint tex = 0, vao = 0, vbo = 0, shaderProgram = 0;

// Timing
LONGLONG now_ms() {
    static LARGE_INTEGER freq; static int init = 0;
    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (t.QuadPart * 1000) / freq.QuadPart;
}

// --- Shader helpers ---
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
        MessageBoxA(NULL, log, "Shader Compile Error", MB_OK);
        exit(1);
    }
    return s;
}

GLuint createProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(prog, 512, NULL, log);
        MessageBoxA(NULL, log, "Program Link Error", MB_OK);
        exit(1);
    }
    glDeleteShader(v); glDeleteShader(f);
    return prog;
}

// --- Fullscreen toggle ---
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

// --- Window procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY: running = 0; PostQuitMessage(0); return 0;
    case WM_SIZE: {
        win_width = LOWORD(lParam); win_height = HIWORD(lParam);
        if (framebuffer) free(framebuffer);
        framebuffer = malloc(win_width * win_height * 4);
        glViewport(0, 0, win_width, win_height);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_width, win_height, 0,
                     GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == 'Q' || wParam == VK_ESCAPE) { running = 0; PostQuitMessage(0); }
        if (wParam == 'F') toggle_fullscreen();
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- User rasterizer (CPU writes framebuffer) ---
void drawFrame(float dt) {
    static float hue = 0; hue += dt * 50; if (hue > 255) hue -= 255;
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

// --- GL setup ---
void initGL() {
    // Simple quad vertices: pos (x,y), tex (u,v)
    float verts[] = {
        -1, -1, 0, 0,
         1, -1, 1, 0,
        -1,  1, 0, 1,
         1,  1, 1, 1,
    };

    const char* vs_src =
        "#version 330 core\n"
        "layout(location=0) in vec2 pos;"
        "layout(location=1) in vec2 uv_in;"
        "out vec2 uv;"
        "void main(){ uv=uv_in; gl_Position=vec4(pos,0,1); }";

    const char* fs_src =
        "#version 330 core\n"
        "in vec2 uv;"
        "out vec4 FragColor;"
        "uniform sampler2D tex;"
        "void main(){ FragColor=texture(tex, uv); }";

    shaderProgram = createProgram(vs_src, fs_src);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    framebuffer = malloc(win_width * win_height * 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_width, win_height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
}

// --- Render ---
void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, win_width, win_height,
                    GL_BGRA_EXT, GL_UNSIGNED_BYTE, framebuffer);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SwapBuffers(hDC);
}

// --- Create GL context with wgl extensions ---
void initContext(HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32 };
    hDC = GetDC(hwnd);
    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    HGLRC temp = wglCreateContext(hDC);
    wglMakeCurrent(hDC, temp);

    // Load WGL extension for modern context
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    hRC = wglCreateContextAttribsARB(hDC, 0, attribs);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(temp);
    wglMakeCurrent(hDC, hRC);
}

// --- Entry ---
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = "GLWin"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hwnd = CreateWindow("GLWin", "OpenGL Pixel Window (Shaders)",
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        win_width, win_height,
                        NULL, NULL, hInst, NULL);

    initContext(hwnd);
    initGL();

    MSG msg; LONGLONG last = now_ms();
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        LONGLONG t = now_ms();
        float dt = (t - last) / 1000.0f; last = t;
        drawFrame(dt);
        render();
        Sleep(1);
    }

    if (framebuffer) free(framebuffer);
    wglMakeCurrent(NULL, NULL); wglDeleteContext(hRC); ReleaseDC(hwnd, hDC);
    DestroyWindow(hwnd);
    return 0;
}
