#pragma once

// Use the project's centralized OpenGL header which handles platform-specific includes correctly
#include "opengl.h"
#include <SDL.h>
#include <string>

// Initialize text rendering (load font)
bool text_init();

// Create a GL texture with rendered text
GLuint text_create_texture(const std::string& text, int* outW, int* outH, SDL_Color color);

// Draw texture at screen position (orthographic)
void text_draw_texture(GLuint tex, int x, int y, int w, int h);

// Free texture
void text_free_texture(GLuint tex);
