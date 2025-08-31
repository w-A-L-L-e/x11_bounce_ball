GXX=clang
all: bounce tetris

bounce: bounce.c
	$(GXX) -O3 bounce.c -o bounce -lX11


tetris: tetris.c
	$(GXX) -O3 tetris.c -o tetris -lX11


clean:
	@rm -vf bounce tetris
