echo "compiling cocoa mac version minimal screen drawing:"
clang -framework Cocoa -framework QuartzCore -o pixelwin pixelwin_mac.m

echo "C version with minimal cocoa wrapper to just handle screen and window"
clang main.c cocoa_wrapper.m -framework Cocoa -framework QuartzCore -o pixelwin
