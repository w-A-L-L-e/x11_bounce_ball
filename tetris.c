//
// Author: Walter Schreppers
//  This is not completed nor clean at all yet. but just wanted to see adding things like draw_block
//  would be easy... if I feel a little bored I might make it into an actual tetris some day :D

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// height/width are updated when window is resized
int win_width = 800;
int win_height = 600;

// circle props in some globals to keep it all simple
float cx = 400;
float cy = 300;
float vx = 450.0f, vy = 420.0f;
int radius = 40;


int random_background = 0;
int filled_circle = 1;



// Direct pixel write into XImage->data (32-bit TrueColor)
static inline void set_pixel(XImage *img, int x, int y, unsigned char r,
                             unsigned char g, unsigned char b) {
  if (x < 0 || x >= win_width || y < 0 || y >= win_height)
    return;
  unsigned long pixel = (r << 16) | (g << 8) | b; // RGB24
  unsigned int *dst =
      (unsigned int *)(img->data + y * img->bytes_per_line + x * 4);
  *dst = pixel;
}

void clear_image(XImage *img, unsigned char r, unsigned char g, unsigned char b) {
  unsigned long pixel = (r << 16) | (g << 8) | b;
  for (int y = 0; y < win_height; y++) {
    unsigned int *row = (unsigned int *)(img->data + y * img->bytes_per_line);
    for (int x = 0; x < win_width; x++)
      row[x] = pixel;
  }
}

#define RSIZE 2500
void random_image(XImage *img) {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned long pixel = 0;

// make lookup table for fast random colors
  unsigned long random_colors[RSIZE];
  for (int i = 0; i < RSIZE; i++) {
    r = rand() % 251;
    g = rand() % 251;
    b = rand() % 251;
    random_colors[i] = (r << 16) | (g << 8) | b;
  }

  int pos = 0;
  for (int y = 0; y < win_height; y++) {
    unsigned int *row = (unsigned int *)(img->data + y * img->bytes_per_line);
    for (int x = 0; x < win_width; x++) {
      // pixel = random_colors[(x+y) % 1024]; //diagonal bands also nice effect
      pos = (pos + 13) % RSIZE;
      pixel = random_colors[pos];
      row[x] = pixel;
    }
  }
}

// Bresenham circle
void draw_circle(XImage *img, int cx, int cy, int radius, unsigned char r,
                 unsigned char g, unsigned char b) {
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;
  while (y >= x) {
    set_pixel(img, cx + x, cy + y, r, g, b);
    set_pixel(img, cx - x, cy + y, r, g, b);
    set_pixel(img, cx + x, cy - y, r, g, b);
    set_pixel(img, cx - x, cy - y, r, g, b);
    set_pixel(img, cx + y, cy + x, r, g, b);
    set_pixel(img, cx - y, cy + x, r, g, b);
    set_pixel(img, cx + y, cy - x, r, g, b);
    set_pixel(img, cx - y, cy - x, r, g, b);
    x++;
    if (d > 0) {
      y--;
      d += 4 * (x - y) + 10;
    } else {
      d += 4 * x + 6;
    }
  }
}

