// Android build: the game includes SDL as <SDL2/SDL.h>, while the SDL2
// build exposes the headers as plain <SDL.h>. This forwarder lives in a
// directory that contains no plain SDL.h, so it can never include itself.
#include <SDL.h>
