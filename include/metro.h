#ifndef METRO_H
#define METRO_H

/*
 * Metro Rush — pure C99 game logic (no platform/render).
 * Mutable GameState updated in place. Windows 98–friendly: C99 only,
 * stdlib / math / string / stdio / stdint / stdbool.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Limits                                                                     */
/* -------------------------------------------------------------------------- */

#define MAX_ENTITIES              512
#define MAX_ENTITIES_PER_SEGMENT  256
#define MAX_LOOSE_ENTITIES        64
#define MAX_SEGMENTS              32
#define MAX_EFFECTS               8
#define MAX_EVENTS                32
#define MAX_NEAR_MISS             64
#define MAX_COMMANDS              32
#define MAX_FLATTEN_ENTITIES      1024

/* -------------------------------------------------------------------------- */
/* Config (matches GAME_CONFIG in game-config.ts exactly)                     */
/* -------------------------------------------------------------------------- */

typedef struct GameConfig {
  float laneWidth;
  float laneChangeSpeed;
  float initialSpeed;
  float maximumSpeed;
  float speedAcceleration;
  float gravity;
  float jumpVelocity;
  float jumpClearanceY;
  float slideDurationSeconds;
  float coyoteSeconds;
  float inputBufferSeconds;
  float playerHeight;
  float playerSlideHeight;
  float playerWidth;
  float playerDepth;
  float groundY;
  float trainRoofY;
  float trainBodyHeight;
  float trainMountAssistY;
  int   boardChargesPerRun;
  float boardRecoverySeconds;
  float boardStunSeconds;
  float hitStopSeconds;
  float deathOverlayDelaySeconds;
  float segmentLength;
  float spawnDistance;
  float despawnDistance;
  float coinValue;
  float scorePerMeter;
  float coinStreakTimeoutSeconds;
  float magnetRadius;
  float magnetPullSpeed;
  float magnetDurationSeconds;
  float multiplierDurationSeconds;
  float invincibleDurationSeconds;
  float boostDurationSeconds;
  float boostSpeedBonus;
  float boostHoverHeight;
  float boostLiftSpeed;
  float maxDeltaSeconds;
  float cameraLag;
  float cameraOffsetX;
  float cameraOffsetY;
  float cameraOffsetZ;
  float cameraLookAhead;
  float cameraFov;
  float cameraBoostFov;
  float cameraMaxSpeedFov;
  float nearMissPadding;
  float nearMissScore;
  float nearMissShake;
} GameConfig;

extern const GameConfig GAME_CONFIG;

/* -------------------------------------------------------------------------- */
/* Math                                                                       */
/* -------------------------------------------------------------------------- */

typedef struct Vec3 {
  float x;
  float y;
  float z;
} Vec3;

typedef struct Bounds {
  Vec3 center;
  Vec3 halfSize;
} Bounds;

typedef struct RandomResult {
  float    value; /* [0, 1) */
  uint32_t seed;
} RandomResult;

typedef struct IntResult {
  int      value;
  uint32_t seed;
} IntResult;

typedef struct ChanceResult {
  bool     hit;
  uint32_t seed;
} ChanceResult;

Vec3  vec3(float x, float y, float z);
Vec3  add_vec3(Vec3 a, Vec3 b);
Vec3  scale_vec3(Vec3 v, float s);
Vec3  lerp_vec3(Vec3 a, Vec3 b, float t);
float distance_xz(Vec3 a, Vec3 b);

float clamp_f(float value, float min_v, float max_v);
float lerp_f(float a, float b, float t);
float approach_f(float current, float target, float max_delta);
float round_to(float value, int decimals);

bool    bounds_intersects(Bounds a, Bounds b);
Bounds  create_bounds(Vec3 center, float width, float height, float depth);

/* Mulberry32 — matches TS nextRandom / Math.imul uint32 semantics */
RandomResult next_random(uint32_t seed);
IntResult    next_int(uint32_t seed, int min_inclusive, int max_exclusive);
IntResult    pick_index(uint32_t seed, int length);
ChanceResult chance(uint32_t seed, float probability);

/* -------------------------------------------------------------------------- */
/* Domain enums / types                                                       */
/* -------------------------------------------------------------------------- */

typedef int8_t Lane; /* -1, 0, 1 */

typedef enum EntityKind {
  ENTITY_OBSTACLE = 0,
  ENTITY_COIN,
  ENTITY_TRAIN,
  ENTITY_POWER_UP,
  ENTITY_DECORATION
} EntityKind;

typedef enum ObstacleVariant {
  OBSTACLE_BARRIER = 0,
  OBSTACLE_LOW_BARRIER,
  OBSTACLE_OVERHEAD,
  OBSTACLE_CRATE
} ObstacleVariant;

typedef enum PowerUpType {
  POWERUP_MAGNET = 0,
  POWERUP_MULTIPLIER,
  POWERUP_INVINCIBLE,
  POWERUP_BOOST
} PowerUpType;

