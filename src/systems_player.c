#include "metro.h"
#include <math.h>
#include <string.h>

bool is_above_train_roof(float playerY) {
  return playerY >= GAME_CONFIG.trainRoofY - 0.45f;
}

static float train_front_z(const GameEntity *train) {
  return train->positionZ - train->length;
}

static float train_back_z(const GameEntity *train) {
  return train->positionZ;
}

bool player_overlaps_train_footprint(const PlayerState *player,
                                     const GameEntity *train,
                                     float lateralSlack) {
  float trainX = (float)train->lane * GAME_CONFIG.laneWidth;
  float z;
  if (fabs(player->position.x - trainX) >
      GAME_CONFIG.laneWidth * lateralSlack) {
    return false;
  }
  z = player->position.z;
  return z <= train_back_z(train) + 0.45f &&
         z >= train_front_z(train) - 0.45f;
}

const GameEntity *find_ride_train(const PlayerState *player,
                                  const GameEntity *entities,
                                  int count) {
  const GameEntity *best = NULL;
  int i;
  for (i = 0; i < count; i++) {
    if (entities[i].kind != ENTITY_TRAIN) {
      continue;
    }
    if (!player_overlaps_train_footprint(player, &entities[i], 0.62f)) {
      continue;
    }
    if (player->position.y <
        GAME_CONFIG.trainRoofY - GAME_CONFIG.trainMountAssistY) {
      continue;
    }
    best = &entities[i];
  }
  return best;
}

float find_train_support_y(const PlayerState *player,
                           const GameEntity *entities,
                           int count) {
  const GameEntity *ride = find_ride_train(player, entities, count);
  return ride == NULL ? GAME_CONFIG.groundY : GAME_CONFIG.trainRoofY;
}

bool can_land_on_train(const PlayerState *player,
                       const GameEntity *train,
                       float nextY,
                       float velocityY) {
  float roof;
  if (velocityY > 0.5f) {
    return false;
  }
  if (!player_overlaps_train_footprint(player, train, 0.62f)) {
    return false;
  }
  roof = GAME_CONFIG.trainRoofY;
  return player->position.y >= roof - GAME_CONFIG.trainMountAssistY ||
         (nextY <= roof + 0.2f && player->position.y >= roof - 1.15f);
}

void apply_train_ride_motion(PlayerState *player,
                             const GameEntity *train,
                             float dt) {
  float trainX;
  float assistX;
  if (train == NULL ||
      player->position.y < GAME_CONFIG.trainRoofY - 0.35f) {
    player->hasRideTrain = false;
    player->rideTrainId = 0;
    return;
  }

  trainX = (float)train->lane * GAME_CONFIG.laneWidth;
  assistX = player->position.x +
            (trainX - player->position.x) * clamp_f(8.0f * dt, 0.0f, 1.0f);

  player->hasRideTrain = true;
  player->rideTrainId = train->id;
  player->position.x = assistX;
  player->position.z = player->position.z + train->speed * dt;
}

static float target_lane_x(Lane lane) {
  return (float)lane * GAME_CONFIG.laneWidth;
}

static void update_horizontal(PlayerState *player, float dt) {
  float targetX;
  float t;
  if (player->movement.type == MOVE_STUNNED) {
    return;
  }
  targetX = target_lane_x(player->lane);
  t = 1.0f - (float)exp((double)(-GAME_CONFIG.laneChangeSpeed * dt));
  player->position.x =
      player->position.x + (targetX - player->position.x) * t;
}

static void tick_timers(PlayerState *player, float dt) {
  player->coyoteSeconds = clamp_f(player->coyoteSeconds - dt, 0.0f, 1e9f);
  player->jumpBufferSeconds =
      clamp_f(player->jumpBufferSeconds - dt, 0.0f, 1e9f);
  player->slideBufferSeconds =
      clamp_f(player->slideBufferSeconds - dt, 0.0f, 1e9f);
  player->boardRecoverySeconds =
      clamp_f(player->boardRecoverySeconds - dt, 0.0f, 1e9f);
}

static bool try_buffered_jump(PlayerState *player) {
  if (player->jumpBufferSeconds <= 0.0f || !player_can_jump(player)) {
    return false;
  }
  player->velocityY = GAME_CONFIG.jumpVelocity;
  player->movement.type = MOVE_JUMPING;
  player->movement.remainingSeconds = 0.0f;
  player->coyoteSeconds = 0.0f;
  player->jumpBufferSeconds = 0.0f;
  return true;
}

static bool try_buffered_slide(PlayerState *player, float supportY) {
  bool canSlide;
  if (player->slideBufferSeconds <= 0.0f) {
    return false;
  }
  canSlide =
      player->movement.type == MOVE_RUNNING ||
      ((player->movement.type == MOVE_JUMPING ||
        player->movement.type == MOVE_FALLING) &&
       player->position.y <= supportY + 1.4f);
  if (!canSlide) {
    return false;
  }
  player->velocityY = 0.0f;
  player->position.y = supportY;
  player->movement.type = MOVE_SLIDING;
  player->movement.remainingSeconds = GAME_CONFIG.slideDurationSeconds;
  player->slideBufferSeconds = 0.0f;
  return true;
}

