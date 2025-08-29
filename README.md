# X11 Bounce Ball Example

We only link with lX11. So no raylib or SDL used here. This also means it
only works in linux. But its tiny and renders the bouncing pretty smoothly.

There are some keyboard shortcuts to toggle features:

```
f : full screeen toggle
r : randomize background toggle
c : filled circle toggle
q : quit application

```

Also if you resize the application window it properly behaves and the ball stays in bounce and canvas
is auto resized etc.

Basically if you now want to write a game or something with minimal dependencies this would be a good starting point.

Also it has double buffering built in and performance of put_pixel is fast because it is inlined and writes to just memory.
The main loop handles drawing it to x11 window.
Further cleanup is definitely possible.
Mainly tested GPT5 and wanted a toy example. Still needed to bugfix and tweak things to get it all properly working but indeed
it took me a lot less time to make this than I would have spent without GPT.

## Installation

```
make
./bounce
```
