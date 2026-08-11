#include "metro.h"
#include <math.h>
#include <string.h>

typedef struct SpawnContext {
  uint32_t   seed;
  float      difficulty;
  CheatFlags cheats;
} SpawnContext;

typedef enum RowKind {
  ROW_JUMP = 0,
  ROW_SLIDE,
  ROW_DODGE,
  ROW_TRAIN,
  ROW_COINS,
  ROW_WEAVE,
  ROW_DUAL_TRAIN,
  ROW_MIXED
} RowKind;

static const Lane LANES[3] = { (Lane)-1, (Lane)0, (Lane)1 };

/* Choreographed beat sequences — match BEAT_SEQUENCES in spawn-segments.ts */
static const RowKind BEAT_0[] = { ROW_JUMP, ROW_COINS, ROW_SLIDE, ROW_DODGE };
static const RowKind BEAT_1[] = { ROW_DODGE, ROW_TRAIN, ROW_WEAVE, ROW_SLIDE };
static const RowKind BEAT_2[] = { ROW_JUMP, ROW_SLIDE, ROW_DODGE, ROW_COINS };
static const RowKind BEAT_3[] = { ROW_TRAIN, ROW_JUMP, ROW_WEAVE, ROW_SLIDE };
static const RowKind BEAT_4[] = { ROW_SLIDE, ROW_COINS, ROW_TRAIN, ROW_DODGE };
static const RowKind BEAT_5[] = { ROW_WEAVE, ROW_JUMP, ROW_DODGE, ROW_TRAIN };
static const RowKind BEAT_6[] = { ROW_TRAIN, ROW_SLIDE, ROW_COINS };
static const RowKind BEAT_7[] = { ROW_DODGE, ROW_JUMP, ROW_TRAIN, ROW_WEAVE };
static const RowKind BEAT_8[] = { ROW_JUMP, ROW_TRAIN, ROW_SLIDE };
static const RowKind BEAT_9[] = {
    ROW_COINS, ROW_SLIDE, ROW_JUMP, ROW_DODGE, ROW_TRAIN };
static const RowKind BEAT_10[] = { ROW_TRAIN, ROW_WEAVE, ROW_SLIDE, ROW_JUMP };
static const RowKind BEAT_11[] = { ROW_MIXED, ROW_JUMP, ROW_SLIDE, ROW_COINS };

typedef struct BeatSeq {
  const RowKind *rows;
  int            count;
} BeatSeq;

static const BeatSeq BEAT_SEQUENCES[] = {
    { BEAT_0, 4 },  { BEAT_1, 4 },  { BEAT_2, 4 },  { BEAT_3, 4 },
    { BEAT_4, 4 },  { BEAT_5, 4 },  { BEAT_6, 3 },  { BEAT_7, 4 },
    { BEAT_8, 3 },  { BEAT_9, 5 },  { BEAT_10, 4 }, { BEAT_11, 4 },
};

static const int BEAT_COUNT =
    (int)(sizeof(BEAT_SEQUENCES) / sizeof(BEAT_SEQUENCES[0]));

static const PowerUpType POWER_UPS[4] = {
    POWERUP_MAGNET, POWERUP_MULTIPLIER, POWERUP_INVINCIBLE, POWERUP_BOOST
};

static bool power_up_allowed(PowerUpType type, const CheatFlags *cheats) {
  if (cheats->noMagnets && type == POWERUP_MAGNET) {
    return false;
  }
  if (cheats->noBoost && type == POWERUP_BOOST) {
    return false;
  }
  return true;
}

static PowerUpType pick_power_up(uint32_t *seed, const CheatFlags *cheats) {
  PowerUpType available[4];
  int count = 0;
  int i;
  IntResult roll;

  for (i = 0; i < 4; i++) {
    if (power_up_allowed(POWER_UPS[i], cheats)) {
      available[count++] = POWER_UPS[i];
    }
  }
  if (count == 0) {
    return POWERUP_MULTIPLIER;
  }
  roll = pick_index(*seed, count);
  *seed = roll.seed;
  return available[roll.value];
}

