
// cocoa_gl_wrapper.m
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>
#import <OpenGL/gl3.h>      // core profile 3.2+
#import <OpenGL/OpenGL.h>

extern int win_width, win_height;
extern uint32_t *framebuffer;
extern void drawFrame(float dt);

static long long now_ms_objc() {
    return (long long)([NSDate date].timeIntervalSince1970 * 1000.0);
}

@interface GLView : NSOpenGLView
@end

@implementation GLView {
    CVDisplayLinkRef displayLink;

    GLuint prog, vao, vbo, tex;
    GLint uTexLoc;
    CGSize pixelSize;          // drawable size in pixels (accounts for retina)
    BOOL needsTexRealloc;
}

// Simple passthrough shaders (core profile)
static const char *vs_src =
"#version 150\n"
"in vec2 aPos; in vec2 aUV; out vec2 vUV;"
"void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0);}";

static const char *fs_src =
"#version 150\n"
"in vec2 vUV; out vec4 frag;"
"uniform sampler2D uTex;"
"void main(){ frag = texture(uTex, vUV); }";

+ (NSOpenGLPixelFormat *)defaultPixelFormat {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 0,
        0
    };
    return [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect pixelFormat:[GLView defaultPixelFormat]];
    if (self) {
        [self setWantsBestResolutionOpenGLSurface:YES]; // retina
    }
    return self;
}

- (void)prepareOpenGL {
    [super prepareOpenGL];

    // VSync via swap interval (also driven by CVDisplayLink)
    GLint one = 1;
    [[self openGLContext] setValues:&one forParameter:NSOpenGLCPSwapInterval];

    // Compile shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aUV");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    uTexLoc = glGetUniformLocation(prog, "uTex");

    // Fullscreen triangle strip
    const float quad[] = {
        //  x,   y,   u,  v
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
        -1.f,  1.f, 0.f, 1.f,
         1.f,  1.f, 1.f, 1.f,
    };
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    // Texture for CPU framebuffer
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // initial sizes
    [self updateDrawableSizeAndFramebuffer:YES];

    // CVDisplayLink -> render loop
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink,
        [](CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*,
           CVOptionFlags, CVOptionFlags*, void *ctx)->CVReturn {
            GLView *v = (__bridge GLView*)ctx;
            static long long last = 0;
            long long t = now_ms_objc();
            if (!last) last = t;
            float dt = (float)(t - last) / 1000.0f;
            last = t;

            // Drive user draw + request display on main thread
            dispatch_async(dispatch_get_main_queue(), ^{
                [v tick:dt];
            });
            return kCVReturnSuccess;
        }, (__bridge void *)self);
    CVDisplayLinkStart(displayLink);
}

- (void)dealloc {
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    if (tex) glDeleteTextures(1, &tex);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)e {
    NSString *chars = e.charactersIgnoringModifiers;
    if (chars.length == 0) return;
    unichar k = [chars characterAtIndex:0];
    if (k == 'q' || k == 0x1B) { [NSApp terminate:nil]; return; } // Esc
    if (k == 'f') { [[self window] toggleFullScreen:nil]; return; }
}

- (void)reshape {
    [super reshape];
    [self updateDrawableSizeAndFramebuffer:NO];
}

- (void)updateDrawableSizeAndFramebuffer:(BOOL)force {
    // Retina-aware pixel size
    NSWindow *w = self.window;
    CGFloat scale = w.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor ?: 1.0;
    NSRect b = self.bounds;
    CGSize newPixels = CGSizeMake((size_t)(b.size.width * scale), (size_t)(b.size.height * scale));

    if (force || newPixels.width != pixelSize.width || newPixels.height != pixelSize.height) {
        pixelSize = newPixels;

        // Update GL viewport
        [[self openGLContext] makeCurrentContext];
        glViewport(0, 0, (GLsizei)pixelSize.width, (GLsizei)pixelSize.height);

        // Reallocate CPU framebuffer to logical (point) size, not pixel size:
        // We’ll keep 1:1 pixels for sharpness on retina by using pixelSize.
        win_width  = (int)pixelSize.width;
        win_height = (int)pixelSize.height;

        if (framebuffer) free(framebuffer);
        framebuffer = (uint32_t*)malloc((size_t)win_width * (size_t)win_height * 4);

        // (Re)allocate GL texture storage
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     win_width, win_height, 0,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        needsTexRealloc = NO;
    }
}

- (void)tick:(float)dt {
    // Step user code
    drawFrame(dt);

    // Upload framebuffer and draw
    [self setNeedsDisplay:YES];
    [self display]; // call drawRect now
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[self openGLContext] makeCurrentContext];

    if (!framebuffer) return;

    glBindTexture(GL_TEXTURE_2D, tex);
    // Update texture with CPU buffer (BGRA matches our ARGB little-endian)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, win_width, win_height,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, framebuffer);

    glUseProgram(prog);
    glBindVertexArray(vao);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    [[self openGLContext] flushBuffer];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect rect = NSMakeRect(100, 100, 800, 600);
        NSWindow *win = [[NSWindow alloc] initWithContentRect:rect
            styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable)
              backing:NSBackingStoreBuffered defer:NO];
        win.title = @"OpenGL Pixel Window";
        [win makeKeyAndOrderFront:nil];

        GLView *view = [[GLView alloc] initWithFrame:rect];
        win.contentView = view;

        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];

        if (framebuffer) free(framebuffer);
    }
    return 0;
}
