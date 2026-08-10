#include "metro.h"
#include "platform.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_WIDTH 640
#define FB_HEIGHT 480

static SaveData save_from_state(const GameState *state) {
  SaveData s;
  s.highScore = state->highScore;
  s.totalCoins = state->totalCoins;
  s.muted = state->muted;
  s.cheats = state->cheats;
  return s;
}

static int save_changed(const SaveData *a, const SaveData *b) {
  return a->highScore != b->highScore || a->totalCoins != b->totalCoins ||
         a->muted != b->muted || a->cheats.immortal != b->cheats.immortal ||
         a->cheats.infiniteMagnet != b->cheats.infiniteMagnet ||
         a->cheats.infiniteMultiplier != b->cheats.infiniteMultiplier ||
         a->cheats.lockMaxSpeed != b->cheats.lockMaxSpeed;
}

static void persist_if_needed(const GameState *state, SaveData *last,
                              int force) {
  SaveData snap = save_from_state(state);
  if (force || save_changed(&snap, last)) {
    storage_save(&snap);
    *last = snap;
  }
}

int main(void) {
  Platform *platform = NULL;
  Framebuffer fb;
  GameState state;
  CreateGameStateOptions opts;
  SaveData save;
  SaveData lastPersisted;
  InputState empty;
  double lastTime;
  GameStatusType prevStatus;

  memset(&fb, 0, sizeof(fb));
  fb.width = FB_WIDTH;
  fb.height = FB_HEIGHT;
  fb.pixels = (uint32_t *)calloc((size_t)FB_WIDTH * (size_t)FB_HEIGHT,
                                 sizeof(uint32_t));
  if (fb.pixels == NULL) {
    fprintf(stderr, "Failed to allocate framebuffer\n");
    return 1;
  }

  if (!platform_init(&platform, FB_WIDTH, FB_HEIGHT, "Metro Rush")) {
    fprintf(stderr, "Failed to init platform\n");
    free(fb.pixels);
    return 1;
  }

  storage_load(&save);
  memset(&opts, 0, sizeof(opts));
  opts.highScore = save.highScore;
  opts.totalCoins = save.totalCoins;
  opts.muted = save.muted;
  opts.cheats = save.cheats;
  opts.useCustomCheats = true;

  create_game_state(&state, &opts);
  empty = empty_input();
  update_game(&state, &empty, 0.0f); /* spawn initial world */

  render_init();
  lastPersisted = save_from_state(&state);
  lastTime = platform_time_seconds();
  prevStatus = state.status.type;

  while (1) {
    InputState input;
    double now;
    float dt;
    int alive;

    alive = platform_poll(platform, &input);
    if (!alive) {
      break;
    }

    now = platform_time_seconds();
    dt = (float)(now - lastTime);
    lastTime = now;
    if (dt < 0.0f) {
      dt = 0.0f;
    }
    if (dt > 0.05f) {
      dt = 0.05f;
    }

    update_game(&state, &input, dt);
    render_frame(&fb, &state, dt);
    platform_present(platform, &fb);

    if (state.status.type == STATUS_GAME_OVER &&
        prevStatus != STATUS_GAME_OVER) {
      persist_if_needed(&state, &lastPersisted, 1);
    } else {
      persist_if_needed(&state, &lastPersisted, 0);
    }
    prevStatus = state.status.type;

    /* Cap around ~60 FPS when vsync unavailable. */
    platform_sleep_ms(1);
  }

  persist_if_needed(&state, &lastPersisted, 1);
  platform_shutdown(platform);
  free(fb.pixels);
  return 0;
}
