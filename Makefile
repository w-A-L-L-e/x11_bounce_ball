CC=clang
all: bounce tetris pixelgl_linux

bounce: bounce.c
	$(CC) -O3 bounce.c -o bounce -lX11


tetris: tetris.c
	$(CC) -O3 tetris.c -o tetris -lX11


pixelgl_linux: pixelgl_linux.c
	$(CC) pixelgl_linux.c -O3 -o pixelgl_linux -lX11 -lGL -lm

clean:
	@rm -vf bounce tetris pixelgl_linux
