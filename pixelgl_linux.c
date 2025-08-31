// This version uses opengl and a shader to get optimal performance
// it seems faster and we even added the random_backgound (which is random_colors in our bounce.c)
// 
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------- Globals ----------
Display *dpy = NULL;
Window win;
GLXContext ctx = NULL;
int win_w = 800, win_h = 600;
int running = 1;

uint32_t *framebuffer = NULL;
GLuint tex = 0, vao = 0, vbo = 0, prog = 0;

Atom wm_state, wm_fullscreen;

// Function pointers for modern GL (not in gl.h)
PFNGLCREATESHADERPROC        pglCreateShader = NULL;
PFNGLSHADERSOURCEPROC        pglShaderSource = NULL;
PFNGLCOMPILESHADERPROC       pglCompileShader = NULL;
PFNGLCREATEPROGRAMPROC       pglCreateProgram = NULL;
PFNGLATTACHSHADERPROC        pglAttachShader = NULL;
PFNGLLINKPROGRAMPROC         pglLinkProgram = NULL;
PFNGLDELETESHADERPROC        pglDeleteShader = NULL;
PFNGLUSEPROGRAMPROC          pglUseProgram = NULL;
PFNGLGENVERTEXARRAYSPROC     pglGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC     pglBindVertexArray = NULL;
PFNGLGENBUFFERSPROC          pglGenBuffers = NULL;
PFNGLBINDBUFFERPROC          pglBindBuffer = NULL;
PFNGLBUFFERDATAPROC          pglBufferData = NULL;
PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = NULL;
PFNGLGETUNIFORMLOCATIONPROC  pglGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC           pglUniform1i = NULL;
PFNGLDELETEBUFFERSPROC       pglDeleteBuffers = NULL;
PFNGLDELETEVERTEXARRAYSPROC  pglDeleteVertexArrays = NULL;

// Additional pointers for shader/program info
PFNGLGETSHADERIVPROC          pglGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC     pglGetShaderInfoLog = NULL;
PFNGLGETPROGRAMIVPROC         pglGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC    pglGetProgramInfoLog = NULL;

PFNGLDELETEPROGRAMPROC pglDeleteProgram = NULL;

// ---------- Helpers ----------
static void *glsym(const char *name) {
    void *p = (void*)glXGetProcAddressARB((const GLubyte*)name);
    if (!p) fprintf(stderr,"Failed load GL symbol: %s\n",name);
    return p;
}

static void load_gl_functions(void) {
    pglCreateShader = (PFNGLCREATESHADERPROC) glsym("glCreateShader");
    pglShaderSource = (PFNGLSHADERSOURCEPROC) glsym("glShaderSource");
    pglCompileShader = (PFNGLCOMPILESHADERPROC) glsym("glCompileShader");
    pglCreateProgram = (PFNGLCREATEPROGRAMPROC) glsym("glCreateProgram");
    pglAttachShader = (PFNGLATTACHSHADERPROC) glsym("glAttachShader");
    pglLinkProgram = (PFNGLLINKPROGRAMPROC) glsym("glLinkProgram");
    pglDeleteShader = (PFNGLDELETESHADERPROC) glsym("glDeleteShader");
    pglUseProgram = (PFNGLUSEPROGRAMPROC) glsym("glUseProgram");
    pglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) glsym("glGenVertexArrays");
    pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) glsym("glBindVertexArray");
    pglGenBuffers = (PFNGLGENBUFFERSPROC) glsym("glGenBuffers");
    pglBindBuffer = (PFNGLBINDBUFFERPROC) glsym("glBindBuffer");
    pglBufferData = (PFNGLBUFFERDATAPROC) glsym("glBufferData");
    pglVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) glsym("glVertexAttribPointer");
    pglEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) glsym("glEnableVertexAttribArray");
    pglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) glsym("glGetUniformLocation");
    pglUniform1i = (PFNGLUNIFORM1IPROC) glsym("glUniform1i");
    pglDeleteBuffers = (PFNGLDELETEBUFFERSPROC) glsym("glDeleteBuffers");
    pglDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) glsym("glDeleteVertexArrays");

    pglGetShaderiv = (PFNGLGETSHADERIVPROC) glsym("glGetShaderiv");
    pglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) glsym("glGetShaderInfoLog");
    pglGetProgramiv = (PFNGLGETPROGRAMIVPROC) glsym("glGetProgramiv");
    pglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) glsym("glGetProgramInfoLog");

    pglDeleteProgram = (PFNGLDELETEPROGRAMPROC) glsym("glDeleteProgram");
}