static void update_boost_hover(PlayerState *player, float dt) {
  float targetY = GAME_CONFIG.boostHoverHeight;
  float heightError = targetY - player->position.y;
  float lift;
  if (heightError > 0.0f) {
    lift = GAME_CONFIG.boostLiftSpeed * dt;
    if (lift > heightError) {
      lift = heightError;
    }
  } else {
    lift = -GAME_CONFIG.boostLiftSpeed * 0.55f * dt;
    if (lift < heightError) {
      lift = heightError;
    }
  }
  player->position.y = player->position.y + lift;
  player->velocityY =
      heightError > 0.15f ? GAME_CONFIG.boostLiftSpeed * 0.35f : 0.0f;
  player->movement.type = MOVE_JUMPING;
  player->movement.remainingSeconds = 0.0f;
  player->coyoteSeconds = 0.0f;
  player->hasRideTrain = false;
  player->rideTrainId = 0;
}

static void update_vertical(PlayerState *player,
                            float dt,
                            float supportY,
                            bool canMountTrain) {
  float gravityScale;
  float nextVelocityY;
  float nextY;

  if (player->movement.type == MOVE_STUNNED) {
    float remaining = player->movement.remainingSeconds - dt;
    if (remaining <= 0.0f) {
      player->movement.type = MOVE_RUNNING;
      player->movement.remainingSeconds = 0.0f;
      player->position.y = supportY;
      player->velocityY = 0.0f;
      return;
    }
    player->movement.remainingSeconds = remaining;
    player->position.y = supportY;
    player->velocityY = 0.0f;
    return;
  }

  if (player->movement.type == MOVE_SLIDING) {
    float remaining = player->movement.remainingSeconds - dt;
    if (player->position.y > supportY + 0.25f) {
      player->movement.type = MOVE_FALLING;
      player->movement.remainingSeconds = 0.0f;
      if (player->velocityY > 0.0f) {
        player->velocityY = 0.0f;
      }
      player->coyoteSeconds = GAME_CONFIG.coyoteSeconds;
      return;
    }
    if (remaining <= 0.0f) {
      player->movement.type = MOVE_RUNNING;
      player->movement.remainingSeconds = 0.0f;
      player->position.y = supportY;
      player->velocityY = 0.0f;
      return;
    }
    player->movement.remainingSeconds = remaining;
    player->position.y = supportY;
    player->velocityY = 0.0f;
    return;
  }

  if (player->movement.type == MOVE_RUNNING) {
    if (player->position.y > supportY + 0.25f) {
      player->movement.type = MOVE_FALLING;
      player->velocityY = 0.0f;
      player->coyoteSeconds = GAME_CONFIG.coyoteSeconds;
      return;
    }
    player->position.y = supportY;
    player->velocityY = 0.0f;
    player->coyoteSeconds = GAME_CONFIG.coyoteSeconds;
    return;
  }

  gravityScale =
      (fabs(player->velocityY) < 2.5f && player->velocityY > 0.0f) ? 0.72f
                                                                   : 1.0f;
  nextVelocityY =
      player->velocityY - GAME_CONFIG.gravity * gravityScale * dt;
  nextY = player->position.y + nextVelocityY * dt;

  if (canMountTrain && nextY <= GAME_CONFIG.trainRoofY &&
      nextVelocityY <= 0.0f) {
    player->position.y = GAME_CONFIG.trainRoofY;
    player->velocityY = 0.0f;
    player->movement.type = MOVE_RUNNING;
    player->coyoteSeconds = GAME_CONFIG.coyoteSeconds;
    return;
  }

  if (nextY <= supportY && nextVelocityY <= 0.0f) {
    player->position.y = supportY;
    player->velocityY = 0.0f;
    player->movement.type = MOVE_RUNNING;
    player->coyoteSeconds = GAME_CONFIG.coyoteSeconds;
    return;
  }

  player->position.y = nextY > supportY ? nextY : supportY;
  player->velocityY = nextVelocityY;
  player->movement.type =
      nextVelocityY > 0.0f ? MOVE_JUMPING : MOVE_FALLING;
  player->movement.remainingSeconds = 0.0f;
}

void update_player(GameState *state, float dt) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int flatCount;
  PlayerState player;
  MovementType originalMovement;
  bool boosting;
  bool wasAirborne;
  bool landingTrain;
  float supportY;
  const GameEntity *ride;
  int i;
  GameEvent ev;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
  boosting = has_effect(state->effects, state->effectCount, POWERUP_BOOST) ||
             state->cheats.fly;
  originalMovement = state->player.movement.type;
  player = state->player;

  tick_timers(&player, dt);
  update_horizontal(&player, dt);

  if (boosting && player.movement.type != MOVE_STUNNED) {
    update_boost_hover(&player, dt);
    state->player = player;
    return;
  }

  ride = find_ride_train(&player, flat, flatCount);
  apply_train_ride_motion(&player, ride, dt);

  supportY = find_train_support_y(&player, flat, flatCount);
  landingTrain = false;
  for (i = 0; i < flatCount; i++) {
    if (flat[i].kind == ENTITY_TRAIN &&
        can_land_on_train(&player, &flat[i],
                          player.position.y + player.velocityY * dt,
                          player.velocityY)) {
      landingTrain = true;
      break;
    }
  }

  wasAirborne = !player_is_grounded(&player);
  update_vertical(&player, dt, supportY, landingTrain);

  if (try_buffered_jump(&player)) {
    state->player = player;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_PLAYER_JUMPED;
    append_event(state, ev);
    return;
  }

  if (try_buffered_slide(&player, supportY)) {
    state->player = player;
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_PLAYER_SLID;
    append_event(state, ev);
    return;
  }

  state->player = player;
  if (wasAirborne &&
      (originalMovement == MOVE_JUMPING || originalMovement == MOVE_FALLING) &&
      player.movement.type == MOVE_RUNNING) {
    memset(&ev, 0, sizeof(ev));
    ev.type = EVENT_PLAYER_LANDED;
    append_event(state, ev);
  }
}