// Filled Bresenham circle: draws a solid disk
void draw_filled_circle(XImage *img, int cx, int cy, int radius,
                        unsigned char r, unsigned char g, unsigned char b) {
  int x = 0;
  int y = radius;
  int d = 3 - 2 * radius;

  while (y >= x) {
    // For each "row", draw a horizontal line between symmetric points
    for (int xi = cx - x; xi <= cx + x; xi++) {
      set_pixel(img, xi, cy + y, r, g, b);
      set_pixel(img, xi, cy - y, r, g, b);
    }
    for (int xi = cx - y; xi <= cx + y; xi++) {
      set_pixel(img, xi, cy + x, r, g, b);
      set_pixel(img, xi, cy - x, r, g, b);
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

void draw_block(
  XImage* img, 
  int x, int y, int width, int height, 
  unsigned char r, unsigned char g, unsigned char b
) {
    
  for(int xpos = x; xpos < x+width; xpos++){
    for(int ypos = y; ypos < y+height; ypos++){
      set_pixel(img, xpos, ypos, r,g,b);
    }
  }
}




void draw(float dt, XImage* buffer){
  // Update physics
  cx += vx * dt;
  cy += vy * dt;
  if (cx - radius < 0) {
    cx = radius;
    vx = -vx;
  }
  if (cx + radius >= win_width) {
    cx = win_width - radius - 1;
    vx = -vx;
  }
  if (cy - radius < 0) {
    cy = radius;
    vy = -vy;
  }
  if (cy + radius >= win_height) {
    cy = win_height - radius - 1;
    vy = -vy;
  }

  // Draw frame
  clear_image(buffer, 25, 25, 25);

  if (random_background)
    random_image(buffer);

  draw_block(buffer, 20, 20, 100,100, 200, 10,10);


  if (filled_circle) {
    draw_filled_circle(buffer, (int)cx, (int)cy, radius, 230, 230, 0);
  } else {
    draw_circle(buffer, (int)cx, (int)cy, radius, 230, 230, 0);
  }


}



// time in ms
long long now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main() {
  srand(time(NULL)); // seed for random numbers

  printf("Bouncy the ball\n");
  printf("===============\n\n");

  printf("Keyboard shortcuts:\n\n");
  printf("f : full screen toggle \n");
  printf("r : randomize background toggle \n");
  printf("c : filled circle toggle \n");
  printf("q : quit application \n\n");

  printf(
      "TIP: you can live resize the window and the ball stays in bounds\n\n");

  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "Cannot open display\n");
    return 1;
  }
  int screen = DefaultScreen(dpy);

  Window win = XCreateSimpleWindow(
      dpy, RootWindow(dpy, screen), 10, 10, win_width, win_height, 1,
      BlackPixel(dpy, screen), BlackPixel(dpy, screen));
  XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
  XMapWindow(dpy, win);

  GC gc = XCreateGC(dpy, win, 0, NULL);

  // to handle close window
  Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wm_delete, 1);

  // for full screen
  Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom wm_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

  // Create XImage with raw buffer
  char *data = malloc(win_width * win_height * 4);
  XImage *img =
      XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen),
                   ZPixmap, 0, data, win_width, win_height, 32, 0);


  long long last = now_ms();
  int running = 1;


  while (running) {
    // Handle events
    while (XPending(dpy)) {
      XEvent e;
      XNextEvent(dpy, &e);

      if (e.type == KeyPress) {
        KeySym key = XLookupKeysym(&e.xkey, 0);
        if (key == XK_q || key == XK_Escape) { // quit
          running = 0;
        }

        if (key == XK_c || key == XK_C) {
          filled_circle = !filled_circle;
        }

        if (key == XK_r || key == XK_R) {
          random_background = !random_background;
        }

        if (key == XK_f || key == XK_F) { // toggle fullscreen
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

      // detect resize+fullscreen
      if (e.type == ConfigureNotify) {
        // ignore event if size has not changed
        if (win_width == e.xconfigure.width && win_height == e.xconfigure.height) break;

        win_width = e.xconfigure.width;
        win_height = e.xconfigure.height;
        // printf("Window resized to width=%i height=%i\n", win_width, win_height);

        // Recreate XImage with new size
        if (img) {
          XDestroyImage(img);
        }
        img = XCreateImage(dpy, DefaultVisual(dpy, screen),
                           DefaultDepth(dpy, screen), ZPixmap, 0,
                           malloc(win_width * win_height * 4), win_width,
                           win_height, 32, 0);
      }

      // detect close
      if (e.type == ClientMessage) {
        if ((Atom)e.xclient.data.l[0] == wm_delete)
          running = 0;
      }
    }

    // compute time step and deltatiem
    long long t = now_ms();
    float dt = (t - last) / 1000.0f;
    last = t;

    // draw to image buffer using deltatime for physics
    draw(dt, img);

    // copy buffered image to screen
    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, win_width, win_height);

    // reduce CPU load with some sleep
    usleep(2000);
  }

  XDestroyImage(img); // free double buffer img
  XFreeGC(dpy, gc);   // free display and graphics context
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
  return 0;
}