static void remove_power_up_from_list(GameEntity *entities, int *count,
                                      PowerUpType type) {
  int write = 0;
  int read;
  for (read = 0; read < *count; read++) {
    GameEntity *entity = &entities[read];
    if (entity->kind == ENTITY_POWER_UP && entity->powerUpType == type) {
      continue;
    }
    if (write != read) {
      entities[write] = entities[read];
    }
    write++;
  }
  *count = write;
}

void purge_blocked_power_ups(GameState *state) {
  int s;
  if (state == NULL) {
    return;
  }
  if (state->cheats.noMagnets) {
    remove_power_up_from_list(state->entities, &state->entityCount,
                              POWERUP_MAGNET);
    for (s = 0; s < state->world.segmentCount; s++) {
      WorldSegment *seg = &state->world.segments[s];
      remove_power_up_from_list(seg->entities, &seg->entityCount,
                                POWERUP_MAGNET);
    }
  }
  if (state->cheats.noBoost) {
    remove_power_up_from_list(state->entities, &state->entityCount,
                              POWERUP_BOOST);
    for (s = 0; s < state->world.segmentCount; s++) {
      WorldSegment *seg = &state->world.segments[s];
      remove_power_up_from_list(seg->entities, &seg->entityCount,
                                POWERUP_BOOST);
    }
  }
}

static float difficulty_from_state(const GameState *state) {
  float speedRange = GAME_CONFIG.maximumSpeed - GAME_CONFIG.initialSpeed;
  if (speedRange <= 0.0f) {
    return 0.0f;
  }
  {
    float d = (state->speed - GAME_CONFIG.initialSpeed) / speedRange;
    return d < 1.0f ? d : 1.0f;
  }
}

static bool push_entity(WorldSegment *seg, GameEntity entity) {
  if (seg->entityCount >= MAX_ENTITIES_PER_SEGMENT) {
    return false;
  }
  seg->entities[seg->entityCount++] = entity;
  return true;
}

static GameEntity make_obstacle(Lane lane, float positionZ,
                                ObstacleVariant variant) {
  GameEntity e;
  memset(&e, 0, sizeof(e));
  e.id = create_entity_id();
  e.kind = ENTITY_OBSTACLE;
  e.variant = variant;
  e.lane = lane;
  e.positionZ = positionZ;
  switch (variant) {
    case OBSTACLE_BARRIER:
      e.height = 1.55f;
      e.width = GAME_CONFIG.laneWidth * 0.72f;
      e.depth = 0.4f;
      e.baseY = 0.0f;
      break;
    case OBSTACLE_LOW_BARRIER:
      e.height = 0.5f;
      e.width = GAME_CONFIG.laneWidth * 0.72f;
      e.depth = 0.45f;
      e.baseY = 0.0f;
      break;
    case OBSTACLE_OVERHEAD:
      e.height = 1.05f;
      e.width = GAME_CONFIG.laneWidth * 0.76f;
      e.depth = 0.42f;
      e.baseY = 1.05f;
      break;
    case OBSTACLE_CRATE:
      e.height = 1.15f;
      e.width = 0.92f;
      e.depth = 0.85f;
      e.baseY = 0.0f;
      break;
  }
  return e;
}

static GameEntity make_coin(Lane lane, float positionZ, float height) {
  GameEntity e;
  memset(&e, 0, sizeof(e));
  e.id = create_entity_id();
  e.kind = ENTITY_COIN;
  e.lane = lane;
  e.positionX = (float)lane * GAME_CONFIG.laneWidth;
  e.positionZ = positionZ;
  e.height = height;
  e.collected = false;
  return e;
}

static GameEntity make_train(Lane lane, float positionZ, float speed,
                             int cars) {
  GameEntity e;
  const float carLength = 7.2f;
  memset(&e, 0, sizeof(e));
  e.id = create_entity_id();
  e.kind = ENTITY_TRAIN;
  e.lane = lane;
  e.positionZ = positionZ;
  e.length = (float)cars * carLength;
  e.speed = speed;
  e.cars = cars;
  return e;
}

