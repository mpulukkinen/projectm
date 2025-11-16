// Minimal no-op implementation: we intentionally do NOT depend on SDL_ttf.
// This keeps the SDL UI build simple — labels are not rendered but buttons
// remain clickable. Implementations are safe no-ops.

// Use the project's centralized OpenGL header which handles platform-specific includes correctly
#include "opengl.h"
#include <SDL.h>
#include <string>

bool text_init() {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "text_render: SDL_ttf disabled; labels disabled");
    return false;
}

GLuint text_create_texture(const std::string& /*text*/, int* outW, int* outH, SDL_Color /*color*/) {
    if (outW) *outW = 0;
    if (outH) *outH = 0;
    return 0;
}

void text_draw_texture(GLuint /*tex*/, int /*x*/, int /*y*/, int /*w*/, int /*h*/) {
    // no-op
}

void text_free_texture(GLuint /*tex*/) {
    // no-op
}