// ---------- Timing ----------
static long long now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec*1000LL + ts.tv_nsec/1000000LL;
}

// ---------- Shader helpers ----------
static GLuint compile_shader(GLenum type, const char *src){
    GLuint s = pglCreateShader(type);
    pglShaderSource(s,1,&src,NULL);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){
        char log[512]; pglGetShaderInfoLog(s,512,NULL,log);
        fprintf(stderr,"Shader compile error: %s\n",log);
        exit(1);
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs){
    GLuint pr = pglCreateProgram();
    pglAttachShader(pr, vs);
    pglAttachShader(pr, fs);
    pglLinkProgram(pr);
    GLint ok=0;
    pglGetProgramiv(pr, GL_LINK_STATUS, &ok);
    if(!ok){
        char log[512]; pglGetProgramInfoLog(pr,512,NULL,log);
        fprintf(stderr,"Program link error: %s\n",log);
        exit(1);
    }
    return pr;
}

// ---------- CPU Rasterizer ----------
// void drawFrame(float dt){
//     static float hue=0; hue+=dt*50; if(hue>255) hue-=255;
//     for(int y=0;y<win_h;y++){
//         uint32_t *row=framebuffer+y*win_w;
//         for(int x=0;x<win_w;x++){
//             uint8_t r=(x^y)&255;
//             uint8_t g=(int)hue&255;
//             uint8_t b=y&255;
//             row[x]=0xFF000000|(r)|(g<<8)|(b<<16);
//         }
//     }
// }


// ---------- Pixel helpers ----------
inline void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if(x<0 || x>=win_w || y<0 || y>=win_h) return;
    framebuffer[y*win_w + x] = 0xFF000000 | (r) | (g<<8) | (b<<16);
}

void clear_screen(uint8_t r, uint8_t g, uint8_t b){
    for(int y=0;y<win_h;y++){
        for(int x=0;x<win_w;x++){
            set_pixel(x,y,r,g,b);
        }
    }
}


#define RSIZE 2500
void random_background() {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned long pixel = 0;

// make lookup table for fast random colors
  uint32_t random_colors[RSIZE];
  for (int i = 0; i < RSIZE; i++) {
    r = rand() % 251;
    g = rand() % 251;
    b = rand() % 251;
    random_colors[i] = 0xFF000000 | (r) | (g<<8) | (b<<16);
  }

  int pos = 0;
  for (int y = 0; y < win_h; y++) {
    // unsigned int *row = (unsigned int *)(img->data + y * img->bytes_per_line);
    for (int x = 0; x < win_w; x++) {
      // pixel = random_colors[(x+y) % 1024]; //diagonal bands also nice effect
      pos = (pos + 13) % RSIZE;
      pixel = random_colors[pos];
      // row[x] = pixel;
     framebuffer[y*win_w + x] = pixel;
    }
  }
}



void draw_circle(int cx, int cy, int radius, unsigned char r,
                 unsigned char g, unsigned char b) {
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;
  while (y >= x) {
    set_pixel(cx + x, cy + y, r, g, b);
    set_pixel(cx - x, cy + y, r, g, b);
    set_pixel(cx + x, cy - y, r, g, b);
    set_pixel(cx - x, cy - y, r, g, b);
    set_pixel(cx + y, cy + x, r, g, b);
    set_pixel(cx - y, cy + x, r, g, b);
    set_pixel(cx + y, cy - x, r, g, b);
    set_pixel(cx - y, cy - x, r, g, b);
    x++;
    if (d > 0) {
      y--;
      d += 4 * (x - y) + 10;
    } else {
      d += 4 * x + 6;
    }
  }
}


void draw_filled_circle(int cx, int cy, int radius,
                        unsigned char r, unsigned char g, unsigned char b) {
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;

  while (y >= x) {
    // For each "row", draw a horizontal line between symmetric points
    for (int xi = cx - x; xi <= cx + x; xi++) {
      set_pixel(xi, cy + y, r, g, b);
      set_pixel(xi, cy - y, r, g, b);
    }
    for (int xi = cx - y; xi <= cx + y; xi++) {
      set_pixel(xi, cy + x, r, g, b);
      set_pixel(xi, cy - x, r, g, b);
    }

    x++;
    if (d > 0) {
      y--;
      d += 4 * (x - y) + 10;
    } else {
      d += 4 * x + 6;
    }
  }
}