static GameEntity make_power_up(Lane lane, float positionZ,
                                PowerUpType powerUpType) {
  GameEntity e;
  memset(&e, 0, sizeof(e));
  e.id = create_entity_id();
  e.kind = ENTITY_POWER_UP;
  e.powerUpType = powerUpType;
  e.lane = lane;
  e.positionZ = positionZ;
  e.height = 1.25f;
  e.collected = false;
  return e;
}

static GameEntity make_decoration(Lane lane, float positionZ,
                                  DecorationStyle style) {
  GameEntity e;
  memset(&e, 0, sizeof(e));
  e.id = create_entity_id();
  e.kind = ENTITY_DECORATION;
  e.lane = lane;
  e.positionZ = positionZ;
  e.style = style;
  return e;
}

typedef struct LanePick {
  Lane     value;
  uint32_t seed;
} LanePick;

static LanePick pick_lane(uint32_t seed) {
  LanePick r;
  IntResult picked = pick_index(seed, 3);
  r.value = LANES[picked.value];
  r.seed = picked.seed;
  return r;
}

/* blocked[0]=lane-1, blocked[1]=lane0, blocked[2]=lane1 */
static int pick_safe_lanes(const bool blocked[3], Lane *out) {
  int n = 0;
  int i;
  for (i = 0; i < 3; i++) {
    if (!blocked[i]) {
      out[n++] = LANES[i];
    }
  }
  return n;
}

static void add_coin_line(WorldSegment *seg, Lane lane, float startZ,
                          int count, float baseHeight, bool arc) {
  int i;
  for (i = 0; i < count; i++) {
    float height;
    if (arc) {
      float denom = count - 1 > 0 ? (float)(count - 1) : 1.0f;
      height =
          baseHeight +
          (float)sin((double)((float)i / denom) * 3.141592653589793) * 1.1f;
    } else {
      height = baseHeight + (float)(i % 2) * 0.28f;
    }
    push_entity(seg, make_coin(lane, startZ - (float)i * 1.35f, height));
  }
}

static void add_coin_weave(WorldSegment *seg, Lane startLane, float startZ,
                           int count, float baseHeight) {
  static const Lane path0[] = { 0, 1, 0, -1, 0, 1, 0, -1 };
  static const Lane path1[] = { 1, 0, -1, 0, 1, 0, -1, 0 };
  static const Lane pathM[] = { -1, 0, 1, 0, -1, 0, 1, 0 };
  const Lane *path;
  int i;
  if (startLane == 0) {
    path = path0;
  } else if (startLane == 1) {
    path = path1;
  } else {
    path = pathM;
  }
  for (i = 0; i < count; i++) {
    Lane lane = path[i % 8];
    float denom = count - 1 > 0 ? (float)(count - 1) : 1.0f;
    float height =
        baseHeight +
        (float)sin((double)((float)i / denom) * 3.141592653589793) * 0.55f;
    push_entity(seg, make_coin(lane, startZ - (float)i * 1.45f, height));
  }
}

static void add_sky_coin_arc(WorldSegment *seg, Lane lane, float startZ,
                             int count) {
  int i;
  for (i = 0; i < count; i++) {
    float denom = count - 1 > 0 ? (float)(count - 1) : 1.0f;
    float t = (float)i / denom;
    float height = GAME_CONFIG.boostHoverHeight - 0.4f +
                   (float)sin((double)t * 3.141592653589793) * 0.9f;
    push_entity(seg, make_coin(lane, startZ - (float)i * 1.5f, height));
  }
}

static uint32_t spawn_jump_row(WorldSegment *seg, float rowZ,
                               SpawnContext ctx, uint32_t seed) {
  bool blocked[3] = { false, false, false };
  int maxExclusive = 1 + (int)floor((double)(ctx.difficulty * 0.75f));
  IntResult countRoll;
  int i;

  if (maxExclusive > 2) {
    maxExclusive = 2;
  }
  countRoll = next_int(seed, 1, maxExclusive);
  seed = countRoll.seed;

  for (i = 0; i < countRoll.value; i++) {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    IntResult laneRoll;
    Lane lane;
    if (safeCount <= 1) {
      break;
    }
    laneRoll = pick_index(seed, safeCount);
    seed = laneRoll.seed;
    lane = safe[laneRoll.value];
    blocked[lane + 1] = true;
    push_entity(seg, make_obstacle(lane, rowZ, OBSTACLE_LOW_BARRIER));
  }

  {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    for (i = 0; i < safeCount; i++) {
      add_coin_line(seg, safe[i], rowZ - 1.2f, 7, 1.55f, true);
    }
  }
  return seed;
}

