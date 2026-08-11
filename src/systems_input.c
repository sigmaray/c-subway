#include "metro.h"
#include <string.h>

static void start_run(GameState *state) {
  GameEvent ev;
  state->status.type = STATUS_RUNNING;
  memset(&ev, 0, sizeof(ev));
  ev.type = EVENT_GAME_STARTED;
  append_event(state, ev);
}

static void restart_run(GameState *state) {
  CreateGameStateOptions opts;
  GameState fresh;
  int bankedCoins;
  GameEvent ev;

  bankedCoins = state->status.type == STATUS_GAME_OVER
                    ? state->totalCoins
                    : state->totalCoins + state->coins;

  memset(&opts, 0, sizeof(opts));
  opts.highScore =
      state->highScore > state->score ? state->highScore : state->score;
  opts.totalCoins = bankedCoins;
  opts.muted = state->muted;
  opts.randomSeed = (state->randomSeed + 7919u);
  opts.useCustomSeed = true;
  opts.cheats = state->cheats;
  opts.useCustomCheats = true;

  create_game_state(&fresh, &opts);
  *state = fresh;
  start_run(state);
  memset(&ev, 0, sizeof(ev));
  ev.type = EVENT_GAME_RESTARTED;
  append_event(state, ev);
}

static void apply_command(GameState *state, GameCommand command) {
  GameEvent ev;

  switch (command.type) {
    case CMD_TOGGLE_MUTE:
      state->muted = !state->muted;
      return;
    case CMD_TOGGLE_CHEAT:
      state->cheats = toggle_cheat_flag(state->cheats, command.cheatId);
      return;
    case CMD_RESTART:
      if (state->status.type == STATUS_READY) {
        start_run(state);
        return;
      }
      if (state->status.type == STATUS_GAME_OVER ||
          state->status.type == STATUS_PAUSED) {
        restart_run(state);
        return;
      }
      return;
    case CMD_PAUSE:
      if (state->status.type == STATUS_RUNNING) {
        state->status.type = STATUS_PAUSED;
        return;
      }
      if (state->status.type == STATUS_PAUSED) {
        state->status.type = STATUS_RUNNING;
        return;
      }
      return;
    case CMD_MOVE_LEFT:
    case CMD_MOVE_RIGHT:
    case CMD_JUMP:
    case CMD_SLIDE:
    case CMD_STAND:
      break;
  }

  if (state->status.type != STATUS_RUNNING) {
    return;
  }
  if (state->player.movement.type == MOVE_STUNNED) {
    return;
  }

  switch (command.type) {
    case CMD_MOVE_LEFT: {
      Lane nextLane = shift_lane(state->player.lane, -1);
      GameEntity flat[MAX_FLATTEN_ENTITIES];
      int flatCount;
      if (nextLane == state->player.lane) {
        return;
      }
      flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
      if (train_blocks_lane_change(&state->player, nextLane, flat, flatCount)) {
        return;
      }
      state->player.lane = nextLane;
      memset(&ev, 0, sizeof(ev));
      ev.type = EVENT_LANE_CHANGED;
      ev.direction = -1;
      append_event(state, ev);
      return;
    }
    case CMD_MOVE_RIGHT: {
      Lane nextLane = shift_lane(state->player.lane, 1);
      GameEntity flat[MAX_FLATTEN_ENTITIES];
      int flatCount;
      if (nextLane == state->player.lane) {
        return;
      }
      flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
      if (train_blocks_lane_change(&state->player, nextLane, flat, flatCount)) {
        return;
      }
      state->player.lane = nextLane;
      memset(&ev, 0, sizeof(ev));
      ev.type = EVENT_LANE_CHANGED;
      ev.direction = 1;
      append_event(state, ev);
      return;
    }
    case CMD_JUMP: {
      if (!player_can_jump(&state->player)) {
        state->player.jumpBufferSeconds = GAME_CONFIG.inputBufferSeconds;
        return;
      }
      state->player.velocityY = GAME_CONFIG.jumpVelocity;
      state->player.movement.type = MOVE_JUMPING;
      state->player.movement.remainingSeconds = 0.0f;
      state->player.coyoteSeconds = 0.0f;
      state->player.jumpBufferSeconds = 0.0f;
      memset(&ev, 0, sizeof(ev));
      ev.type = EVENT_PLAYER_JUMPED;
      append_event(state, ev);
      return;
    }
    case CMD_SLIDE: {
      bool canSlideFromRun;
      bool canDiveSlide;
      if (has_effect(state->effects, state->effectCount, POWERUP_BOOST) ||
          state->cheats.fly) {
        return;
      }
      if (state->player.movement.type == MOVE_SLIDING) {
        return;
      }
      canSlideFromRun = state->player.movement.type == MOVE_RUNNING;
      canDiveSlide =
          (state->player.movement.type == MOVE_JUMPING ||
           state->player.movement.type == MOVE_FALLING) &&
          state->player.position.y <= GAME_CONFIG.trainRoofY + 0.5f;
      if (!canSlideFromRun && !canDiveSlide) {
        state->player.slideBufferSeconds = GAME_CONFIG.inputBufferSeconds;
        return;
      }
      state->player.velocityY = 0.0f;
      state->player.movement.type = MOVE_SLIDING;
      state->player.movement.remainingSeconds =
          GAME_CONFIG.slideDurationSeconds;
      state->player.slideBufferSeconds = 0.0f;
      memset(&ev, 0, sizeof(ev));
      ev.type = EVENT_PLAYER_SLID;
      append_event(state, ev);
      return;
    }
    case CMD_STAND: {
      if (state->player.movement.type != MOVE_SLIDING) {
        return;
      }
      state->player.movement.type = MOVE_RUNNING;
      state->player.movement.remainingSeconds = 0.0f;
      return;
    }
    default:
      return;
  }
}

void apply_input(GameState *state, const InputState *input) {
  int i;
  if (input == NULL) {
    return;
  }
  for (i = 0; i < input->count; i++) {
    apply_command(state, input->commands[i]);
  }
}
