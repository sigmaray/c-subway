#include "metro.h"
#include <string.h>

int flatten_entities(const GameState *state, GameEntity *out, int max_out) {
  int count = 0;
  int s;
  int e;

  for (s = 0; s < state->world.segmentCount; s++) {
    const WorldSegment *seg = &state->world.segments[s];
    for (e = 0; e < seg->entityCount; e++) {
      if (count >= max_out) {
        return count;
      }
      out[count++] = seg->entities[e];
    }
  }
  for (e = 0; e < state->entityCount; e++) {
    if (count >= max_out) {
      return count;
    }
    out[count++] = state->entities[e];
  }
  return count;
}

void update_world(GameState *state, float dt) {
  float boostBonus;
  float effectiveSpeed;
  float traveled;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  boostBonus =
      has_effect(state->effects, state->effectCount, POWERUP_BOOST)
          ? GAME_CONFIG.boostSpeedBonus
          : 0.0f;
  effectiveSpeed =
      state->player.forwardBlocked ? 0.0f : state->speed + boostBonus;
  traveled = effectiveSpeed * dt;

  state->elapsedSeconds += dt;
  state->distance += traveled;
  state->world.distanceTraveled += traveled;
  state->player.position.z -= traveled;
}

static void update_entity(GameEntity *entity, float dt) {
  if (entity->kind == ENTITY_TRAIN) {
    entity->positionZ += entity->speed * dt;
  }
}

void update_entities(GameState *state, float dt) {
  int s;
  int e;

  if (state->status.type != STATUS_RUNNING) {
    return;
  }

  for (e = 0; e < state->entityCount; e++) {
    update_entity(&state->entities[e], dt);
  }
  for (s = 0; s < state->world.segmentCount; s++) {
    WorldSegment *seg = &state->world.segments[s];
    for (e = 0; e < seg->entityCount; e++) {
      update_entity(&seg->entities[e], dt);
    }
  }
}

bool is_behind_despawn_line(float positionZ, float playerZ,
                            float despawnDistance) {
  return positionZ >= playerZ + despawnDistance;
}

static bool keep_entity(const GameEntity *entity, float playerZ) {
  float rearZ;
  if (entity->kind == ENTITY_COIN && entity->collected) {
    return false;
  }
  if (entity->kind == ENTITY_POWER_UP && entity->collected) {
    return false;
  }
  rearZ = entity->kind == ENTITY_TRAIN
              ? entity->positionZ - entity->length
              : entity->positionZ;
  return !is_behind_despawn_line(rearZ, playerZ, GAME_CONFIG.despawnDistance);
}

static void compact_entities(GameEntity *entities, int *count, float playerZ) {
  int write = 0;
  int read;
  for (read = 0; read < *count; read++) {
    if (keep_entity(&entities[read], playerZ)) {
      if (write != read) {
        entities[write] = entities[read];
      }
      write++;
    }
  }
  *count = write;
}

void remove_expired_entities(GameState *state) {
  float playerZ = state->player.position.z;
  int s;
  int write;

  compact_entities(state->entities, &state->entityCount, playerZ);

  write = 0;
  for (s = 0; s < state->world.segmentCount; s++) {
    WorldSegment *seg = &state->world.segments[s];
    compact_entities(seg->entities, &seg->entityCount, playerZ);
    if (!is_behind_despawn_line(seg->startZ - seg->length, playerZ,
                                GAME_CONFIG.despawnDistance)) {
      if (write != s) {
        state->world.segments[write] = *seg;
      }
      write++;
    }
  }
  state->world.segmentCount = write;
}