static uint32_t spawn_slide_row(WorldSegment *seg, float rowZ,
                                SpawnContext ctx, uint32_t seed) {
  bool blocked[3] = { false, false, false };
  int maxExclusive = 1 + (int)floor((double)(ctx.difficulty * 0.75f));
  IntResult countRoll;
  int i;

  if (maxExclusive > 2) {
    maxExclusive = 2;
  }
  countRoll = next_int(seed, 1, maxExclusive);
  seed = countRoll.seed;

  for (i = 0; i < countRoll.value; i++) {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    IntResult laneRoll;
    Lane lane;
    if (safeCount <= 1) {
      break;
    }
    laneRoll = pick_index(seed, safeCount);
    seed = laneRoll.seed;
    lane = safe[laneRoll.value];
    blocked[lane + 1] = true;
    push_entity(seg, make_obstacle(lane, rowZ, OBSTACLE_OVERHEAD));
  }

  {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    for (i = 0; i < safeCount; i++) {
      add_coin_line(seg, safe[i], rowZ - 0.8f, 6, 0.85f, false);
    }
  }
  return seed;
}

static uint32_t spawn_dodge_row(WorldSegment *seg, float rowZ,
                                SpawnContext ctx, uint32_t seed) {
  LanePick freePick = pick_lane(seed);
  int i;
  seed = freePick.seed;

  for (i = 0; i < 3; i++) {
    Lane lane = LANES[i];
    if (lane == freePick.value) {
      add_coin_line(seg, lane, rowZ - 1.0f, 6, 1.1f, false);
      continue;
    }
    {
      ChanceResult variantRoll =
          chance(seed, 0.55f + ctx.difficulty * 0.2f);
      seed = variantRoll.seed;
      push_entity(seg,
                  make_obstacle(lane, rowZ,
                                variantRoll.hit ? OBSTACLE_BARRIER
                                                : OBSTACLE_CRATE));
    }
  }
  return seed;
}

static uint32_t spawn_train_row(WorldSegment *seg, float rowZ,
                                SpawnContext ctx, uint32_t seed) {
  LanePick lanePick = pick_lane(seed);
  float movingChance = 0.25f + ctx.difficulty * 0.35f;
  ChanceResult movingRoll;
  IntResult carsRoll;
  RandomResult speedRoll;
  int cars;
  bool oncoming;
  float speed;
  GameEntity train;
  int roofCount;
  int i;

  seed = lanePick.seed;
  movingRoll = chance(seed, movingChance);
  seed = movingRoll.seed;
  carsRoll =
      next_int(seed, 2, 3 + (int)floor((double)(ctx.difficulty * 2.0f)));
  seed = carsRoll.seed;
  speedRoll = next_random(seed);
  seed = speedRoll.seed;

  cars = carsRoll.value;
  if (cars < 2) {
    cars = 2;
  }
  oncoming = movingRoll.hit;
  speed = oncoming ? 4.0f + speedRoll.value * 5.0f + ctx.difficulty * 5.0f
                   : 0.0f;
  train = make_train(lanePick.value, rowZ + 10.0f, speed, cars);
  push_entity(seg, train);

  roofCount = 3 + cars * 2;
  if (roofCount > 8) {
    roofCount = 8;
  }
  for (i = 0; i < roofCount; i++) {
    float z = train.positionZ - 1.8f - (float)i * 1.55f;
    if (z < train.positionZ - train.length + 1.2f) {
      break;
    }
    push_entity(seg, make_coin(lanePick.value, z,
                               GAME_CONFIG.trainRoofY + 1.05f));
  }

  for (i = 0; i < 3; i++) {
    Lane lane = LANES[i];
    ChanceResult coinChance;
    if (lane == lanePick.value) {
      continue;
    }
    coinChance = chance(seed, 0.7f);
    seed = coinChance.seed;
    if (coinChance.hit) {
      add_coin_line(seg, lane, rowZ - 1.0f, 6, 1.15f, false);
    }
  }

  if (ctx.difficulty > 0.25f) {
    ChanceResult skyRoll =
        chance(seed, 0.35f + ctx.difficulty * 0.25f);
    seed = skyRoll.seed;
    if (skyRoll.hit) {
      bool blocked[3] = { false, false, false };
      Lane safe[3];
      int safeCount;
      Lane skyLane;
      blocked[lanePick.value + 1] = true;
      safeCount = pick_safe_lanes(blocked, safe);
      skyLane = safeCount > 0 ? safe[0] : (Lane)0;
      add_sky_coin_arc(seg, skyLane, rowZ - 2.0f, 5);
    }
  }
  return seed;
}

