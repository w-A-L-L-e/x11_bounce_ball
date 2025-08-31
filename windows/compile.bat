# compile simple windows version
x86_64-w64-mingw32-gcc -mwindows -s -o pixelwin.exe pixelwin.c -lgdi32


# compile windows version that uses opengl
x86_64-w64-mingw32-gcc pixelwin_gl.c -lopengl32 -lgdi32 -o pixelwin_gl.exe


# compile windows version with opengl3 code that uses shaders:

x86_64-w64-mingw32-gcc pixelwin_gl_modern.c -lopengl32 -lgdi32 -o pixelwin_gl_modern.exe
