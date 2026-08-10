#include "metro.h"
#include <math.h>
#include <string.h>

#define MAX_IMMORTAL_RESOLVE_PUSH 3.0f
#define CONTACT_SLOP             0.02f

Bounds get_player_bounds(const PlayerState *player) {
  float height = player_is_sliding(player) ? GAME_CONFIG.playerSlideHeight
                                           : GAME_CONFIG.playerHeight;
  float centerY = player->position.y + height * 0.5f;
  return create_bounds(
      vec3(player->position.x, centerY, player->position.z),
      GAME_CONFIG.playerWidth,
      height,
      GAME_CONFIG.playerDepth);
}

bool get_entity_bounds(const GameEntity *entity, Bounds *out) {
  switch (entity->kind) {
    case ENTITY_COIN:
      if (entity->collected) {
        return false;
      }
      *out = create_bounds(
          vec3(entity->positionX, entity->height, entity->positionZ),
          0.42f, 0.42f, 0.42f);
      return true;
    case ENTITY_POWER_UP:
      if (entity->collected) {
        return false;
      }
      *out = create_bounds(
          vec3((float)entity->lane * GAME_CONFIG.laneWidth,
               entity->height,
               entity->positionZ),
          0.8f, 0.8f, 0.8f);
      return true;
    case ENTITY_OBSTACLE:
      *out = create_bounds(
          vec3((float)entity->lane * GAME_CONFIG.laneWidth,
               entity->baseY + entity->height * 0.5f,
               entity->positionZ),
          entity->width,
          entity->height,
          entity->depth);
      return true;
    case ENTITY_TRAIN:
      *out = create_bounds(
          vec3((float)entity->lane * GAME_CONFIG.laneWidth,
               GAME_CONFIG.trainBodyHeight * 0.32f,
               entity->positionZ - entity->length * 0.5f),
          GAME_CONFIG.laneWidth * 0.9f,
          GAME_CONFIG.trainBodyHeight * 0.64f,
          entity->length);
      return true;
    case ENTITY_DECORATION:
      return false;
  }
  return false;
}

bool collision_reason_for_entity(const GameEntity *entity,
                                 CollisionReason *out) {
  switch (entity->kind) {
    case ENTITY_OBSTACLE:
      if (entity->variant == OBSTACLE_BARRIER) {
        *out = COLLISION_BARRIER;
      } else if (entity->variant == OBSTACLE_LOW_BARRIER) {
        *out = COLLISION_LOW_BARRIER;
      } else if (entity->variant == OBSTACLE_OVERHEAD) {
        *out = COLLISION_OVERHEAD;
      } else {
        *out = COLLISION_OBSTACLE;
      }
      return true;
    case ENTITY_TRAIN:
      *out = COLLISION_TRAIN;
      return true;
    case ENTITY_COIN:
    case ENTITY_POWER_UP:
    case ENTITY_DECORATION:
      return false;
  }
  return false;
}

bool can_avoid_by_jump(const GameEntity *entity) {
  return entity->kind == ENTITY_OBSTACLE &&
         entity->variant == OBSTACLE_LOW_BARRIER;
}

bool can_avoid_by_slide(const GameEntity *entity) {
  return entity->kind == ENTITY_OBSTACLE &&
         entity->variant == OBSTACLE_OVERHEAD;
}

bool entities_collide(const PlayerState *player, const GameEntity *entity) {
  CollisionReason reason;
  Bounds playerBounds;
  Bounds entityBounds;

  if (!collision_reason_for_entity(entity, &reason)) {
    return false;
  }
  if (can_avoid_by_jump(entity) &&
      player->position.y > GAME_CONFIG.jumpClearanceY) {
    return false;
  }
  if (can_avoid_by_slide(entity) && player_is_sliding(player)) {
    return false;
  }
  if (entity->kind == ENTITY_TRAIN) {
    if (is_above_train_roof(player->position.y)) {
      return false;
    }
    if ((player->movement.type == MOVE_JUMPING ||
         player->movement.type == MOVE_FALLING) &&
        player->position.y > GAME_CONFIG.trainBodyHeight * 0.45f) {
      return false;
    }
  }

  playerBounds = get_player_bounds(player);
  if (!get_entity_bounds(entity, &entityBounds)) {
    return false;
  }
  return bounds_intersects(playerBounds, entityBounds);
}