static uint32_t spawn_dual_train_row(WorldSegment *seg, float rowZ,
                                     SpawnContext ctx, uint32_t seed) {
  LanePick freePick = pick_lane(seed);
  IntResult carsRoll;
  int cars;
  int i;

  seed = freePick.seed;
  carsRoll =
      next_int(seed, 2, 3 + (int)floor((double)ctx.difficulty));
  seed = carsRoll.seed;
  cars = carsRoll.value;
  if (cars < 2) {
    cars = 2;
  }

  for (i = 0; i < 3; i++) {
    Lane lane = LANES[i];
    if (lane == freePick.value) {
      add_coin_weave(seg, lane, rowZ - 1.0f, 7, 1.2f);
      continue;
    }
    {
      RandomResult speedRoll = next_random(seed);
      ChanceResult movingRoll;
      RandomResult offsetRoll;
      float speed;
      seed = speedRoll.seed;
      movingRoll = chance(seed, 0.3f + ctx.difficulty * 0.25f);
      seed = movingRoll.seed;
      speed = movingRoll.hit
                  ? 3.5f + speedRoll.value * 4.5f + ctx.difficulty * 4.0f
                  : 0.0f;
      offsetRoll = next_random(seed);
      seed = offsetRoll.seed;
      push_entity(seg,
                  make_train(lane, rowZ + 8.0f + offsetRoll.value * 4.0f,
                             speed, cars));
    }
  }
  return seed;
}

static uint32_t spawn_weave_row(WorldSegment *seg, float rowZ,
                                uint32_t seed) {
  LanePick lanePick = pick_lane(seed);
  seed = lanePick.seed;
  add_coin_weave(seg, lanePick.value, rowZ, 10, 1.15f);
  return seed;
}

static uint32_t spawn_coin_row(WorldSegment *seg, float rowZ, uint32_t seed) {
  LanePick lanePick = pick_lane(seed);
  ChanceResult arcRoll;
  seed = lanePick.seed;
  arcRoll = chance(seed, 0.5f);
  seed = arcRoll.seed;
  add_coin_line(seg, lanePick.value, rowZ, 8,
                arcRoll.hit ? 1.2f : 1.05f, arcRoll.hit);
  return seed;
}

