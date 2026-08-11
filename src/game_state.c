#include "metro.h"
#include <string.h>

static uint32_t g_next_entity = 1;
static uint32_t g_next_segment = 1;

const CheatFlags DEFAULT_CHEATS = {
  false, false, false, false, false
};

void reset_identifiers(void) {
  g_next_entity = 1;
  g_next_segment = 1;
}

uint32_t create_entity_id(void) {
  uint32_t id = g_next_entity;
  g_next_entity += 1;
  return id;
}

uint32_t create_segment_id(void) {
  uint32_t id = g_next_segment;
  g_next_segment += 1;
  return id;
}

Lane clamp_lane(int lane) {
  if (lane <= -1) {
    return (Lane)-1;
  }
  if (lane >= 1) {
    return (Lane)1;
  }
  return (Lane)0;
}

Lane shift_lane(Lane lane, int delta) {
  return clamp_lane((int)lane + delta);
}

PlayerState create_player_state(void) {
  PlayerState p;
  memset(&p, 0, sizeof(p));
  p.lane = 0;
  p.position = vec3(0.0f, 0.0f, 0.0f);
  p.velocityY = 0.0f;
  p.movement.type = MOVE_RUNNING;
  p.movement.remainingSeconds = 0.0f;
  p.coyoteSeconds = 0.0f;
  p.jumpBufferSeconds = 0.0f;
  p.slideBufferSeconds = 0.0f;
  p.boardRecoverySeconds = 0.0f;
  p.rideTrainId = 0;
  p.hasRideTrain = false;
  p.forwardBlocked = false;
  return p;
}

bool player_is_grounded(const PlayerState *player) {
  if (player->movement.type == MOVE_RUNNING ||
      player->movement.type == MOVE_SLIDING) {
    return true;
  }
  if (player->movement.type == MOVE_STUNNED &&
      player->position.y <= GAME_CONFIG.groundY + 0.05f) {
    return true;
  }
  return false;
}

bool player_can_jump(const PlayerState *player) {
  return player_is_grounded(player) || player->coyoteSeconds > 0.0f;
}

bool player_is_sliding(const PlayerState *player) {
  return player->movement.type == MOVE_SLIDING;
}

bool player_is_board_recovering(const PlayerState *player) {
  return player->boardRecoverySeconds > 0.0f;
}

bool has_effect(const ActiveEffect *effects, int count, PowerUpType type) {
  int i;
  for (i = 0; i < count; i++) {
    if (effects[i].type == type) {
      return true;
    }
  }
  return false;
}

float get_multiplier(const ActiveEffect *effects, int count, const CheatFlags *cheats) {
  (void)cheats;
  if (has_effect(effects, count, POWERUP_MULTIPLIER)) {
    return 2.0f;
  }
  return 1.0f;
}

bool is_magnet_active(const ActiveEffect *effects, int count, const CheatFlags *cheats) {
  (void)cheats;
  return has_effect(effects, count, POWERUP_MAGNET);
}

CheatFlags toggle_cheat_flag(CheatFlags cheats, CheatId id) {
  switch (id) {
    case CHEAT_IMMORTAL:
      cheats.immortal = !cheats.immortal;
      break;
    case CHEAT_MAX_SPEED:
      cheats.maxSpeed = !cheats.maxSpeed;
      break;
    case CHEAT_FLY:
      cheats.fly = !cheats.fly;
      break;
    case CHEAT_NO_MAGNETS:
      cheats.noMagnets = !cheats.noMagnets;
      break;
    case CHEAT_NO_BOOST:
      cheats.noBoost = !cheats.noBoost;
      break;
  }
  return cheats;
}

bool is_cheat_active(CheatFlags cheats) {
  return cheats.immortal || cheats.maxSpeed || cheats.fly ||
         cheats.noMagnets || cheats.noBoost;
}

void create_empty_world(WorldState *out) {
  memset(out, 0, sizeof(*out));
  out->segmentCount = 0;
  out->nextSegmentZ = -20.0f;
  out->distanceTraveled = 0.0f;
}

void create_game_state(GameState *out, const CreateGameStateOptions *options) {
  CreateGameStateOptions defaults;
  memset(&defaults, 0, sizeof(defaults));
  defaults.randomSeed = 0xc0ffeeu;
  defaults.cheats = DEFAULT_CHEATS;

  if (options == NULL) {
    options = &defaults;
  }

  reset_identifiers();
  memset(out, 0, sizeof(*out));

  out->status.type = STATUS_READY;
  out->elapsedSeconds = 0.0f;
  out->distance = 0.0f;
  out->score = 0.0f;
  out->coins = 0;
  out->coinStreak = 0;
  out->streakTimer = 0.0f;
  out->speed = GAME_CONFIG.initialSpeed;
  out->highScore = options->highScore;
  out->totalCoins = options->totalCoins;
  out->boardCharges = GAME_CONFIG.boardChargesPerRun;
  out->hitStopSeconds = 0.0f;
  out->deathAnimSeconds = 0.0f;
  out->cameraShake = 0.0f;
  out->nearMissCount = 0;
  out->player = create_player_state();
  create_empty_world(&out->world);
  out->entityCount = 0;
  out->effectCount = 0;
  out->eventCount = 0;
  out->randomSeed = options->useCustomSeed ? options->randomSeed : 0xc0ffeeu;
  out->muted = options->muted;
  out->cheats = options->useCustomCheats ? options->cheats : DEFAULT_CHEATS;
}

void clear_events(GameState *state) {
  state->eventCount = 0;
}

bool append_event(GameState *state, GameEvent event) {
  if (state->eventCount >= MAX_EVENTS) {
    return false;
  }
  state->events[state->eventCount++] = event;
  return true;
}

InputState empty_input(void) {
  InputState input;
  memset(&input, 0, sizeof(input));
  return input;
}