int cx = 400; 
int cy = 300;

float vx = 750;
float vy = 450;

// ---------- Draw ----------
void drawFrame(float dt){
    static float hue = 0; 
    hue += dt * 50; 
    if(hue > 255) hue -= 255;

    // 1️⃣ Clear to black
    clear_screen(0,0,0);

    //uncomment to get random background
    // random_background();


    // update circle pos and draw
    // int radius = (win_h < win_w ? win_h : win_w)/4; // quarter of smaller dimension
    int radius = 40;

    uint8_t r = 255;               // circle color (red)
    uint8_t g = (int)hue & 255;    // green varies with hue
    uint8_t b = 128;               // constant blue
  
    cx += vx * dt;
    cy += vy * dt;

    if (cx - radius < 0) {
      cx = radius;
      vx = -vx;
    }
    if (cx + radius >= win_w) {
      cx = win_w - radius - 1;
      vx = -vx;
    }
    if (cy - radius < 0) {
      cy = radius;
      vy = -vy;
    }
    if (cy + radius >= win_h) {
      cy = win_h - radius - 1;
      vy = -vy;
    }

    // draw_circle(cx, cy, radius, r,g,b);
    draw_filled_circle(cx, cy, radius, r,g,b);
    
}



// ---------- GL setup ----------
const char *vs_src =
"#version 330 core\n"
"layout(location=0) in vec2 aPos;\n"
"layout(location=1) in vec2 aUV;\n"
"out vec2 vUV;\n"
"void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1); }\n";

const char *fs_src =
"#version 330 core\n"
"in vec2 vUV;\n"
"out vec4 fragColor;\n"
"uniform sampler2D uTex;\n"
"void main(){ fragColor=texture(uTex,vUV); }\n";

void gl_setup(){
    load_gl_functions();

    float verts[]={
        -1,-1,0,0,
         1,-1,1,0,
        -1, 1,0,1,
         1, 1,1,1
    };

    pglGenVertexArrays(1,&vao);
    pglBindVertexArray(vao);
    pglGenBuffers(1,&vbo);
    pglBindBuffer(GL_ARRAY_BUFFER,vbo);
    pglBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);

    pglVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    pglEnableVertexAttribArray(1);

    GLuint vs=compile_shader(GL_VERTEX_SHADER,vs_src);
    GLuint fs=compile_shader(GL_FRAGMENT_SHADER,fs_src);
    prog=link_program(vs,fs);
    pglDeleteShader(vs); pglDeleteShader(fs);

    glGenTextures(1,&tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,win_w,win_h,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,NULL);

    pglUseProgram(prog);
    int loc=pglGetUniformLocation(prog,"uTex");
    pglUniform1i(loc,0);
}

// ---------- Render ----------
void render_frame(){
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,win_w,win_h,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    pglUseProgram(prog);
    pglBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glXSwapBuffers(dpy,win);
}

// ---------- X11 + GLX setup ----------
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)(Display*,GLXFBConfig,GLXContext,Bool,const int*);
PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB=NULL;

GLXFBConfig choose_fb(Display *dpy){
    int attribs[]={GLX_X_RENDERABLE,True,GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
                   GLX_RENDER_TYPE,GLX_RGBA_BIT,GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
                   GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_ALPHA_SIZE,8,
                   GLX_DEPTH_SIZE,24,GLX_STENCIL_SIZE,8,GLX_DOUBLEBUFFER,True,None};
    int n; GLXFBConfig *fbs=glXChooseFBConfig(dpy,DefaultScreen(dpy),attribs,&n);
    if(!fbs||n==0){fprintf(stderr,"No FBConfig\n"); exit(1);}
    GLXFBConfig fb=fbs[0]; XFree(fbs); return fb;
}