static uint32_t spawn_mixed_row(WorldSegment *seg, float rowZ,
                                SpawnContext ctx, uint32_t seed) {
  bool blocked[3] = { false, false, false };
  ChanceResult trainRoll;
  IntResult obstacleCountRoll;
  int placed;
  static const ObstacleVariant variants[4] = {
      OBSTACLE_BARRIER, OBSTACLE_LOW_BARRIER, OBSTACLE_OVERHEAD,
      OBSTACLE_CRATE
  };
  int maxExclusive;
  int i;

  trainRoll = chance(seed, 0.06f + ctx.difficulty * 0.1f);
  seed = trainRoll.seed;
  if (trainRoll.hit) {
    LanePick lanePick = pick_lane(seed);
    ChanceResult movingRoll;
    RandomResult speedRoll;
    int cars;
    GameEntity train;
    seed = lanePick.seed;
    blocked[lanePick.value + 1] = true;
    movingRoll = chance(seed, 0.5f);
    seed = movingRoll.seed;
    speedRoll = next_random(seed);
    seed = speedRoll.seed;
    cars = 1 + (int)floor((double)(ctx.difficulty * 2.0f));
    if (cars < 2) {
      cars = 2;
    }
    train = make_train(
        lanePick.value, rowZ + 5.0f,
        movingRoll.hit ? 2.5f + speedRoll.value * 4.0f + ctx.difficulty * 3.0f
                       : 0.0f,
        cars);
    push_entity(seg, train);
    for (i = 0; i < 4; i++) {
      float z = train.positionZ - 1.6f - (float)i * 1.5f;
      if (z < train.positionZ - train.length + 1.0f) {
        break;
      }
      push_entity(seg, make_coin(lanePick.value, z,
                                 GAME_CONFIG.trainRoofY + 1.05f));
    }
  }

  maxExclusive = 1 + (int)floor((double)(ctx.difficulty * 0.75f));
  if (maxExclusive > 2) {
    maxExclusive = 2;
  }
  obstacleCountRoll = next_int(seed, 1, maxExclusive);
  seed = obstacleCountRoll.seed;
  placed = 0;

  while (placed < obstacleCountRoll.value) {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    IntResult laneRoll;
    IntResult variantRoll;
    Lane lane;
    ObstacleVariant variant;
    if (safeCount <= 1) {
      break;
    }
    laneRoll = pick_index(seed, safeCount);
    seed = laneRoll.seed;
    lane = safe[laneRoll.value];
    variantRoll = pick_index(seed, 4);
    seed = variantRoll.seed;
    variant = variants[variantRoll.value];
    blocked[lane + 1] = true;
    push_entity(seg, make_obstacle(lane, rowZ, variant));
    placed += 1;
  }

  {
    Lane safe[3];
    int safeCount = pick_safe_lanes(blocked, safe);
    for (i = 0; i < safeCount; i++) {
      ChanceResult coinChance =
          chance(seed, 0.6f + ctx.difficulty * 0.15f);
      seed = coinChance.seed;
      if (coinChance.hit) {
        add_coin_line(seg, safe[i], rowZ - 1.2f, 3, 1.1f, false);
      }
    }
  }

  return seed;
}

static uint32_t spawn_pattern(WorldSegment *seg, float startZ,
                              SpawnContext ctx) {
  uint32_t seed = ctx.seed;
  float gapScale = 1.0f - ctx.difficulty * 0.18f;
  IntResult beatPick = pick_index(seed, BEAT_COUNT);
  const BeatSeq *beat;
  RowKind rows[16];
  int rowCount;
  int extraRows;
  int row;
  int side;
  int i;

  seed = beatPick.seed;
  beat = &BEAT_SEQUENCES[beatPick.value];
  rowCount = 0;
  for (i = 0; i < beat->count && rowCount < 16; i++) {
    rows[rowCount++] = beat->rows[i];
  }
  extraRows = (int)floor((double)(ctx.difficulty * 0.5f));
  for (i = 0; i < extraRows && rowCount < 16; i++) {
    IntResult extraPick = pick_index(seed, beat->count);
    seed = extraPick.seed;
    rows[rowCount++] = beat->rows[extraPick.value];
  }

  for (row = 0; row < rowCount; row++) {
    float rowZ = startZ - 6.0f - (float)row * (12.5f * gapScale + 4.5f);
    RowKind kind = rows[row];
    ChanceResult powerChance;

    switch (kind) {
      case ROW_JUMP:
        seed = spawn_jump_row(seg, rowZ, ctx, seed);
        break;
      case ROW_SLIDE:
        seed = spawn_slide_row(seg, rowZ, ctx, seed);
        break;
      case ROW_DODGE:
        seed = spawn_dodge_row(seg, rowZ, ctx, seed);
        break;
      case ROW_TRAIN:
        seed = spawn_train_row(seg, rowZ, ctx, seed);
        break;
      case ROW_DUAL_TRAIN:
        seed = spawn_dual_train_row(seg, rowZ, ctx, seed);
        break;
      case ROW_COINS:
        seed = spawn_coin_row(seg, rowZ, seed);
        break;
      case ROW_WEAVE:
        seed = spawn_weave_row(seg, rowZ, seed);
        break;
      case ROW_MIXED:
        seed = spawn_mixed_row(seg, rowZ, ctx, seed);
        break;
    }

    powerChance = chance(seed, 0.09f + ctx.difficulty * 0.07f);
    seed = powerChance.seed;
    if (powerChance.hit) {
      LanePick lanePick = pick_lane(seed);
      PowerUpType powerUpType;
      seed = lanePick.seed;
      powerUpType = pick_power_up(&seed, &ctx.cheats);
      push_entity(seg,
                  make_power_up(lanePick.value, rowZ - 4.5f, powerUpType));
      if (powerUpType == POWERUP_BOOST) {
        add_sky_coin_arc(seg, lanePick.value, rowZ - 8.0f, 8);
      }
    }
  }

  for (side = 0; side < 2; side++) {
    Lane sideLane = side == 0 ? (Lane)-1 : (Lane)1;
    for (i = 0; i < 4; i++) {
      ChanceResult decorChance = chance(seed, 0.9f);
      IntResult styleRoll;
      DecorationStyle style;
      seed = decorChance.seed;
      if (!decorChance.hit) {
        continue;
      }
      styleRoll = next_int(seed, 0, 3);
      seed = styleRoll.seed;
      style = (DecorationStyle)styleRoll.value;
      push_entity(seg,
                  make_decoration(
                      sideLane,
                      startZ - 5.0f - (float)i * 10.0f -
                          GAME_CONFIG.segmentLength * 0.1f,
                      style));
    }
  }

  return seed;
}

