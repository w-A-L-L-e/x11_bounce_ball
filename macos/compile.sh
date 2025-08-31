echo "compiling cocoa mac version minimal screen drawing:"
clang -framework Cocoa -framework QuartzCore -o pixelwin pixelwin_mac.m

echo "compile C version with minimal cocoa wrapper to just handle screen and window"
clang main.c cocoa_wrapper.m -framework Cocoa -framework QuartzCore -o pixelwin

echo "compile C version with cocoa wrapper that uses OpenGL for better GPU performance"
clang main2.c cocoa_gl_wrapper.m -framework Cocoa -framework QuartzCore -framework OpenGL -o pixelwin_gl
./pixelwin_gl