typedef enum DecorationStyle {
  DECOR_LAMP = 0,
  DECOR_SIGN,
  DECOR_PILLAR
} DecorationStyle;

typedef enum CollisionReason {
  COLLISION_OBSTACLE = 0,
  COLLISION_BARRIER,
  COLLISION_TRAIN,
  COLLISION_LOW_BARRIER,
  COLLISION_OVERHEAD
} CollisionReason;

typedef enum GameStatusType {
  STATUS_READY = 0,
  STATUS_RUNNING,
  STATUS_PAUSED,
  STATUS_GAME_OVER
} GameStatusType;

typedef enum MovementType {
  MOVE_RUNNING = 0,
  MOVE_JUMPING,
  MOVE_FALLING,
  MOVE_SLIDING,
  MOVE_STUNNED
} MovementType;

typedef enum CheatId {
  CHEAT_IMMORTAL = 0,
  CHEAT_MAX_SPEED,
  CHEAT_FLY
} CheatId;

typedef enum CommandType {
  CMD_MOVE_LEFT = 0,
  CMD_MOVE_RIGHT,
  CMD_JUMP,
  CMD_SLIDE,
  CMD_STAND,
  CMD_PAUSE,
  CMD_RESTART,
  CMD_TOGGLE_MUTE,
  CMD_TOGGLE_CHEAT
} CommandType;

typedef enum GameEventType {
  EVENT_COIN_COLLECTED = 0,
  EVENT_POWER_UP_COLLECTED,
  EVENT_PLAYER_JUMPED,
  EVENT_PLAYER_SLID,
  EVENT_PLAYER_LANDED,
  EVENT_LANE_CHANGED,
  EVENT_BOARD_SAVED,
  EVENT_NEAR_MISS,
  EVENT_COLLISION,
  EVENT_GAME_STARTED,
  EVENT_GAME_OVER,
  EVENT_GAME_RESTARTED
} GameEventType;

typedef struct GameStatus {
  GameStatusType type;
  CollisionReason reason; /* valid when type == STATUS_GAME_OVER */
} GameStatus;

typedef struct PlayerMovement {
  MovementType type;
  float remainingSeconds; /* sliding / stunned */
} PlayerMovement;

typedef struct PlayerState {
  Lane           lane;
  Vec3           position;
  float          velocityY;
  PlayerMovement movement;
  float          coyoteSeconds;
  float          jumpBufferSeconds;
  float          slideBufferSeconds;
  float          boardRecoverySeconds;
  uint32_t       rideTrainId; /* 0 = none */
  bool           hasRideTrain;
  bool           forwardBlocked;
} PlayerState;

typedef struct GameEntity {
  uint32_t   id;
  EntityKind kind;
  Lane       lane;
  float      positionZ;

  /* obstacle */
  ObstacleVariant variant;
  float height;
  float width;
  float depth;
  float baseY;

  /* coin */
  float positionX;
  bool  collected;

  /* train */
  float length;
  float speed;
  int   cars;

  /* power-up */
  PowerUpType powerUpType;

  /* decoration */
  DecorationStyle style;
} GameEntity;

typedef struct WorldSegment {
  uint32_t   id;
  float      startZ;
  float      length;
  int        entityCount;
  GameEntity entities[MAX_ENTITIES_PER_SEGMENT];
} WorldSegment;

typedef struct WorldState {
  int          segmentCount;
  WorldSegment segments[MAX_SEGMENTS];
  float        nextSegmentZ;
  float        distanceTraveled;
} WorldState;

typedef struct ActiveEffect {
  PowerUpType type;
  float       remainingSeconds;
} ActiveEffect;

typedef struct CheatFlags {
  bool immortal;
  bool maxSpeed;
  bool fly;
} CheatFlags;

typedef struct GameEvent {
  GameEventType   type;
  uint32_t        entityId;
  int             streak;
  float           points;
  PowerUpType     powerUpType;
  int             direction; /* -1 or 1 for laneChanged */
  CollisionReason reason;
} GameEvent;

typedef struct GameCommand {
  CommandType type;
  CheatId     cheatId;
} GameCommand;

typedef struct InputState {
  int         count;
  GameCommand commands[MAX_COMMANDS];
} InputState;

typedef struct GameState {
  GameStatus   status;
  float        elapsedSeconds;
  float        distance;
  float        score;
  int          coins;
  int          coinStreak;
  float        streakTimer;
  float        speed;
  float        highScore;
  int          totalCoins;
  int          boardCharges;
  float        hitStopSeconds;
  float        deathAnimSeconds;
  float        cameraShake;
  int          nearMissCount;
  uint32_t     nearMissIds[MAX_NEAR_MISS];
  PlayerState  player;
  WorldState   world;
  int          entityCount;
  GameEntity   entities[MAX_LOOSE_ENTITIES];
  int          effectCount;
  ActiveEffect effects[MAX_EFFECTS];
  int          eventCount;
  GameEvent    events[MAX_EVENTS];
  uint32_t     randomSeed;
  bool         muted;
  CheatFlags   cheats;
} GameState;