static void create_segment(WorldSegment *out, float startZ, uint32_t seed,
                           float difficulty, CheatFlags cheats,
                           uint32_t *outSeed) {
  SpawnContext ctx;
  memset(out, 0, sizeof(*out));
  out->id = create_segment_id();
  out->startZ = startZ;
  out->length = GAME_CONFIG.segmentLength;
  out->entityCount = 0;
  ctx.seed = seed;
  ctx.difficulty = difficulty;
  ctx.cheats = cheats;
  *outSeed = spawn_pattern(out, startZ, ctx);
}

static void create_tutorial_segment(WorldSegment *out, float startZ,
                                    uint32_t seed, uint32_t *outSeed) {
  GameEntity train;
  int i;

  memset(out, 0, sizeof(*out));
  out->id = create_segment_id();
  out->startZ = startZ;
  out->length = GAME_CONFIG.segmentLength;
  out->entityCount = 0;

  push_entity(out, make_obstacle((Lane)-1, startZ - 8.0f,
                                 OBSTACLE_LOW_BARRIER));
  push_entity(out, make_obstacle((Lane)1, startZ - 8.0f,
                                 OBSTACLE_LOW_BARRIER));
  add_coin_line(out, (Lane)0, startZ - 6.0f, 6, 1.2f, true);

  push_entity(out, make_obstacle((Lane)0, startZ - 16.0f, OBSTACLE_OVERHEAD));
  add_coin_line(out, (Lane)-1, startZ - 15.0f, 4, 0.9f, false);
  add_coin_line(out, (Lane)1, startZ - 15.0f, 4, 0.9f, false);

  add_coin_weave(out, (Lane)0, startZ - 20.0f, 6, 1.1f);

  train = make_train((Lane)1, startZ - 28.0f, 0.0f, 2);
  push_entity(out, train);
  for (i = 0; i < 5; i++) {
    float z = train.positionZ - 1.8f - (float)i * 1.5f;
    push_entity(out, make_coin((Lane)1, z, GAME_CONFIG.trainRoofY + 1.05f));
  }
  add_coin_line(out, (Lane)0, startZ - 30.0f, 7, 1.1f, false);
  add_coin_line(out, (Lane)-1, startZ - 34.0f, 5, 1.15f, true);

  *outSeed = (seed + 1337u);
}