static void clear_forward_block(GameState *state) {
  if (state->player.forwardBlocked) {
    state->player.forwardBlocked = false;
  }
}

static void end_run(GameState *state, uint32_t entityId,
                    CollisionReason reason) {
  GameEvent ev;

  state->status.type = STATUS_GAME_OVER;
  state->status.reason = reason;
  if (state->score > state->highScore) {
    state->highScore = state->score;
  }
  state->totalCoins += state->coins;
  state->hitStopSeconds = GAME_CONFIG.hitStopSeconds;
  state->deathAnimSeconds = 0.0f;
  state->coinStreak = 0;
  state->streakTimer = 0.0f;
  state->player.forwardBlocked = false;

  memset(&ev, 0, sizeof(ev));
  ev.type = EVENT_COLLISION;
  ev.entityId = entityId;
  ev.reason = reason;
  append_event(state, ev);

  memset(&ev, 0, sizeof(ev));
  ev.type = EVENT_GAME_OVER;
  append_event(state, ev);
}

static void save_with_board(GameState *state, uint32_t entityId,
                            CollisionReason reason) {
  GameEvent ev;
  Lane escapeLane =
      shift_lane(state->player.lane, state->player.lane == 1 ? -1 : 1);

  state->boardCharges -= 1;
  state->hitStopSeconds = GAME_CONFIG.hitStopSeconds;
  state->player.lane = escapeLane;
  state->player.velocityY = GAME_CONFIG.jumpVelocity * 0.45f;
  state->player.movement.type = MOVE_STUNNED;
  state->player.movement.remainingSeconds = GAME_CONFIG.boardStunSeconds;
  state->player.boardRecoverySeconds = GAME_CONFIG.boardRecoverySeconds;
  state->player.jumpBufferSeconds = 0.0f;
  state->player.slideBufferSeconds = 0.0f;
  state->player.forwardBlocked = false;

  memset(&ev, 0, sizeof(ev));
  ev.type = EVENT_BOARD_SAVED;
  ev.entityId = entityId;
  ev.reason = reason;
  append_event(state, ev);
}

static void resolve_immortal_collisions(GameState *state) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
  float pushBack = 0.0f;
  bool blocked = false;
  int i;

  for (i = 0; i < flatCount; i++) {
    CollisionReason reason;
    Bounds playerBounds;
    Bounds entityBounds;
    float playerFrontZ;
    float entityFrontZ;
    float neededPush;

    if (!entities_collide(&state->player, &flat[i])) {
      continue;
    }
    if (!collision_reason_for_entity(&flat[i], &reason)) {
      continue;
    }

    blocked = true;
    playerBounds = get_player_bounds(&state->player);
    if (!get_entity_bounds(&flat[i], &entityBounds)) {
      continue;
    }

    playerFrontZ = playerBounds.center.z - playerBounds.halfSize.z;
    entityFrontZ = entityBounds.center.z + entityBounds.halfSize.z;
    neededPush = entityFrontZ - playerFrontZ - CONTACT_SLOP;
    if (neededPush > 0.0f && neededPush <= MAX_IMMORTAL_RESOLVE_PUSH) {
      if (neededPush > pushBack) {
        pushBack = neededPush;
      }
    }
  }

  if (!blocked) {
    clear_forward_block(state);
    return;
  }

  if (pushBack <= 0.0f && state->player.forwardBlocked) {
    return;
  }

  state->player.position.z += pushBack;
  state->player.forwardBlocked = true;
  state->distance = state->distance - pushBack > 0.0f
                        ? state->distance - pushBack
                        : 0.0f;
  state->world.distanceTraveled =
      state->world.distanceTraveled - pushBack > 0.0f
          ? state->world.distanceTraveled - pushBack
          : 0.0f;
}