void create_x_window_and_glx(){
    dpy=XOpenDisplay(NULL);
    if(!dpy){fprintf(stderr,"Cannot open display\n"); exit(1);}
    int screen=DefaultScreen(dpy);
    GLXFBConfig fb=choose_fb(dpy);
    XVisualInfo *vi=glXGetVisualFromFBConfig(dpy,fb);
    if(!vi){fprintf(stderr,"No visual\n"); exit(1);}
    XSetWindowAttributes swa;
    swa.colormap=XCreateColormap(dpy,RootWindow(dpy,screen),vi->visual,AllocNone);
    swa.event_mask=ExposureMask|KeyPressMask|StructureNotifyMask;
    win=XCreateWindow(dpy,RootWindow(dpy,screen),10,10,win_w,win_h,0,vi->depth,InputOutput,vi->visual,CWColormap|CWEventMask,&swa);
    XStoreName(dpy,win,"OpenGL Pixel Window");
    Atom wm_delete=XInternAtom(dpy,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(dpy,win,&wm_delete,1);
    XMapWindow(dpy,win);

    GLXContext temp_ctx=glXCreateContext(dpy,vi,0,GL_TRUE);
    glXMakeCurrent(dpy,win,temp_ctx);
    glXCreateContextAttribsARB=(PFNGLXCREATECONTEXTATTRIBSARBPROC)
        glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    int attribs[]={GLX_CONTEXT_MAJOR_VERSION_ARB,3,GLX_CONTEXT_MINOR_VERSION_ARB,3,
                   GLX_CONTEXT_PROFILE_MASK_ARB,GLX_CONTEXT_CORE_PROFILE_BIT_ARB,None};
    if(glXCreateContextAttribsARB){
        ctx=glXCreateContextAttribsARB(dpy,fb,0,True,attribs);
        glXMakeCurrent(dpy,0,0); glXDestroyContext(dpy,temp_ctx); glXMakeCurrent(dpy,win,ctx);}
    else ctx=temp_ctx;


    wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    wm_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

    XFree(vi);
}

// ---------- Event handling ----------
void handle_x_events(){
    while(XPending(dpy)){
        XEvent e; XNextEvent(dpy,&e);
        if(e.type==KeyPress){
            KeySym ks=XLookupKeysym(&e.xkey,0);
            if(ks==XK_q||ks==XK_Escape) running=0;
        }
        if(e.type==ConfigureNotify){
            if(win_w==e.xconfigure.width && win_h==e.xconfigure.height) continue;
            win_w=e.xconfigure.width; win_h=e.xconfigure.height;
            if(framebuffer) free(framebuffer);
            framebuffer=malloc((size_t)win_w*win_h*4);
            glViewport(0,0,win_w,win_h);
            glBindTexture(GL_TEXTURE_2D,tex);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,win_w,win_h,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,NULL);
        }
        if(e.type==ClientMessage){
            Atom wm_delete=XInternAtom(dpy,"WM_DELETE_WINDOW",False);
            if((Atom)e.xclient.data.l[0]==wm_delete) running=0;
        }

        if(e.type == KeyPress){
            KeySym ks = XLookupKeysym(&e.xkey, 0);
            if(ks == XK_q || ks == XK_Escape) running = 0;
            if(ks == XK_f || ks == XK_F){
                XEvent xev = {0};
                xev.type = ClientMessage;
                xev.xclient.window = win;
                xev.xclient.message_type = wm_state;
                xev.xclient.format = 32;
                xev.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
                xev.xclient.data.l[1] = wm_fullscreen;
                xev.xclient.data.l[2] = 0; // no second property
                xev.xclient.data.l[3] = 1; // normal source indication
                XSendEvent(dpy, DefaultRootWindow(dpy), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &xev);
            }
        }
    }
}

// ---------- main ----------
int main(){
    srand(time(NULL)); // seed for random numbers

    create_x_window_and_glx();
    framebuffer=malloc((size_t)win_w*win_h*4);
    memset(framebuffer,0,(size_t)win_w*win_h*4);
    gl_setup();

    long long last=now_ms();
    while(running){
        handle_x_events();
        long long t=now_ms();
        float dt=(t-last)/1000.0f; last=t;
        drawFrame(dt);
        render_frame();
        struct timespec req={0,2000000}; nanosleep(&req,NULL);
    }

    if(framebuffer) free(framebuffer);
    if(tex) glDeleteTextures(1,&tex);
    if(vbo) pglDeleteBuffers(1,&vbo);
    if(vao) pglDeleteVertexArrays(1,&vao);
    if(prog) pglDeleteProgram(prog);
    glXMakeCurrent(dpy,None,NULL);
    if(ctx) glXDestroyContext(dpy,ctx);
    XDestroyWindow(dpy,win);
    XCloseDisplay(dpy);
    return 0;
}
