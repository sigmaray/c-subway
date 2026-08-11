#include "metro.h"

void update_score(GameState *state, float dt) {
  float boostBonus;
  float distanceDelta;
  float gain;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  boostBonus = has_effect(state->effects, state->effectCount, POWERUP_BOOST)
                   ? GAME_CONFIG.boostSpeedBonus
                   : 0.0f;
  distanceDelta =
      state->player.forwardBlocked ? 0.0f : (state->speed + boostBonus) * dt;
  gain = distanceDelta * GAME_CONFIG.scorePerMeter *
         get_multiplier(state->effects, state->effectCount, &state->cheats);

  if (state->coinStreak > 0) {
    state->streakTimer -= dt;
    if (state->streakTimer <= 0.0f) {
      state->coinStreak = 0;
      state->streakTimer = 0.0f;
    }
  }

  state->score += gain;
  state->cameraShake =
      state->cameraShake - dt * 2.4f > 0.0f ? state->cameraShake - dt * 2.4f
                                            : 0.0f;
}

void update_difficulty(GameState *state, float dt) {
  float nextSpeed;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  if (state->cheats.maxSpeed) {
    state->speed = GAME_CONFIG.maximumSpeed;
    return;
  }

  nextSpeed = clamp_f(
      state->speed + GAME_CONFIG.speedAcceleration * dt,
      GAME_CONFIG.initialSpeed,
      GAME_CONFIG.maximumSpeed);
  state->speed = nextSpeed;
}