void detect_collisions(GameState *state) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int flatCount;
  int i;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  if (state->cheats.immortal) {
    resolve_immortal_collisions(state);
    return;
  }

  if (has_effect(state->effects, state->effectCount, POWERUP_INVINCIBLE) ||
      has_effect(state->effects, state->effectCount, POWERUP_BOOST) ||
      player_is_board_recovering(&state->player)) {
    clear_forward_block(state);
    return;
  }

  flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
  for (i = 0; i < flatCount; i++) {
    CollisionReason reason;
    if (!entities_collide(&state->player, &flat[i])) {
      continue;
    }
    if (!collision_reason_for_entity(&flat[i], &reason)) {
      continue;
    }
    if (state->boardCharges > 0) {
      save_with_board(state, flat[i].id, reason);
      return;
    }
    end_run(state, flat[i].id, reason);
    return;
  }

  clear_forward_block(state);
}

/* ---- near misses ---- */

static bool is_hazard(const GameEntity *entity) {
  return entity->kind == ENTITY_OBSTACLE || entity->kind == ENTITY_TRAIN;
}

static Bounds inflate_bounds(Bounds bounds, float pad) {
  return create_bounds(
      bounds.center,
      bounds.halfSize.x * 2.0f + pad,
      bounds.halfSize.y * 2.0f + pad * 0.5f,
      bounds.halfSize.z * 2.0f + pad);
}

static bool is_passing_z(float playerZ, float entityZ) {
  float dz = entityZ - playerZ;
  return dz >= -1.2f && dz <= 1.8f;
}

static bool near_miss_seen(const GameState *state, uint32_t id) {
  int i;
  for (i = 0; i < state->nearMissCount; i++) {
    if (state->nearMissIds[i] == id) {
      return true;
    }
  }
  return false;
}

static void near_miss_add(GameState *state, uint32_t id) {
  if (state->nearMissCount >= MAX_NEAR_MISS) {
    return;
  }
  state->nearMissIds[state->nearMissCount++] = id;
}

static bool was_close_call(const GameState *state, const GameEntity *entity) {
  const PlayerState *player = &state->player;
  Bounds entityBounds;
  Bounds playerBounds;
  Bounds nearBounds;
  float laneX;
  bool sameLaneFeel;

  if (!is_passing_z(player->position.z, entity->positionZ)) {
    return false;
  }
  if (!get_entity_bounds(entity, &entityBounds)) {
    return false;
  }

  playerBounds = get_player_bounds(player);
  if (bounds_intersects(playerBounds, entityBounds)) {
    return false;
  }

  laneX = (float)entity->lane * GAME_CONFIG.laneWidth;
  sameLaneFeel =
      fabs(player->position.x - laneX) < GAME_CONFIG.laneWidth * 0.85f;

  if (entity->kind == ENTITY_OBSTACLE) {
    if (can_avoid_by_jump(entity) &&
        player->position.y > GAME_CONFIG.jumpClearanceY && sameLaneFeel) {
      return true;
    }
    if (can_avoid_by_slide(entity) && player_is_sliding(player) &&
        sameLaneFeel) {
      return true;
    }
  }

  nearBounds = inflate_bounds(entityBounds, GAME_CONFIG.nearMissPadding);
  if (!bounds_intersects(playerBounds, nearBounds)) {
    return false;
  }

  if (entity->kind == ENTITY_OBSTACLE) {
    if (entity->variant == OBSTACLE_BARRIER ||
        entity->variant == OBSTACLE_CRATE) {
      return fabs(player->position.x - laneX) <
             GAME_CONFIG.laneWidth * 0.95f;
    }
    return sameLaneFeel;
  }

  if (entity->kind == ENTITY_TRAIN) {
    return fabs(player->position.x - laneX) < GAME_CONFIG.laneWidth * 1.05f &&
           player->position.y < GAME_CONFIG.trainRoofY - 0.2f;
  }

  return false;
}

