#include "metro.h"

void update_game(GameState *state, const InputState *input, float dt) {
  float clampedDelta;

  if (state == NULL) {
    return;
  }

  clampedDelta = dt;
  if (clampedDelta > GAME_CONFIG.maxDeltaSeconds) {
    clampedDelta = GAME_CONFIG.maxDeltaSeconds;
  }

  clear_events(state);

  if (state->status.type == STATUS_GAME_OVER) {
    state->deathAnimSeconds += clampedDelta;
    state->hitStopSeconds -= clampedDelta;
    if (state->hitStopSeconds < 0.0f) {
      state->hitStopSeconds = 0.0f;
    }
    apply_input(state, input);
    return;
  }

  if (state->hitStopSeconds > 0.0f) {
    state->hitStopSeconds -= clampedDelta;
    if (state->hitStopSeconds < 0.0f) {
      state->hitStopSeconds = 0.0f;
    }
    apply_input(state, input);
    return;
  }

  apply_input(state, input);
  update_player(state, clampedDelta);
  update_world(state, clampedDelta);
  update_entities(state, clampedDelta);
  attract_coins(state, clampedDelta);
  collect_items(state);
  detect_collisions(state);
  detect_near_misses(state);
  update_effects(state, clampedDelta);
  remove_expired_entities(state);
  spawn_world_objects(state);
  update_score(state, clampedDelta);
  update_difficulty(state, clampedDelta);
}