typedef struct CreateGameStateOptions {
  float      highScore;
  int        totalCoins;
  bool       muted;
  uint32_t   randomSeed;
  CheatFlags cheats;
  bool       useCustomSeed;
  bool       useCustomCheats;
} CreateGameStateOptions;

/* -------------------------------------------------------------------------- */
/* Identifiers                                                                */
/* -------------------------------------------------------------------------- */

void     reset_identifiers(void);
uint32_t create_entity_id(void);
uint32_t create_segment_id(void);

/* -------------------------------------------------------------------------- */
/* Lane helpers                                                               */
/* -------------------------------------------------------------------------- */

Lane clamp_lane(int lane);
Lane shift_lane(Lane lane, int delta);

/* -------------------------------------------------------------------------- */
/* Player helpers                                                             */
/* -------------------------------------------------------------------------- */

PlayerState create_player_state(void);
bool        player_is_grounded(const PlayerState *player);
bool        player_can_jump(const PlayerState *player);
bool        player_is_sliding(const PlayerState *player);
bool        player_is_board_recovering(const PlayerState *player);

/* -------------------------------------------------------------------------- */
/* Power-ups / cheats                                                         */
/* -------------------------------------------------------------------------- */

extern const CheatFlags DEFAULT_CHEATS;

bool  has_effect(const ActiveEffect *effects, int count, PowerUpType type);
float get_multiplier(const ActiveEffect *effects, int count, const CheatFlags *cheats);
bool  is_magnet_active(const ActiveEffect *effects, int count, const CheatFlags *cheats);
CheatFlags toggle_cheat_flag(CheatFlags cheats, CheatId id);
bool  is_cheat_active(CheatFlags cheats);

/* -------------------------------------------------------------------------- */
/* Game state                                                                 */
/* -------------------------------------------------------------------------- */

void create_game_state(GameState *out, const CreateGameStateOptions *options);
void clear_events(GameState *state);
bool append_event(GameState *state, GameEvent event);
InputState empty_input(void);

/* -------------------------------------------------------------------------- */
/* World / entities                                                           */
/* -------------------------------------------------------------------------- */

void create_empty_world(WorldState *out);

/* Flatten segment + loose entities into out[]; returns count (capped). */
int  flatten_entities(const GameState *state, GameEntity *out, int max_out);

void update_world(GameState *state, float dt);
void update_entities(GameState *state, float dt);
void remove_expired_entities(GameState *state);
bool is_behind_despawn_line(float positionZ, float playerZ, float despawnDistance);

/* -------------------------------------------------------------------------- */
/* Player / train / input                                                     */
/* -------------------------------------------------------------------------- */

void update_player(GameState *state, float dt);
void apply_input(GameState *state, const InputState *input);

bool          is_above_train_roof(float playerY);
bool          player_overlaps_train_footprint(const PlayerState *player,
                                              const GameEntity *train,
                                              float lateralSlack);
const GameEntity *find_ride_train(const PlayerState *player,
                                  const GameEntity *entities,
                                  int count);
float         find_train_support_y(const PlayerState *player,
                                   const GameEntity *entities,
                                   int count);
bool          can_land_on_train(const PlayerState *player,
                                const GameEntity *train,
                                float nextY,
                                float velocityY);
void          apply_train_ride_motion(PlayerState *player,
                                      const GameEntity *train,
                                      float dt);
bool          train_blocks_lane_change(const PlayerState *player, Lane targetLane,
                                       const GameEntity *entities, int count);

/* -------------------------------------------------------------------------- */
/* Collision / collect / attract / effects                                    */
/* -------------------------------------------------------------------------- */

Bounds get_player_bounds(const PlayerState *player);
/* Returns false if entity has no collision bounds. */
bool   get_entity_bounds(const GameEntity *entity, Bounds *out);
bool   collision_reason_for_entity(const GameEntity *entity, CollisionReason *out);
bool   can_avoid_by_jump(const GameEntity *entity);
bool   can_avoid_by_slide(const GameEntity *entity);
bool   entities_collide(const PlayerState *player, const GameEntity *entity);

void detect_collisions(GameState *state);
void detect_near_misses(GameState *state);
void collect_items(GameState *state);
void attract_coins(GameState *state, float dt);
void update_effects(GameState *state, float dt);

/* -------------------------------------------------------------------------- */
/* Spawn                                                                      */
/* -------------------------------------------------------------------------- */

bool has_passable_route(const GameEntity *entities, int count);
void spawn_world_objects(GameState *state);

/* -------------------------------------------------------------------------- */
/* Score / difficulty                                                         */
/* -------------------------------------------------------------------------- */

void update_score(GameState *state, float dt);
void update_difficulty(GameState *state, float dt);

/* -------------------------------------------------------------------------- */
/* Orchestrator                                                               */
/* -------------------------------------------------------------------------- */

void update_game(GameState *state, const InputState *input, float dt);

#ifdef __cplusplus
}
#endif

#endif /* METRO_H */