void detect_near_misses(GameState *state) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int flatCount;
  float scoreGain = 0.0f;
  float shake;
  int i;
  int write;
  float playerZ;
  GameEvent events[MAX_EVENTS];
  int eventCount = 0;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  shake = state->cameraShake;
  flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);

  for (i = 0; i < flatCount; i++) {
    GameEvent ev;
    if (!is_hazard(&flat[i]) || near_miss_seen(state, flat[i].id)) {
      continue;
    }
    if (!was_close_call(state, &flat[i])) {
      continue;
    }
    near_miss_add(state, flat[i].id);
    scoreGain += GAME_CONFIG.nearMissScore;
    if (GAME_CONFIG.nearMissShake > shake) {
      shake = GAME_CONFIG.nearMissShake;
    }
    if (eventCount < MAX_EVENTS) {
      memset(&ev, 0, sizeof(ev));
      ev.type = EVENT_NEAR_MISS;
      ev.entityId = flat[i].id;
      ev.points = GAME_CONFIG.nearMissScore;
      events[eventCount++] = ev;
    }
  }

  if (eventCount == 0) {
    return;
  }

  playerZ = state->player.position.z;
  write = 0;
  for (i = 0; i < state->nearMissCount; i++) {
    uint32_t id = state->nearMissIds[i];
    int j;
    bool found = false;
    for (j = 0; j < flatCount; j++) {
      if (flat[j].id == id) {
        if (flat[j].positionZ < playerZ + GAME_CONFIG.despawnDistance) {
          found = true;
        }
        break;
      }
    }
    if (found) {
      state->nearMissIds[write++] = id;
    }
  }
  state->nearMissCount = write;
  state->score += scoreGain;
  state->cameraShake = shake;

  for (i = 0; i < eventCount; i++) {
    append_event(state, events[i]);
  }
}

/* ---- collect ---- */

static float effect_duration(PowerUpType type) {
  switch (type) {
    case POWERUP_MAGNET:
      return GAME_CONFIG.magnetDurationSeconds;
    case POWERUP_MULTIPLIER:
      return GAME_CONFIG.multiplierDurationSeconds;
    case POWERUP_INVINCIBLE:
      return GAME_CONFIG.invincibleDurationSeconds;
    case POWERUP_BOOST:
      return GAME_CONFIG.boostDurationSeconds;
  }
  return 0.0f;
}

static void upsert_effect(GameState *state, PowerUpType type) {
  int i;
  for (i = 0; i < state->effectCount; i++) {
    if (state->effects[i].type == type) {
      state->effects[i].remainingSeconds = effect_duration(type);
      return;
    }
  }
  if (state->effectCount < MAX_EFFECTS) {
    state->effects[state->effectCount].type = type;
    state->effects[state->effectCount].remainingSeconds =
        effect_duration(type);
    state->effectCount++;
  }
}

static int streak_bonus(int streak) {
  int v = streak - 1;
  if (v < 0) {
    v = 0;
  }
  v = v / 3;
  if (v > 4) {
    v = 4;
  }
  return 1 + v;
}

static bool can_collect(const PlayerState *player, const GameEntity *entity) {
  Bounds entityBounds;
  if (!get_entity_bounds(entity, &entityBounds)) {
    return false;
  }
  return bounds_intersects(get_player_bounds(player), entityBounds);
}

static void mark_collected_in_list(GameEntity *entities, int count,
                                   const uint32_t *ids, int idCount) {
  int e;
  int i;
  for (e = 0; e < count; e++) {
    for (i = 0; i < idCount; i++) {
      if (entities[e].id == ids[i]) {
        if (entities[e].kind == ENTITY_COIN ||
            entities[e].kind == ENTITY_POWER_UP) {
          entities[e].collected = true;
        }
        break;
      }
    }
  }
}

