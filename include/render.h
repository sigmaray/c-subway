#ifndef RENDER_H
#define RENDER_H

#include "metro.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_init(void);
void render_frame(Framebuffer *fb, GameState *state, float dt);
void render_toggle_cheat_overlay(void);

#ifdef __cplusplus
}
#endif

#endif /* RENDER_H */
