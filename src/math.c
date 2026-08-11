#include "metro.h"
#include <math.h>

const GameConfig GAME_CONFIG = {
  /* laneWidth */              2.6f,
  /* laneChangeSpeed */        22.0f,
  /* initialSpeed */           17.0f,
  /* maximumSpeed */           36.0f,
  /* speedAcceleration */      0.48f,
  /* gravity */                38.0f,
  /* jumpVelocity */           16.2f,
  /* jumpClearanceY */         0.75f,
  /* slideDurationSeconds */   0.7f,
  /* coyoteSeconds */          0.1f,
  /* inputBufferSeconds */     0.14f,
  /* playerHeight */           1.55f,
  /* playerSlideHeight */      0.72f,
  /* playerWidth */            0.65f,
  /* playerDepth */            0.65f,
  /* groundY */                0.0f,
  /* trainRoofY */             2.2f,
  /* trainBodyHeight */        2.2f,
  /* trainMountAssistY */      0.95f,
  /* boardChargesPerRun */     1,
  /* boardRecoverySeconds */   2.4f,
  /* boardStunSeconds */       0.45f,
  /* hitStopSeconds */         0.12f,
  /* deathOverlayDelaySeconds */ 0.4f,
  /* segmentLength */          40.0f,
  /* spawnDistance */          200.0f,
  /* despawnDistance */        45.0f,
  /* coinValue */              10.0f,
  /* scorePerMeter */          1.0f,
  /* coinStreakTimeoutSeconds */ 1.4f,
  /* magnetRadius */           10.0f,
  /* magnetPullSpeed */        38.0f,
  /* magnetDurationSeconds */  8.0f,
  /* multiplierDurationSeconds */ 10.0f,
  /* invincibleDurationSeconds */ 6.0f,
  /* boostDurationSeconds */   5.0f,
  /* boostSpeedBonus */        11.0f,
  /* boostHoverHeight */       5.2f,
  /* boostLiftSpeed */         14.0f,
  /* maxDeltaSeconds */        (1.0f / 20.0f),
  /* cameraLag */              10.0f,
  /* cameraOffset */           0.0f, 9.0f, 12.5f,
  /* cameraLookAhead */        9.5f,
  /* cameraFov */              50.0f,
  /* cameraBoostFov */         60.0f,
  /* cameraMaxSpeedFov */      54.0f,
  /* nearMissPadding */        1.25f,
  /* nearMissScore */          75.0f,
  /* nearMissShake */          0.34f,
};

Vec3 vec3(float x, float y, float z) {
  Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

Vec3 add_vec3(Vec3 a, Vec3 b) {
  return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 scale_vec3(Vec3 v, float s) {
  return vec3(v.x * s, v.y * s, v.z * s);
}

Vec3 lerp_vec3(Vec3 a, Vec3 b, float t) {
  return vec3(
      a.x + (b.x - a.x) * t,
      a.y + (b.y - a.y) * t,
      a.z + (b.z - a.z) * t);
}

float distance_xz(Vec3 a, Vec3 b) {
  float dx = a.x - b.x;
  float dz = a.z - b.z;
  return (float)sqrt((double)(dx * dx + dz * dz));
}

float clamp_f(float value, float min_v, float max_v) {
  if (value < min_v) {
    return min_v;
  }
  if (value > max_v) {
    return max_v;
  }
  return value;
}

float lerp_f(float a, float b, float t) {
  return a + (b - a) * t;
}

float approach_f(float current, float target, float max_delta) {
  float delta = target - current;
  if (delta <= max_delta && delta >= -max_delta) {
    return target;
  }
  return current + (delta > 0.0f ? max_delta : -max_delta);
}

float round_to(float value, int decimals) {
  float factor = 1.0f;
  int i;
  for (i = 0; i < decimals; i++) {
    factor *= 10.0f;
  }
  return (float)(floor((double)(value * factor) + 0.5) / (double)factor);
}

bool bounds_intersects(Bounds a, Bounds b) {
  return fabs(a.center.x - b.center.x) <= a.halfSize.x + b.halfSize.x &&
         fabs(a.center.y - b.center.y) <= a.halfSize.y + b.halfSize.y &&
         fabs(a.center.z - b.center.z) <= a.halfSize.z + b.halfSize.z;
}

Bounds create_bounds(Vec3 center, float width, float height, float depth) {
  Bounds b;
  b.center = center;
  b.halfSize.x = width * 0.5f;
  b.halfSize.y = height * 0.5f;
  b.halfSize.z = depth * 0.5f;
  return b;
}

/* Mulberry32 matching TS:
 *   let t = (seed + 0x6d2b79f5) | 0;
 *   t = Math.imul(t ^ (t >>> 15), t | 1);
 *   t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
 *   value = ((t ^ (t >>> 14)) >>> 0) / 4294967296;
 *   seed = t >>> 0;
 * uint32_t multiply wraps identically to Math.imul low 32 bits.
 * Value computed in double (JS Number) so next_int/chance stay bit-stable.
 */
static void mulberry32_step(uint32_t seed, uint32_t *out_seed, double *out_value) {
  uint32_t t = seed + 0x6d2b79f5u;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  *out_seed = t;
  *out_value = (double)(t ^ (t >> 14)) * (1.0 / 4294967296.0);
}

RandomResult next_random(uint32_t seed) {
  RandomResult r;
  double value;
  mulberry32_step(seed, &r.seed, &value);
  r.value = (float)value;
  return r;
}

IntResult next_int(uint32_t seed, int min_inclusive, int max_exclusive) {
  IntResult r;
  double value;
  int span = max_exclusive - min_inclusive;
  mulberry32_step(seed, &r.seed, &value);
  r.value = min_inclusive + (int)floor(value * (double)span);
  return r;
}

IntResult pick_index(uint32_t seed, int length) {
  return next_int(seed, 0, length);
}

ChanceResult chance(uint32_t seed, float probability) {
  ChanceResult r;
  double value;
  mulberry32_step(seed, &r.seed, &value);
  r.hit = value < (double)probability;
  return r;
}