void collect_items(GameState *state) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int flatCount;
  float multiplier;
  float scoreGain = 0.0f;
  int coinsGain = 0;
  uint32_t collectedIds[MAX_FLATTEN_ENTITIES];
  int collectedCount = 0;
  GameEvent events[MAX_EVENTS];
  int eventCount = 0;
  int i;
  int s;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  multiplier =
      get_multiplier(state->effects, state->effectCount, &state->cheats);
  flatCount = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);

  for (i = 0; i < flatCount; i++) {
    GameEvent ev;
    if (flat[i].kind == ENTITY_COIN && !flat[i].collected) {
      float combo;
      float points;
      if (!can_collect(&state->player, &flat[i])) {
        continue;
      }
      if (collectedCount < MAX_FLATTEN_ENTITIES) {
        collectedIds[collectedCount++] = flat[i].id;
      }
      coinsGain += 1;
      state->coinStreak += 1;
      state->streakTimer = GAME_CONFIG.coinStreakTimeoutSeconds;
      combo = (float)streak_bonus(state->coinStreak);
      points = GAME_CONFIG.coinValue * multiplier * combo;
      scoreGain += points;
      if (eventCount < MAX_EVENTS) {
        memset(&ev, 0, sizeof(ev));
        ev.type = EVENT_COIN_COLLECTED;
        ev.entityId = flat[i].id;
        ev.streak = state->coinStreak;
        ev.points = points;
        events[eventCount++] = ev;
      }
      continue;
    }

    if (flat[i].kind == ENTITY_POWER_UP && !flat[i].collected) {
      if (!can_collect(&state->player, &flat[i])) {
        continue;
      }
      if (collectedCount < MAX_FLATTEN_ENTITIES) {
        collectedIds[collectedCount++] = flat[i].id;
      }
      upsert_effect(state, flat[i].powerUpType);
      if (eventCount < MAX_EVENTS) {
        memset(&ev, 0, sizeof(ev));
        ev.type = EVENT_POWER_UP_COLLECTED;
        ev.entityId = flat[i].id;
        ev.powerUpType = flat[i].powerUpType;
        events[eventCount++] = ev;
      }
    }
  }

  if (collectedCount == 0) {
    return;
  }

  mark_collected_in_list(state->entities, state->entityCount, collectedIds,
                         collectedCount);
  for (s = 0; s < state->world.segmentCount; s++) {
    mark_collected_in_list(state->world.segments[s].entities,
                           state->world.segments[s].entityCount,
                           collectedIds, collectedCount);
  }

  state->score += scoreGain;
  state->coins += coinsGain;

  for (i = 0; i < eventCount; i++) {
    append_event(state, events[i]);
  }
}

/* ---- attract ---- */

static void pull_coin(GameEntity *coin, const PlayerState *player, float dt) {
  float dx = player->position.x - coin->positionX;
  float dz = player->position.z - coin->positionZ;
  float dy = player->position.y + 1.0f - coin->height;
  float distance = (float)sqrt((double)(dx * dx + dz * dz + dy * dy));
  float step;
  float t;

  if (distance > GAME_CONFIG.magnetRadius || distance < 0.001f) {
    return;
  }

  step = GAME_CONFIG.magnetPullSpeed * dt;
  t = step / distance;
  if (t > 1.0f) {
    t = 1.0f;
  }
  coin->positionX += dx * t;
  coin->positionZ += dz * t;
  coin->height =
      approach_f(coin->height, player->position.y + 1.0f, step);
}

static void map_pull_coins(GameEntity *entities, int count,
                           const PlayerState *player, float dt) {
  int i;
  for (i = 0; i < count; i++) {
    if (entities[i].kind != ENTITY_COIN || entities[i].collected) {
      continue;
    }
    pull_coin(&entities[i], player, dt);
  }
}

void attract_coins(GameState *state, float dt) {
  int s;
  if (state->status.type != STATUS_RUNNING) {
    return;
  }
  if (!is_magnet_active(state->effects, state->effectCount, &state->cheats)) {
    return;
  }

  map_pull_coins(state->entities, state->entityCount, &state->player, dt);
  for (s = 0; s < state->world.segmentCount; s++) {
    map_pull_coins(state->world.segments[s].entities,
                   state->world.segments[s].entityCount, &state->player, dt);
  }
}

void update_effects(GameState *state, float dt) {
  int write = 0;
  int i;
  if (state->status.type != STATUS_RUNNING) {
    return;
  }
  for (i = 0; i < state->effectCount; i++) {
    ActiveEffect e = state->effects[i];
    e.remainingSeconds -= dt;
    if (e.remainingSeconds > 0.0f) {
      state->effects[write++] = e;
    }
  }
  state->effectCount = write;
}