bool has_passable_route(const GameEntity *entities, int count) {
  /* Sparse rows keyed by rounded Z; lanes bitmask bit0=lane-1 ... */
  typedef struct {
    int key;
    int lanes;
    bool used;
  } RowBlock;
  RowBlock rows[256];
  int rowCount = 0;
  int i;
  int r;

  memset(rows, 0, sizeof(rows));

  for (i = 0; i < count; i++) {
    int key;
    int bit;
    int found = -1;
    if (entities[i].kind != ENTITY_OBSTACLE &&
        entities[i].kind != ENTITY_TRAIN) {
      continue;
    }
    if (entities[i].kind == ENTITY_OBSTACLE &&
        entities[i].variant != OBSTACLE_BARRIER &&
        entities[i].variant != OBSTACLE_CRATE) {
      continue;
    }
    key = (int)floor((double)entities[i].positionZ + 0.5);
    bit = 1 << (entities[i].lane + 1);
    for (r = 0; r < rowCount; r++) {
      if (rows[r].key == key) {
        found = r;
        break;
      }
    }
    if (found < 0) {
      if (rowCount >= 256) {
        continue;
      }
      rows[rowCount].key = key;
      rows[rowCount].lanes = bit;
      rows[rowCount].used = true;
      rowCount++;
    } else {
      rows[found].lanes |= bit;
    }
  }

  for (r = 0; r < rowCount; r++) {
    int bits = rows[r].lanes;
    int n = 0;
    if (bits & 1) {
      n++;
    }
    if (bits & 2) {
      n++;
    }
    if (bits & 4) {
      n++;
    }
    if (n >= 3) {
      return false;
    }
  }
  return true;
}

void spawn_world_objects(GameState *state) {
  uint32_t seed;
  float nextSegmentZ;
  float difficulty;
  float playerZ;
  float targetZ;
  int initialSegmentCount;

  if (state->status.type != STATUS_RUNNING &&
      state->status.type != STATUS_READY) {
    return;
  }

  seed = state->randomSeed;
  nextSegmentZ = state->world.nextSegmentZ;
  difficulty = difficulty_from_state(state);
  playerZ = state->player.position.z;
  targetZ = playerZ - GAME_CONFIG.spawnDistance;
  initialSegmentCount = state->world.segmentCount;

  while (nextSegmentZ > targetZ) {
    WorldSegment created;
    uint32_t newSeed;
    bool isFirstStretch =
        state->world.segmentCount == 0 && state->distance < 5.0f;

    if (state->world.segmentCount >= MAX_SEGMENTS) {
      break;
    }

    if (isFirstStretch) {
      create_tutorial_segment(&created, nextSegmentZ, seed, &newSeed);
    } else {
      create_segment(&created, nextSegmentZ, seed, difficulty, state->cheats,
                     &newSeed);
    }
    seed = newSeed;

    if (!isFirstStretch &&
        !has_passable_route(created.entities, created.entityCount)) {
      float retryDiff = difficulty - 0.2f;
      if (retryDiff < 0.0f) {
        retryDiff = 0.0f;
      }
      create_segment(&created, nextSegmentZ, seed, retryDiff, state->cheats,
                     &newSeed);
      seed = newSeed;
    }

    state->world.segments[state->world.segmentCount++] = created;
    nextSegmentZ -= GAME_CONFIG.segmentLength;
  }

  if (state->status.type == STATUS_READY &&
      state->world.segmentCount == 0) {
    WorldSegment tutorial;
    uint32_t newSeed;
    create_tutorial_segment(&tutorial, nextSegmentZ, seed, &newSeed);
    seed = newSeed;
    if (state->world.segmentCount < MAX_SEGMENTS) {
      state->world.segments[state->world.segmentCount++] = tutorial;
    }
    nextSegmentZ -= GAME_CONFIG.segmentLength;
    while (state->world.segmentCount < 5 &&
           state->world.segmentCount < MAX_SEGMENTS) {
      WorldSegment created;
      create_segment(&created, nextSegmentZ, seed, 0.0f, state->cheats,
                     &newSeed);
      seed = newSeed;
      state->world.segments[state->world.segmentCount++] = created;
      nextSegmentZ -= GAME_CONFIG.segmentLength;
    }
  }

  if (state->world.segmentCount == initialSegmentCount) {
    return;
  }

  state->randomSeed = seed;
  state->world.nextSegmentZ = nextSegmentZ;
}
