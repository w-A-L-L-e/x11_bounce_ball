GXX=clang
all: bounce

bounce: bounce.c
	$(GXX) -O3 bounce.c -o bounce -lX11

clean:
	@rm -vf bounce
