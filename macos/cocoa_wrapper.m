#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>

extern int win_width, win_height;
extern uint32_t *framebuffer;
extern void drawFrame(float dt);

@interface MyView : NSView
@end

@implementation MyView
- (BOOL)acceptsFirstResponder { return YES; }
- (void)keyDown:(NSEvent *)event {
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) return;
    unichar key = [chars characterAtIndex:0];
    if (key == 'q' || key == 0x1B) { [NSApp terminate:nil]; }
    if (key == 'f') { [[self window] toggleFullScreen:nil]; }
}
- (void)resizeFramebuffer:(NSSize)size {
    win_width = size.width;
    win_height = size.height;
    if (framebuffer) free(framebuffer);
    framebuffer = malloc(win_width * win_height * 4);
}
- (void)drawRect:(NSRect)dirtyRect {
    if (!framebuffer) return;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(framebuffer, win_width, win_height,
                                             8, win_width * 4, cs,
                                             kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRef c = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(c, NSMakeRect(0,0,win_width,win_height), img);
    CGImageRelease(img);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
}
@end

static long long now_ms() {
    return (long long)([[NSDate date] timeIntervalSince1970] * 1000);
}
static CVReturn displayLinkCallback(CVDisplayLinkRef link,
                                    const CVTimeStamp* now,
                                    const CVTimeStamp* out,
                                    CVOptionFlags f1, CVOptionFlags* f2,
                                    void* ctx) {
    static long long last = 0;
    long long t = now_ms();
    if (!last) last = t;
    float dt = (t - last) / 1000.0f;
    last = t;
    drawFrame(dt);
    dispatch_async(dispatch_get_main_queue(), ^{ [(NSView*)CFBridgingRelease(ctx) setNeedsDisplay:YES]; CFRetain(ctx); });
    return kCVReturnSuccess;
}

int main() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSRect rect = NSMakeRect(100,100,win_width,win_height);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:rect
                    styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable)
                      backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Pixel Window"];
    [win makeKeyAndOrderFront:nil];
    MyView *view = [[MyView alloc] initWithFrame:rect];
    [win setContentView:view];
    [view resizeFramebuffer:rect.size];
    [NSApp activateIgnoringOtherApps:YES];
    CVDisplayLinkRef dl;
    CVDisplayLinkCreateWithActiveCGDisplays(&dl);
    CVDisplayLinkSetOutputCallback(dl, displayLinkCallback, (__bridge void*)view);
    CVDisplayLinkStart(dl);
    [NSApp run];
    CVDisplayLinkStop(dl);
    CVDisplayLinkRelease(dl);
    if (framebuffer) free(framebuffer);
    return 0;
}
