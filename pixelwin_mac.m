// gpt converted code (not tested, used linux example as source):
//
// clang -framework Cocoa -framework QuartzCore -o pixelwin pixelwin_mac.m
// ./pixelwin
//
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>

static int win_width = 800;
static int win_height = 600;
static uint32_t *framebuffer = NULL;
static BOOL fullscreen = NO;

@interface MyView : NSView
@end

@implementation MyView

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)event {
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) return;
    unichar key = [chars characterAtIndex:0];

    if (key == 'q' || key == 0x1B) { // Esc
        [NSApp terminate:nil];
    } else if (key == 'f') {
        NSWindow *win = [self window];
        if (!fullscreen) {
            [win toggleFullScreen:nil];
            fullscreen = YES;
        } else {
            [win toggleFullScreen:nil];
            fullscreen = NO;
        }
    }
}

- (void)resizeFramebuffer:(NSSize)size {
    win_width = (int)size.width;
    win_height = (int)size.height;
    if (framebuffer) free(framebuffer);
    framebuffer = malloc(win_width * win_height * 4);
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    if (!framebuffer) return;

    // Create CGImage from raw buffer
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(framebuffer, win_width, win_height,
                                             8, win_width * 4, cs,
                                             kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);

    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRef c = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(c, NSMakeRect(0, 0, win_width, win_height), img);

    CGImageRelease(img);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
}

@end

// Timing helpers
static long long now_ms() {
    return (long long)([[NSDate date] timeIntervalSince1970] * 1000);
}

static void drawFrame(float dt) {
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

// CADisplayLink callback
static CVReturn displayLinkCallback(CVDisplayLinkRef link,
                                    const CVTimeStamp* now,
                                    const CVTimeStamp* outputTime,
                                    CVOptionFlags flagsIn,
                                    CVOptionFlags* flagsOut,
                                    void* ctx)
{
    static long long last = 0;
    long long t = now_ms();
    if (!last) last = t;
    float dt = (t - last) / 1000.0f;
    last = t;

    drawFrame(dt);

    dispatch_async(dispatch_get_main_queue(), ^{
        [(NSView*)CFBridgingRelease(ctx) setNeedsDisplay:YES];
        CFRetain(ctx); // balance the release
    });

    return kCVReturnSuccess;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect rect = NSMakeRect(100, 100, win_width, win_height);
        NSWindow *win = [[NSWindow alloc] initWithContentRect:rect
                                                    styleMask:(NSWindowStyleMaskTitled |
                                                               NSWindowStyleMaskClosable |
                                                               NSWindowStyleMaskResizable)
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
        [win setTitle:@"Pixel Window"];
        [win makeKeyAndOrderFront:nil];

        MyView *view = [[MyView alloc] initWithFrame:rect];
        [win setContentView:view];
        [view resizeFramebuffer:rect.size];

        [NSApp activateIgnoringOtherApps:YES];

        // Setup display link for render loop
        CVDisplayLinkRef displayLink;
        CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
        CVDisplayLinkSetOutputCallback(displayLink, displayLinkCallback, (__bridge void*)view);
        CVDisplayLinkStart(displayLink);

        [NSApp run];

        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);

        if (framebuffer) free(framebuffer);
    }
    return 0;
}
