#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SKY_TOP 0xFF2B9FE8u
#define SKY_HORIZON 0xFFB8E8FFu
#define FOG_COLOR 0xFF81D4FAu
#define COL_SUN 0xFFFFF3B0u
#define COL_SUN_GLOW 0xFFFFE082u
#define COL_CLOUD 0xFFFFFFF5u
#define COL_PLAYER 0xFFFF1E4Du
#define COL_PLAYER_ACCENT 0xFF2563EBu
#define COL_PLAYER_SKIN 0xFFFFC29Au
#define COL_PLAYER_SHOE 0xFFFBBF24u
#define COL_PLAYER_HAIR 0xFF292524u
#define COL_PLAYER_GOGGLE 0xFF67E8F9u
#define COL_PLAYER_PACK 0xFF0284C7u
#define COL_PLAYER_SCARF 0xFFFF8A00u
#define COL_PLAYER_STRIPE 0xFFFFFFFFu
#define COL_BOARD 0xFF06B6D4u
#define COL_BARRIER 0xFFFFCC00u
#define COL_BARRIER_ACCENT 0xFF111827u
#define COL_OVERHEAD 0xFFA855F7u
#define COL_OVERHEAD_POST 0xFF334155u
#define COL_OVERHEAD_LIGHT 0xFFFEF3C7u
#define COL_CRATE 0xFFC2410Cu
#define COL_CRATE_STRIPE 0xFFFFCC00u
#define COL_TRAIN 0xFF22C1F0u
#define COL_TRAIN_ACCENT 0xFFFF8A3Du
#define COL_TRAIN_WINDOW 0xFFE8F7FFu
#define COL_TRAIN_WHEEL 0xFF0F172Au
#define COL_TRAIN_METAL 0xFF64748Bu
#define COL_TRAIN_LIGHT 0xFFFFF7EDu
#define COL_COIN 0xFFFFD60Au
#define COL_COIN_RIM 0xFFF59E0Bu
#define COL_COIN_CORE 0xFFFFF3A0u
#define COL_MAGNET 0xFF38BDF8u
#define COL_MULTIPLIER 0xFFFFCC00u
#define COL_INVINCIBLE 0xFFD8B4FEu
#define COL_BOOST 0xFF2DD4BFu
#define COL_LAMP 0xFF475569u
#define COL_LAMP_GLOW 0xFFFFF7EDu
#define COL_SIGN 0xFF22D3EEu
#define COL_SIGN_ALT 0xFFFF4D9Au
#define COL_PILLAR 0xFFA8A29Eu
#define COL_TRACK 0xFF6B5B4Au
#define COL_TRACK_CENTER 0xFF564737u
#define COL_BALLAST 0xFF8A7A66u
#define COL_RAIL 0xFFFFFFFFu
#define COL_SLEEPER 0xFF6B3F1Du
#define COL_PLATFORM 0xFFB0BEC8u
#define COL_PLATFORM_TOP 0xFFE8EEF3u
#define COL_PLATFORM_EDGE 0xFFFFCC00u
#define COL_WALL 0xFF90A4AEu
#define COL_WALL_TRIM 0xFFFF7A18u
#define COL_WALL_CAP 0xFFD7E0E8u
#define COL_BILLBOARD 0xFFFF4D9Au
#define COL_BILLBOARD_ALT 0xFF4ADE80u
#define COL_BILLBOARD_FRAME 0xFF1E293Bu
#define COL_AWNING 0xFFFF2D55u
#define COL_BUILDING_ACCENT 0xFFFFE566u
#define COL_BUILDING_ACCENT_COOL 0xFFA5E4FFu
#define COL_BUILDING_ACCENT_WARM 0xFFFFB4C4u
#define COL_HUD 0xFFFFFFFFu
#define COL_HUD_DIM 0xFFE2E8F0u
#define COL_HUD_OUTLINE 0xFF0F172Au
#define COL_HUD_PANEL 0xCC0F172Au
#define COL_HUD_GOLD 0xFFFFD60Au

static const uint32_t BUILDING_PALETTE[] = {
    0xFFFFC8B5u, 0xFFFF6B8Au, 0xFF5EEAD4u, 0xFFC4B5FDu, 0xFFFFE08Au,
    0xFF2DD4BFu, 0xFFFF9AAFu, 0xFF7DD3FCu, 0xFFFFF4E0u};

static const uint32_t TRAIN_BODY[] = {
    0xFFFF2D6Au, 0xFF22C55Eu, 0xFFFF7A18u, 0xFF6366F1u,
    0xFF14B8A6u, 0xFFE11D48u, 0xFF22C1F0u};
static const uint32_t TRAIN_ACCENT[] = {
    0xFFFFF1C2u, 0xFF1E3A8Au, 0xFF38BDF8u, 0xFFFF4D6Du,
    0xFFFFCC00u, 0xFFFFFFFFu, 0xFFFF8A3Du};

typedef struct CameraTransform {
  Vec3 position;
  Vec3 target;
  float fov;
} CameraTransform;

typedef struct ProjVert {
  float x, y, z; /* screen x,y and view-space depth */
  int valid;
} ProjVert;

static CameraTransform g_cam;
static int g_cam_ready;
static float *g_zbuf;
static int g_zbuf_w;
static int g_zbuf_h;
static int g_show_cheats;

/* Tiny 5x7 font: bits MSB left, 5 columns packed in low 5 bits of each row. */
static const unsigned char FONT_5X7[][7] = {
  /* 0-9 */
  {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
  {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
  {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, /* 2 */
  {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, /* 3 */
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
  {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
  {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
  {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
  /* A-Z */
  {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
  {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
  {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
  {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
  {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
  {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
  {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
  {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
  {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
  {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, /* J */
  {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
  {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
  {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
  {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
  {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
  {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
  {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
  {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, /* S */
  {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
  {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
  {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
  {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
  /* space . : / - + ( ) ! ? = */
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, /* . */
  {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}, /* : */
  {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}, /* / */
  {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, /* - */
  {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, /* + */
  {0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x04}, /* ( */
  {0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08}, /* ) */
  {0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x04}, /* ! */
  {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}, /* ? */
  {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}, /* = */
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x08}, /* , */
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08}, /* ` */
  {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F}, /* box placeholder */
};

static int font_index(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'Z') {
    return 10 + (c - 'A');
  }
  if (c >= 'a' && c <= 'z') {
    return 10 + (c - 'a');
  }
  switch (c) {
    case ' ':
      return 36;
    case '.':
      return 37;
    case ':':
      return 38;
    case '/':
      return 39;
    case '-':
      return 40;
    case '+':
      return 41;
    case '(':
      return 42;
    case ')':
      return 43;
    case '!':
      return 44;
    case '?':
      return 45;
    case '=':
      return 46;
    case ',':
      return 47;
    case '`':
      return 48;
    default:
      return 49;
  }
}

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

static float minf(float a, float b) {
  return a < b ? a : b;
}

static float maxf(float a, float b) {
  return a > b ? a : b;
}

static float absf(float v) {
  return v < 0.0f ? -v : v;
}

static Vec3 v3_sub(Vec3 a, Vec3 b) {
  return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 v3_cross(Vec3 a, Vec3 b) {
  return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

static float v3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 v3_norm(Vec3 v) {
  float len = (float)sqrt((double)(v.x * v.x + v.y * v.y + v.z * v.z));
  if (len < 1e-8f) {
    return vec3(0.0f, 1.0f, 0.0f);
  }
  return scale_vec3(v, 1.0f / len);
}

static uint32_t shade_color(uint32_t argb, float intensity) {
  int r = (int)((argb >> 16) & 0xFFu);
  int g = (int)((argb >> 8) & 0xFFu);
  int b = (int)(argb & 0xFFu);
  float t = clampf(intensity, 0.18f, 1.15f);
  r = (int)(r * t);
  g = (int)(g * t);
  b = (int)(b * t);
  if (r > 255) {
    r = 255;
  }
  if (g > 255) {
    g = 255;
  }
  if (b > 255) {
    b = 255;
  }
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint32_t lerp_color(uint32_t a, uint32_t b, float t) {
  int ar = (int)((a >> 16) & 0xFFu);
  int ag = (int)((a >> 8) & 0xFFu);
  int ab = (int)(a & 0xFFu);
  int br = (int)((b >> 16) & 0xFFu);
  int bg = (int)((b >> 8) & 0xFFu);
  int bb = (int)(b & 0xFFu);
  float u = clampf(t, 0.0f, 1.0f);
  int r = (int)(ar + (br - ar) * u);
  int g = (int)(ag + (bg - ag) * u);
  int bl = (int)(ab + (bb - ab) * u);
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static uint32_t apply_fog(uint32_t color, float depth) {
  float t = (depth - 28.0f) / 110.0f;
  return lerp_color(color, FOG_COLOR, clampf(t, 0.0f, 0.82f));
}

static int hash_u32(uint32_t id) {
  id ^= id >> 16;
  id *= 0x7feb352du;
  id ^= id >> 15;
  id *= 0x846ca68bu;
  id ^= id >> 16;
  return (int)(id & 0x7fffffffu);
}

static CameraTransform create_initial_camera(const GameState *state) {
  CameraTransform cam;
  Vec3 pos = state->player.position;
  cam.position = vec3(pos.x * 0.45f + GAME_CONFIG.cameraOffsetX,
                      pos.y + GAME_CONFIG.cameraOffsetY,
                      pos.z + GAME_CONFIG.cameraOffsetZ);
  cam.target =
      vec3(pos.x * 0.3f, pos.y + 1.15f, pos.z - GAME_CONFIG.cameraLookAhead);
  cam.fov = GAME_CONFIG.cameraFov;
  return cam;
}

static CameraTransform calculate_camera(const GameState *state,
                                        const CameraTransform *previous,
                                        float deltaSeconds) {
  CameraTransform desired;
  Vec3 pos = state->player.position;
  bool boost = has_effect(state->effects, state->effectCount, POWERUP_BOOST);
  float jumpLift = pos.y * 0.28f;
  float speedDenom;
  float speedT;
  if (jumpLift > 1.4f) {
    jumpLift = 1.4f;
  }
  speedDenom = GAME_CONFIG.maximumSpeed - GAME_CONFIG.initialSpeed;
  if (speedDenom < 1.0f) {
    speedDenom = 1.0f;
  }
  speedT = (state->speed - GAME_CONFIG.initialSpeed) / speedDenom;
  float speedFov =
      GAME_CONFIG.cameraFov +
      (GAME_CONFIG.cameraMaxSpeedFov - GAME_CONFIG.cameraFov) * speedT;
  float lanePunch =
      ((float)state->player.lane * GAME_CONFIG.laneWidth - pos.x) * 0.12f;
  float shake =
      state->cameraShake * (float)sin((double)state->elapsedSeconds * 48.0) *
      0.35f;
  float shakeY =
      state->cameraShake * (float)cos((double)state->elapsedSeconds * 37.0) *
      0.2f;
  bool gameOver = state->status.type == STATUS_GAME_OVER;
  float lag;
  float t;

  desired.position = vec3(
      pos.x * 0.55f + GAME_CONFIG.cameraOffsetX + lanePunch + shake,
      pos.y * 0.35f + GAME_CONFIG.cameraOffsetY + jumpLift + shakeY +
          (boost ? 1.8f : 0.0f) + (gameOver ? 1.5f : 0.0f),
      pos.z + GAME_CONFIG.cameraOffsetZ - (boost ? 2.2f : 0.0f) +
          (gameOver ? 2.5f : 0.0f));
  desired.target =
      vec3(pos.x * 0.45f + shake * 0.4f,
           pos.y * 0.4f + 1.25f + (boost ? 1.0f : 0.0f),
           pos.z - GAME_CONFIG.cameraLookAhead - (boost ? 5.0f : 0.0f));
  desired.fov = boost ? GAME_CONFIG.cameraBoostFov : speedFov;

  lag = gameOver ? 4.0f : GAME_CONFIG.cameraLag;
  t = 1.0f - (float)exp((double)(-lag * deltaSeconds));
  desired.position = lerp_vec3(previous->position, desired.position, t);
  desired.target = lerp_vec3(previous->target, desired.target, t);
  desired.fov = previous->fov + (desired.fov - previous->fov) * t;
  return desired;
}

typedef struct CamBasis {
  Vec3 forward;
  Vec3 right;
  Vec3 up;
  float focal;
  float aspect;
} CamBasis;

static CamBasis make_basis(const CameraTransform *cam, int w, int h) {
  CamBasis b;
  Vec3 worldUp = vec3(0.0f, 1.0f, 0.0f);
  b.forward = v3_norm(v3_sub(cam->target, cam->position));
  b.right = v3_norm(v3_cross(b.forward, worldUp));
  /* If looking nearly straight up/down, fall back. */
  if (absf(v3_dot(b.forward, worldUp)) > 0.999f) {
    b.right = vec3(1.0f, 0.0f, 0.0f);
  }
  b.up = v3_cross(b.right, b.forward);
  b.focal = 1.0f / tanf(cam->fov * (float)M_PI / 360.0f);
  b.aspect = (h > 0) ? ((float)w / (float)h) : 1.0f;
  return b;
}

#define NEAR_CLIP 0.2f

typedef struct ViewVert {
  float x, y, z;
} ViewVert;

static ViewVert world_to_view(Vec3 world, const CameraTransform *cam,
                             const CamBasis *b) {
  ViewVert v;
  Vec3 rel = v3_sub(world, cam->position);
  v.x = v3_dot(rel, b->right);
  v.y = v3_dot(rel, b->up);
  v.z = v3_dot(rel, b->forward);
  return v;
}

static ProjVert project_view(ViewVert v, const CamBasis *b, int w, int h) {
  ProjVert out;
  float nx, ny;
  out.valid = 0;
  out.x = 0.0f;
  out.y = 0.0f;
  out.z = v.z;
  if (v.z < NEAR_CLIP * 0.5f) {
    return out;
  }
  nx = (v.x / v.z) * b->focal / b->aspect;
  ny = (v.y / v.z) * b->focal;
  out.x = (nx * 0.5f + 0.5f) * (float)w;
  out.y = (-ny * 0.5f + 0.5f) * (float)h;
  out.valid = 1;
  return out;
}

static ViewVert lerp_view(ViewVert a, ViewVert b, float t) {
  ViewVert r;
  r.x = a.x + (b.x - a.x) * t;
  r.y = a.y + (b.y - a.y) * t;
  r.z = a.z + (b.z - a.z) * t;
  return r;
}

/* Clip convex polygon against view-space near plane. out must hold >= n+1. */
static int clip_poly_near(const ViewVert *in, int n, ViewVert *out) {
  int count = 0;
  int i;
  for (i = 0; i < n; i++) {
    ViewVert a = in[i];
    ViewVert b = in[(i + 1) % n];
    int aIn = a.z >= NEAR_CLIP;
    int bIn = b.z >= NEAR_CLIP;
    if (aIn && bIn) {
      out[count++] = b;
    } else if (aIn && !bIn) {
      float t = (NEAR_CLIP - a.z) / (b.z - a.z);
      out[count++] = lerp_view(a, b, t);
    } else if (!aIn && bIn) {
      float t = (NEAR_CLIP - a.z) / (b.z - a.z);
      out[count++] = lerp_view(a, b, t);
      out[count++] = b;
    }
  }
  return count;
}

static void ensure_zbuf(int w, int h) {
  if (g_zbuf != NULL && g_zbuf_w == w && g_zbuf_h == h) {
    return;
  }
  if (g_zbuf != NULL) {
    free(g_zbuf);
  }
  g_zbuf = (float *)malloc((size_t)w * (size_t)h * sizeof(float));
  g_zbuf_w = w;
  g_zbuf_h = h;
}

static void clear_framebuffer(Framebuffer *fb) {
  int x, y;
  int n = fb->width * fb->height;
  int i;
  for (y = 0; y < fb->height; y++) {
    float t = (float)y / (float)(fb->height > 1 ? fb->height - 1 : 1);
    /* Soft Subway-style sky: deep blue above, bright horizon below. */
    float curve = t * t * (3.0f - 2.0f * t);
    uint32_t row = lerp_color(SKY_TOP, SKY_HORIZON, curve);
    uint32_t *line = fb->pixels + y * fb->width;
    for (x = 0; x < fb->width; x++) {
      line[x] = row;
    }
  }
  if (g_zbuf != NULL) {
    for (i = 0; i < n; i++) {
      g_zbuf[i] = 1e30f;
    }
  }
}

static void put_pixel(Framebuffer *fb, int x, int y, float z, uint32_t color) {
  int idx;
  if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  idx = y * fb->width + x;
  if (z >= g_zbuf[idx]) {
    return;
  }
  g_zbuf[idx] = z;
  fb->pixels[idx] = color;
}

static void put_pixel_hud(Framebuffer *fb, int x, int y, uint32_t color) {
  if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) {
    return;
  }
  fb->pixels[y * fb->width + x] = color;
}

static void fill_triangle(Framebuffer *fb, ProjVert a, ProjVert b, ProjVert c,
                          uint32_t color) {
  float minxf, maxxf, minyf, maxyf;
  int minx, maxx, miny, maxy;
  int x, y;
  float area;
  float invA;

  if (!a.valid || !b.valid || !c.valid) {
    return;
  }

  area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  if (absf(area) < 1e-4f) {
    return;
  }
  invA = 1.0f / area;

  minxf = minf(a.x, minf(b.x, c.x));
  maxxf = maxf(a.x, maxf(b.x, c.x));
  minyf = minf(a.y, minf(b.y, c.y));
  maxyf = maxf(a.y, maxf(b.y, c.y));

  minx = (int)floorf(minxf);
  maxx = (int)ceilf(maxxf);
  miny = (int)floorf(minyf);
  maxy = (int)ceilf(maxyf);
  if (minx < 0) {
    minx = 0;
  }
  if (miny < 0) {
    miny = 0;
  }
  if (maxx >= fb->width) {
    maxx = fb->width - 1;
  }
  if (maxy >= fb->height) {
    maxy = fb->height - 1;
  }

  for (y = miny; y <= maxy; y++) {
    for (x = minx; x <= maxx; x++) {
      float px = (float)x + 0.5f;
      float py = (float)y + 0.5f;
      float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * invA;
      float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * invA;
      float w2 = 1.0f - w0 - w1;
      float z;
      if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
        continue;
      }
      z = w0 * a.z + w1 * b.z + w2 * c.z;
      put_pixel(fb, x, y, z, color);
    }
  }
}

static void draw_box(Framebuffer *fb, const CameraTransform *cam,
                     const CamBasis *basis, Vec3 center, Vec3 half,
                     uint32_t color) {
  static const int faces[6][4] = {
      {0, 1, 3, 2}, /* -Z */
      {4, 6, 7, 5}, /* +Z */
      {0, 2, 6, 4}, /* -X */
      {1, 5, 7, 3}, /* +X */
      {0, 4, 5, 1}, /* -Y */
      {2, 3, 7, 6}, /* +Y */
  };
  static const Vec3 normals[6] = {
      {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0},
  };
  Vec3 corners[8];
  ViewVert view[8];
  Vec3 light = v3_norm(vec3(0.35f, 0.85f, 0.25f));
  int i, f;

  corners[0] = vec3(center.x - half.x, center.y - half.y, center.z - half.z);
  corners[1] = vec3(center.x + half.x, center.y - half.y, center.z - half.z);
  corners[2] = vec3(center.x - half.x, center.y + half.y, center.z - half.z);
  corners[3] = vec3(center.x + half.x, center.y + half.y, center.z - half.z);
  corners[4] = vec3(center.x - half.x, center.y - half.y, center.z + half.z);
  corners[5] = vec3(center.x + half.x, center.y - half.y, center.z + half.z);
  corners[6] = vec3(center.x - half.x, center.y + half.y, center.z + half.z);
  corners[7] = vec3(center.x + half.x, center.y + half.y, center.z + half.z);

  for (i = 0; i < 8; i++) {
    view[i] = world_to_view(corners[i], cam, basis);
  }

  for (f = 0; f < 6; f++) {
    Vec3 n = normals[f];
    Vec3 faceCenter =
        vec3(center.x + n.x * half.x, center.y + n.y * half.y,
             center.z + n.z * half.z);
    Vec3 toCam = v3_sub(cam->position, faceCenter);
    float facing = v3_dot(n, toCam);
    float intensity;
    uint32_t shaded;
    ViewVert poly[4];
    ViewVert clipped[8];
    ProjVert proj[8];
    int nc;
    int t;

    if (facing <= 0.0f) {
      continue;
    }
    intensity = 0.42f + 0.72f * maxf(0.0f, v3_dot(n, light));
    shaded = shade_color(color, intensity);
    {
      Vec3 delta = v3_sub(faceCenter, cam->position);
      float depth =
          (float)sqrt((double)(delta.x * delta.x + delta.y * delta.y +
                               delta.z * delta.z));
      shaded = apply_fog(shaded, depth);
    }

    poly[0] = view[faces[f][0]];
    poly[1] = view[faces[f][1]];
    poly[2] = view[faces[f][2]];
    poly[3] = view[faces[f][3]];
    nc = clip_poly_near(poly, 4, clipped);
    if (nc < 3) {
      continue;
    }
    for (i = 0; i < nc; i++) {
      proj[i] = project_view(clipped[i], basis, fb->width, fb->height);
      if (!proj[i].valid) {
        nc = 0;
        break;
      }
    }
    if (nc < 3) {
      continue;
    }
    for (t = 1; t < nc - 1; t++) {
      fill_triangle(fb, proj[0], proj[t], proj[t + 1], shaded);
    }
  }
}

static void draw_building(Framebuffer *fb, const CameraTransform *cam,
                          const CamBasis *basis, float side, float z,
                          int seed) {
  static const float heights[] = {9.5f, 14.5f, 7.2f, 11.0f};
  static const float widths[] = {4.8f, 3.6f, 6.8f, 2.8f};
  static const float depths[] = {5.5f, 5.0f, 5.8f, 4.8f};
  int shape = ((seed % 4) + 4) % 4;
  int seedPos = (seed < 0) ? -seed : seed;
  float h = heights[shape] * (0.88f + (float)(seedPos % 3) * 0.08f);
  float w = widths[shape];
  float d = depths[shape];
  float x = side * (9.2f + (float)(seedPos % 2) * 0.45f);
  float baseY = h * 0.5f;
  uint32_t facade = BUILDING_PALETTE[seedPos % 9];
  uint32_t accent = (seedPos % 3 == 0)   ? COL_BUILDING_ACCENT
                    : (seedPos % 3 == 1) ? COL_BUILDING_ACCENT_COOL
                                         : COL_BUILDING_ACCENT_WARM;
  int band;

  draw_box(fb, cam, basis, vec3(x, baseY, z), vec3(w * 0.5f, h * 0.5f, d * 0.5f),
           facade);
  for (band = 0; band < 4; band++) {
    float by = 1.5f + (float)band * (shape == 1 ? 2.7f : 1.85f);
    if (by > h - 0.8f) {
      break;
    }
    draw_box(fb, cam, basis, vec3(x, by, z),
             vec3(w * 0.46f, 0.22f, d * 0.46f), accent);
  }
  if (seedPos % 2 == 0) {
    draw_box(fb, cam, basis, vec3(x + side * 0.8f, h + 0.6f, z),
             vec3(0.55f, 0.7f, 0.55f), COL_WALL_TRIM);
  }
  if (seedPos % 3 == 0) {
    draw_box(fb, cam, basis, vec3(x + side * -1.5f, 2.5f, z),
             vec3(1.4f, 0.1f, 0.7f), COL_AWNING);
  }
}

/* Keep a repeating Z coordinate locked to the camera so props never pop. */
static float wrap_z_near_camera(float preferredZ, float camZ, float nearZ,
                                float span) {
  float z = preferredZ;
  if (span < 1.0f) {
    return preferredZ;
  }
  while (z > camZ + nearZ) {
    z -= span;
  }
  while (z < camZ + nearZ - span) {
    z += span;
  }
  return z;
}

static void draw_sky_props(Framebuffer *fb, const CameraTransform *cam,
                           const CamBasis *basis, const GameState *state) {
  const float cloudSpan = 176.0f;
  const float skylineSpan = 160.0f;
  float camZ = cam->position.z;
  float camX = cam->position.x;
  float scroll = state->player.position.z * 0.42f;
  int c;
  int i;
  int side;

  /* Sun stays in view-space so it never pops. */
  draw_box(fb, cam, basis, vec3(camX + 18.0f, 26.0f, camZ - 55.0f),
           vec3(3.4f, 3.4f, 0.4f), COL_SUN);
  draw_box(fb, cam, basis, vec3(camX + 18.0f, 26.0f, camZ - 54.5f),
           vec3(5.2f, 5.2f, 0.2f), COL_SUN_GLOW);

  for (c = 0; c < 8; c++) {
    float base = scroll - (float)c * 22.0f - 30.0f;
    float cz = wrap_z_near_camera(base, camZ, -20.0f, cloudSpan);
    float cx = ((float)(c % 5) - 2.0f) * 10.0f + state->player.position.x * 0.05f;
    float cy = 15.0f + (float)(c % 3) * 2.0f;
    float sx = 2.2f + (float)(c % 3) * 0.7f;
    draw_box(fb, cam, basis, vec3(cx, cy, cz), vec3(sx, 0.7f, 1.0f), COL_CLOUD);
    draw_box(fb, cam, basis, vec3(cx - sx * 0.55f, cy - 0.15f, cz),
             vec3(sx * 0.55f, 0.55f, 0.85f), COL_CLOUD);
    draw_box(fb, cam, basis, vec3(cx + sx * 0.5f, cy + 0.1f, cz),
             vec3(sx * 0.45f, 0.5f, 0.8f), COL_CLOUD);
  }

  for (i = 0; i < 10; i++) {
    for (side = -1; side <= 1; side += 2) {
      float sx = (float)side;
      float base = scroll - (float)i * 16.0f - 24.0f;
      float cz = wrap_z_near_camera(base, camZ, -18.0f, skylineSpan);
      uint32_t facade = BUILDING_PALETTE[(i + (side > 0 ? 4 : 0)) % 9];
      float tall = (i % 3 == 0) ? 1.55f : 1.15f + (float)(i % 4) * 0.12f;
      float h = 8.0f * tall;
      draw_box(fb, cam, basis,
               vec3(sx * (14.5f + (float)(i % 4) * 1.1f),
                    4.5f + (float)(i % 5) * 0.8f, cz),
               vec3(2.4f, h * 0.5f, 3.2f), facade);
    }
  }
}

static void draw_ground(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameState *state) {
  float laneW = GAME_CONFIG.laneWidth;
  const float spacing = 2.0f;
  const float stripLen = 4.0f;
  const float buildingStep = 12.0f;
  const float billboardStep = 16.0f;
  float halfLen = stripLen * 0.5f;
  float gridZ;
  float decorStart;
  float bz;
  int strip;
  int laneIdx;
  int side;
  int s;
  static const float LANE_DIRS[3] = {-1.0f, 0.0f, 1.0f};
  (void)state;

  /* Lock repeating track pieces to world Z so sleepers don't swim with the camera. */
  gridZ = (float)floor((double)((cam->position.z - 0.5f) / spacing)) * spacing;

  for (strip = 0; strip < 42; strip++) {
    float z0 = gridZ - (float)strip * stripLen;
    float zMid = z0 - halfLen;

    draw_box(fb, cam, basis, vec3(0.0f, -0.14f, zMid),
             vec3(5.1f, 0.14f, halfLen), COL_TRACK);
    draw_box(fb, cam, basis, vec3(0.0f, -0.02f, zMid),
             vec3(3.9f, 0.04f, halfLen), COL_TRACK_CENTER);

    for (side = -1; side <= 1; side += 2) {
      float sx = (float)side;
      draw_box(fb, cam, basis, vec3(sx * 5.85f, 0.22f, zMid),
               vec3(1.7f, 0.35f, halfLen), COL_PLATFORM);
      draw_box(fb, cam, basis, vec3(sx * 5.85f, 0.58f, zMid),
               vec3(1.675f, 0.04f, halfLen), COL_PLATFORM_TOP);
      draw_box(fb, cam, basis, vec3(sx * 4.35f, 0.62f, zMid),
               vec3(0.14f, 0.07f, halfLen), COL_PLATFORM_EDGE);
      draw_box(fb, cam, basis, vec3(sx * 4.55f, 0.52f, zMid),
               vec3(0.08f, 0.05f, halfLen), COL_PLATFORM_EDGE);
      draw_box(fb, cam, basis, vec3(sx * 7.35f, 2.0f, zMid),
               vec3(0.225f, 2.1f, halfLen), COL_WALL);
      draw_box(fb, cam, basis, vec3(sx * 7.35f, 0.65f, zMid),
               vec3(0.24f, 0.14f, halfLen), COL_WALL_TRIM);
      draw_box(fb, cam, basis, vec3(sx * 7.35f, 4.05f, zMid),
               vec3(0.28f, 0.11f, halfLen), COL_WALL_CAP);
    }

    for (laneIdx = 0; laneIdx < 3; laneIdx++) {
      float x = LANE_DIRS[laneIdx] * laneW;
      draw_box(fb, cam, basis, vec3(x, -0.01f, zMid),
               vec3(1.0f, 0.05f, halfLen), COL_BALLAST);
      draw_box(fb, cam, basis, vec3(x - 0.62f, 0.08f, zMid),
               vec3(0.06f, 0.09f, halfLen), COL_RAIL);
      draw_box(fb, cam, basis, vec3(x + 0.62f, 0.08f, zMid),
               vec3(0.06f, 0.09f, halfLen), COL_RAIL);
      draw_box(fb, cam, basis, vec3(x - 0.52f, 0.04f, zMid),
               vec3(0.04f, 0.05f, halfLen), COL_RAIL);
      draw_box(fb, cam, basis, vec3(x + 0.52f, 0.04f, zMid),
               vec3(0.04f, 0.05f, halfLen), COL_RAIL);
      for (s = 0; s < 2; s++) {
        float sz = z0 - spacing * 0.5f - (float)s * spacing;
        draw_box(fb, cam, basis, vec3(x, 0.01f, sz),
                 vec3(0.825f, 0.07f, 0.18f), COL_SLEEPER);
      }
    }
  }

  /* Side city props use a fixed world grid so they never reshuffle with strip indices. */
  decorStart =
      (float)floor((double)((cam->position.z + 4.0f) / buildingStep)) * buildingStep;
  for (bz = decorStart; bz > cam->position.z - 150.0f; bz -= buildingStep) {
    int cell = (int)floor((double)(-bz / buildingStep));
    for (side = -1; side <= 1; side += 2) {
      draw_building(fb, cam, basis, (float)side, bz, cell * 11 + side * 5 + 4);
    }
  }

  decorStart =
      (float)floor((double)((cam->position.z + 4.0f) / billboardStep)) *
      billboardStep;
  for (bz = decorStart; bz > cam->position.z - 150.0f; bz -= billboardStep) {
    int cell = (int)floor((double)(-bz / billboardStep));
    for (side = -1; side <= 1; side += 2) {
      float sx = (float)side;
      uint32_t board =
          ((cell + side) & 1) == 0 ? COL_BILLBOARD : COL_BILLBOARD_ALT;
      draw_box(fb, cam, basis, vec3(sx * 7.55f, 2.35f, bz),
               vec3(0.06f, 1.0f, 1.1f), COL_BILLBOARD_FRAME);
      draw_box(fb, cam, basis, vec3(sx * 7.62f, 2.35f, bz),
               vec3(0.05f, 0.9f, 0.95f), board);
    }
  }
}

static void draw_player(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameState *state) {
  const PlayerState *p = &state->player;
  float bob = 0.0f;
  float py;
  float bodyH;
  float armSwing = 0.0f;
  float legSwing = 0.0f;
  uint32_t bodyColor = COL_PLAYER;
  bool sliding = player_is_sliding(p);
  bool recovering = player_is_board_recovering(p);
  bool invincible =
      has_effect(state->effects, state->effectCount, POWERUP_INVINCIBLE) ||
      state->cheats.immortal;
  bool boost = has_effect(state->effects, state->effectCount, POWERUP_BOOST);
  bool groundedRun = !sliding && p->movement.type != MOVE_STUNNED &&
                     p->movement.type != MOVE_JUMPING &&
                     p->movement.type != MOVE_FALLING;

  if (recovering && ((int)(state->elapsedSeconds * 14.0f) & 1) == 0) {
    return;
  }

  if (groundedRun) {
    float phase = state->elapsedSeconds * (11.0f + state->speed * 0.4f);
    bob = absf(sinf(phase)) * 0.06f;
    armSwing = sinf(phase) * 0.18f;
    legSwing = sinf(phase) * 0.14f;
  }
  py = p->position.y + bob + ((boost || recovering) ? 0.12f : 0.0f);

  if (invincible) {
    bodyColor = COL_INVINCIBLE;
  } else if (boost || recovering) {
    bodyColor = COL_BOOST;
  } else if (sliding) {
    bodyColor = shade_color(COL_PLAYER, 0.9f);
  }

  bodyH = sliding ? 0.35f : 0.55f;
  /* torso */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.35f : 0.78f), p->position.z),
           vec3(0.29f, bodyH * 0.5f, 0.22f), bodyColor);
  /* white chest stripe */
  if (!sliding) {
    draw_box(fb, cam, basis,
             vec3(p->position.x, py + 0.88f, p->position.z + 0.01f),
             vec3(0.3f, 0.07f, 0.23f), COL_PLAYER_STRIPE);
  }
  /* backpack */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.42f : 0.88f),
                p->position.z - 0.28f),
           vec3(0.2f, 0.23f, 0.11f), COL_PLAYER_PACK);
  /* scarf */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.58f : 1.08f),
                p->position.z + 0.14f),
           vec3(0.24f, 0.06f, 0.09f), COL_PLAYER_SCARF);
  /* head */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.75f : 1.42f), p->position.z),
           vec3(0.22f, 0.22f, 0.22f), COL_PLAYER_SKIN);
  /* hair */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.92f : 1.6f),
                p->position.z - 0.04f),
           vec3(0.24f, 0.09f, 0.2f), COL_PLAYER_HAIR);
  /* cap */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.98f : 1.66f), p->position.z),
           vec3(0.24f, 0.06f, 0.23f), COL_PLAYER_ACCENT);
  /* goggles */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.78f : 1.44f),
                p->position.z + 0.2f),
           vec3(0.24f, 0.07f, 0.06f), COL_PLAYER_GOGGLE);
  /* arms */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.4f, py + (sliding ? 0.4f : 0.84f) + armSwing,
                p->position.z),
           vec3(0.09f, 0.28f, 0.09f), COL_PLAYER_ACCENT);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.4f, py + (sliding ? 0.4f : 0.84f) - armSwing,
                p->position.z),
           vec3(0.09f, 0.28f, 0.09f), COL_PLAYER_ACCENT);
  /* legs */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.16f,
                py + (sliding ? 0.12f : 0.28f) - legSwing * 0.3f,
                p->position.z + legSwing * 0.4f),
           vec3(0.09f, 0.22f, 0.09f), COL_PLAYER_ACCENT);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.16f,
                py + (sliding ? 0.12f : 0.28f) + legSwing * 0.3f,
                p->position.z - legSwing * 0.4f),
           vec3(0.09f, 0.22f, 0.09f), COL_PLAYER_ACCENT);
  /* shoes */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.16f, py + 0.05f,
                p->position.z + 0.06f + legSwing * 0.4f),
           vec3(0.12f, 0.06f, 0.16f), COL_PLAYER_SHOE);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.16f, py + 0.05f,
                p->position.z + 0.06f - legSwing * 0.4f),
           vec3(0.12f, 0.06f, 0.16f), COL_PLAYER_SHOE);

  if (boost || recovering) {
    draw_box(fb, cam, basis, vec3(p->position.x, py + 0.07f, p->position.z),
             vec3(0.35f, 0.05f, 0.7f), COL_BOARD);
    draw_box(fb, cam, basis,
             vec3(p->position.x, py + 0.12f, p->position.z - 0.15f),
             vec3(0.18f, 0.03f, 0.22f), shade_color(COL_BOARD, 1.2f));
  }
}

static uint32_t powerup_color(PowerUpType t) {
  switch (t) {
    case POWERUP_MAGNET:
      return COL_MAGNET;
    case POWERUP_MULTIPLIER:
      return COL_MULTIPLIER;
    case POWERUP_INVINCIBLE:
      return COL_INVINCIBLE;
    case POWERUP_BOOST:
      return COL_BOOST;
  }
  return COL_MAGNET;
}

static void draw_train_car(Framebuffer *fb, const CameraTransform *cam,
                           const CamBasis *basis, float x, float centerZ,
                           float halfW, float halfH, float halfCar, float bodyY,
                           uint32_t body, uint32_t accent, int isLead) {
  float wz;
  draw_box(fb, cam, basis, vec3(x, bodyY, centerZ),
           vec3(halfW, halfH, halfCar), body);
  draw_box(fb, cam, basis, vec3(x, bodyY + halfH * 0.35f, centerZ),
           vec3(halfW * 1.02f, 0.12f, halfCar * 0.98f), accent);
  draw_box(fb, cam, basis, vec3(x, bodyY - halfH * 0.45f, centerZ),
           vec3(halfW * 1.02f, 0.08f, halfCar * 0.98f), accent);
  draw_box(fb, cam, basis, vec3(x, bodyY + halfH + 0.08f, centerZ),
           vec3(halfW * 0.78f, 0.1f, halfCar * 0.7f), accent);
  draw_box(fb, cam, basis, vec3(x, bodyY - halfH - 0.12f, centerZ),
           vec3(halfW * 0.85f, 0.14f, halfCar * 0.8f), COL_TRAIN_METAL);

  for (wz = -1.5f; wz <= 1.5f; wz += 1.5f) {
    draw_box(fb, cam, basis,
             vec3(x - halfW - 0.02f, bodyY + 0.35f, centerZ + wz),
             vec3(0.04f, 0.28f, 0.5f), COL_TRAIN_WINDOW);
    draw_box(fb, cam, basis,
             vec3(x + halfW + 0.02f, bodyY + 0.35f, centerZ + wz),
             vec3(0.04f, 0.28f, 0.5f), COL_TRAIN_WINDOW);
  }
  draw_box(fb, cam, basis, vec3(x - halfW - 0.02f, bodyY - 0.15f, centerZ),
           vec3(0.04f, 0.55f, 0.45f), COL_TRAIN_METAL);
  draw_box(fb, cam, basis, vec3(x + halfW + 0.02f, bodyY - 0.15f, centerZ),
           vec3(0.04f, 0.55f, 0.45f), COL_TRAIN_METAL);

  draw_box(fb, cam, basis,
           vec3(x - halfW * 0.75f, bodyY - halfH - 0.12f, centerZ + halfCar * 0.65f),
           vec3(0.12f, 0.18f, 0.12f), COL_TRAIN_WHEEL);
  draw_box(fb, cam, basis,
           vec3(x + halfW * 0.75f, bodyY - halfH - 0.12f, centerZ + halfCar * 0.65f),
           vec3(0.12f, 0.18f, 0.12f), COL_TRAIN_WHEEL);
  draw_box(fb, cam, basis,
           vec3(x - halfW * 0.75f, bodyY - halfH - 0.12f, centerZ - halfCar * 0.65f),
           vec3(0.12f, 0.18f, 0.12f), COL_TRAIN_WHEEL);
  draw_box(fb, cam, basis,
           vec3(x + halfW * 0.75f, bodyY - halfH - 0.12f, centerZ - halfCar * 0.65f),
           vec3(0.12f, 0.18f, 0.12f), COL_TRAIN_WHEEL);

  if (isLead) {
    draw_box(fb, cam, basis,
             vec3(x, bodyY + 0.1f, centerZ + halfCar + 0.35f),
             vec3(halfW * 0.85f, halfH * 0.55f, 0.4f), body);
    draw_box(fb, cam, basis,
             vec3(x - 0.45f, bodyY - 0.2f, centerZ + halfCar + 0.7f),
             vec3(0.14f, 0.1f, 0.06f), COL_TRAIN_LIGHT);
    draw_box(fb, cam, basis,
             vec3(x + 0.45f, bodyY - 0.2f, centerZ + halfCar + 0.7f),
             vec3(0.14f, 0.1f, 0.06f), COL_TRAIN_LIGHT);
    draw_box(fb, cam, basis,
             vec3(x, bodyY + 0.45f, centerZ + halfCar + 0.65f),
             vec3(0.12f, 0.1f, 0.06f), COL_TRAIN_LIGHT);
  } else {
    draw_box(fb, cam, basis,
             vec3(x, bodyY - 0.35f, centerZ + halfCar + 0.2f),
             vec3(0.16f, 0.12f, 0.2f), COL_TRAIN_METAL);
  }
}

static void draw_entity(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameEntity *e,
                        float time) {
  float laneX = (float)e->lane * GAME_CONFIG.laneWidth;
  Bounds b;
  Vec3 half;
  int stripe;

  switch (e->kind) {
    case ENTITY_OBSTACLE:
      if (!get_entity_bounds(e, &b)) {
        return;
      }
      half = b.halfSize;
      if (e->variant == OBSTACLE_BARRIER) {
        draw_box(fb, cam, basis, b.center, half, COL_BARRIER);
        for (stripe = 0; stripe < 4; stripe++) {
          float sy = b.center.y - half.y + 0.15f + (float)stripe * 0.28f;
          draw_box(fb, cam, basis, vec3(b.center.x, sy, b.center.z),
                   vec3(half.x * 1.02f, 0.08f, half.z * 1.05f),
                   (stripe % 2 == 0) ? COL_BARRIER_ACCENT : COL_BARRIER);
        }
        draw_box(fb, cam, basis,
                 vec3(b.center.x - half.x + 0.08f, b.center.y * 0.55f,
                      b.center.z),
                 vec3(0.1f, b.center.y * 0.7f, 0.1f), COL_BARRIER_ACCENT);
        draw_box(fb, cam, basis,
                 vec3(b.center.x + half.x - 0.08f, b.center.y * 0.55f,
                      b.center.z),
                 vec3(0.1f, b.center.y * 0.7f, 0.1f), COL_BARRIER_ACCENT);
      } else if (e->variant == OBSTACLE_LOW_BARRIER) {
        draw_box(fb, cam, basis, b.center, half, COL_BARRIER);
        draw_box(fb, cam, basis,
                 vec3(b.center.x, b.center.y + half.y * 0.35f, b.center.z),
                 vec3(half.x * 1.02f, 0.07f, half.z * 1.05f), COL_BARRIER_ACCENT);
        draw_box(fb, cam, basis,
                 vec3(b.center.x, b.center.y - half.y * 0.35f, b.center.z),
                 vec3(half.x * 1.02f, 0.05f, half.z * 0.95f), COL_BARRIER_ACCENT);
      } else if (e->variant == OBSTACLE_OVERHEAD) {
        draw_box(fb, cam, basis, b.center, half, COL_OVERHEAD);
        draw_box(fb, cam, basis, vec3(laneX - 1.05f, 1.0f, e->positionZ),
                 vec3(0.1f, 1.1f, 0.1f), COL_OVERHEAD_POST);
        draw_box(fb, cam, basis, vec3(laneX + 1.05f, 1.0f, e->positionZ),
                 vec3(0.1f, 1.1f, 0.1f), COL_OVERHEAD_POST);
        draw_box(fb, cam, basis,
                 vec3(laneX, b.center.y - 0.2f, e->positionZ + 0.05f),
                 vec3(0.18f, 0.1f, 0.18f), COL_OVERHEAD_LIGHT);
      } else {
        draw_box(fb, cam, basis, b.center, half, COL_CRATE);
        draw_box(fb, cam, basis,
                 vec3(b.center.x, b.center.y + half.y * 0.25f, b.center.z),
                 vec3(half.x * 1.05f, 0.08f, half.z * 1.05f), COL_CRATE_STRIPE);
        draw_box(fb, cam, basis,
                 vec3(b.center.x, b.center.y + half.y + 0.05f,
                      b.center.z + half.z * 0.7f),
                 vec3(0.25f, 0.05f, 0.05f), COL_TRAIN_METAL);
      }
      break;

    case ENTITY_TRAIN: {
      const float carLen = 7.2f;
      float x = laneX;
      float bodyH = GAME_CONFIG.trainBodyHeight * 0.64f;
      float bodyY = GAME_CONFIG.trainBodyHeight * 0.32f;
      float halfW = GAME_CONFIG.laneWidth * 0.9f * 0.5f;
      float halfH = bodyH * 0.5f;
      float halfCar = carLen * 0.48f;
      int cars = e->cars;
      int c;
      int liv = hash_u32(e->id) % 7;
      uint32_t body = TRAIN_BODY[liv];
      uint32_t accent = TRAIN_ACCENT[liv];
      if (cars < 1) {
        cars = (int)(e->length / carLen + 0.5f);
        if (cars < 1) {
          cars = 1;
        }
      }
      for (c = 0; c < cars; c++) {
        float rearZ = e->positionZ - (float)c * carLen;
        float centerZ = rearZ - carLen * 0.5f;
        draw_train_car(fb, cam, basis, x, centerZ, halfW, halfH, halfCar, bodyY,
                       body, accent, c == 0);
      }
      break;
    }

    case ENTITY_COIN:
      if (e->collected || !get_entity_bounds(e, &b)) {
        return;
      }
      {
        float spin = absf(sinf(time * 7.0f + e->positionZ * 0.35f));
        float bobY = sinf(time * 5.0f + e->positionZ) * 0.08f;
        float hx = 0.08f + spin * 0.34f;
        float hz = 0.42f - spin * 0.18f;
        Vec3 center = vec3(b.center.x, b.center.y + bobY, b.center.z);
        draw_box(fb, cam, basis, center, vec3(hx, 0.42f, hz), COL_COIN);
        draw_box(fb, cam, basis, center,
                 vec3(hx * 1.08f, 0.42f, hz * 1.08f * 0.15f + 0.04f), COL_COIN_RIM);
        draw_box(fb, cam, basis, center, vec3(hx * 0.45f, 0.18f, hz * 0.45f),
                 COL_COIN_CORE);
      }
      break;

    case ENTITY_POWER_UP:
      if (e->collected || !get_entity_bounds(e, &b)) {
        return;
      }
      {
        float bobY = sinf(time * 4.0f + (float)e->id) * 0.16f;
        Vec3 c = vec3(b.center.x, b.center.y + bobY, b.center.z);
        uint32_t col = powerup_color(e->powerUpType);
        if (e->powerUpType == POWERUP_BOOST) {
          draw_box(fb, cam, basis, c, vec3(0.32f, 0.08f, 0.55f), col);
        } else if (e->powerUpType == POWERUP_MAGNET) {
          draw_box(fb, cam, basis, c, vec3(0.35f, 0.12f, 0.35f), col);
          draw_box(fb, cam, basis, vec3(c.x, c.y, c.z),
                   vec3(0.45f, 0.06f, 0.45f), shade_color(col, 1.2f));
        } else {
          draw_box(fb, cam, basis, c, vec3(0.28f, 0.28f, 0.28f), col);
          draw_box(fb, cam, basis, c, vec3(0.14f, 0.42f, 0.14f),
                   shade_color(col, 1.15f));
        }
      }
      break;

    case ENTITY_DECORATION: {
      float x = (float)e->lane * (GAME_CONFIG.laneWidth * 2.45f);
      if (e->style == DECOR_LAMP) {
        draw_box(fb, cam, basis, vec3(x, 1.55f, e->positionZ),
                 vec3(0.1f, 1.7f, 0.1f), COL_LAMP);
        draw_box(fb, cam, basis, vec3(x, 3.25f, e->positionZ),
                 vec3(0.28f, 0.28f, 0.28f), COL_LAMP_GLOW);
        draw_box(fb, cam, basis, vec3(x + 0.22f, 2.4f, e->positionZ),
                 vec3(0.18f, 0.28f, 0.12f), COL_OVERHEAD_LIGHT);
      } else if (e->style == DECOR_SIGN) {
        uint32_t panel =
            (hash_u32(e->id) % 2 == 0) ? COL_SIGN : COL_SIGN_ALT;
        draw_box(fb, cam, basis, vec3(x, 1.55f, e->positionZ),
                 vec3(0.09f, 1.55f, 0.09f), COL_LAMP);
        draw_box(fb, cam, basis, vec3(x, 3.1f, e->positionZ),
                 vec3(0.08f, 0.55f, 0.9f), COL_BILLBOARD_FRAME);
        draw_box(fb, cam, basis, vec3(x + 0.05f, 3.1f, e->positionZ),
                 vec3(0.06f, 0.48f, 0.8f), panel);
      } else {
        draw_box(fb, cam, basis, vec3(x, 2.0f, e->positionZ),
                 vec3(0.375f, 2.2f, 0.375f), COL_PILLAR);
        draw_box(fb, cam, basis, vec3(x, 4.15f, e->positionZ),
                 vec3(0.42f, 0.12f, 0.42f), COL_WALL_CAP);
      }
      break;
    }
  }
}

static void draw_char(Framebuffer *fb, int x, int y, char c, uint32_t color,
                      int scale) {
  const unsigned char *glyph;
  int row, col, sx, sy;
  int idx = font_index(c);
  glyph = FONT_5X7[idx];
  for (row = 0; row < 7; row++) {
    unsigned char bits = glyph[row];
    for (col = 0; col < 5; col++) {
      if (bits & (1u << (4 - col))) {
        for (sy = 0; sy < scale; sy++) {
          for (sx = 0; sx < scale; sx++) {
            put_pixel_hud(fb, x + col * scale + sx, y + row * scale + sy,
                          color);
          }
        }
      }
    }
  }
}

static void draw_char_outlined(Framebuffer *fb, int x, int y, char c,
                               uint32_t color, int scale) {
  int ox, oy;
  for (oy = -1; oy <= 1; oy++) {
    for (ox = -1; ox <= 1; ox++) {
      if (ox == 0 && oy == 0) {
        continue;
      }
      draw_char(fb, x + ox * scale, y + oy * scale, c, COL_HUD_OUTLINE, scale);
    }
  }
  draw_char(fb, x, y, c, color, scale);
}

static void draw_text(Framebuffer *fb, int x, int y, const char *text,
                      uint32_t color, int scale) {
  int cx = x;
  while (*text) {
    if (*text == '\n') {
      cx = x;
      y += (7 + 1) * scale;
      text++;
      continue;
    }
    draw_char_outlined(fb, cx, y, *text, color, scale);
    cx += (5 + 1) * scale;
    text++;
  }
}

static void fill_rect_hud(Framebuffer *fb, int x0, int y0, int x1, int y1,
                          uint32_t color) {
  int x, y;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 > fb->width) {
    x1 = fb->width;
  }
  if (y1 > fb->height) {
    y1 = fb->height;
  }
  for (y = y0; y < y1; y++) {
    for (x = x0; x < x1; x++) {
      uint32_t dst = fb->pixels[y * fb->width + x];
      int a = (int)((color >> 24) & 0xFFu);
      if (a >= 250) {
        put_pixel_hud(fb, x, y, color | 0xFF000000u);
      } else {
        float t = (float)a / 255.0f;
        put_pixel_hud(fb, x, y, lerp_color(dst | 0xFF000000u, color | 0xFF000000u, t));
      }
    }
  }
}

static void draw_hud(Framebuffer *fb, const GameState *state) {
  char line[96];
  int y = 10;
  const char *status = NULL;
  float mult = get_multiplier(state->effects, state->effectCount, &state->cheats);

  fill_rect_hud(fb, 4, 4, 168, 92, COL_HUD_PANEL);
  fill_rect_hud(fb, fb->width - 118, 4, fb->width - 4, 36, COL_HUD_PANEL);

  sprintf(line, "SCORE %d", (int)state->score);
  draw_text(fb, 10, y, line, COL_HUD, 2);
  y += 18;
  sprintf(line, "COINS %d", state->coins);
  draw_text(fb, 10, y, line, COL_HUD_GOLD, 2);
  y += 18;
  sprintf(line, "DIST %dm", (int)state->distance);
  draw_text(fb, 10, y, line, COL_HUD_DIM, 2);
  y += 18;
  sprintf(line, "BEST %d", (int)state->highScore);
  draw_text(fb, 10, y, line, COL_HUD_DIM, 2);

  sprintf(line, "BOARD %d", state->boardCharges);
  draw_text(fb, fb->width - 8 - (int)strlen(line) * 12, 10, line, COL_BOARD, 2);

  if (mult > 1.01f) {
    sprintf(line, "x%d", (int)(mult + 0.1f));
    draw_text(fb, fb->width - 8 - (int)strlen(line) * 12, 32, line, COL_MULTIPLIER,
              2);
  }

  if (state->effectCount > 0) {
    int i;
    int ex = 10;
    for (i = 0; i < state->effectCount; i++) {
      const char *label = "?";
      uint32_t col = COL_HUD;
      switch (state->effects[i].type) {
        case POWERUP_MAGNET:
          label = "MAG";
          col = COL_MAGNET;
          break;
        case POWERUP_MULTIPLIER:
          label = "x2";
          col = COL_MULTIPLIER;
          break;
        case POWERUP_INVINCIBLE:
          label = "SHIELD";
          col = COL_INVINCIBLE;
          break;
        case POWERUP_BOOST:
          label = "JET";
          col = COL_BOOST;
          break;
      }
      draw_text(fb, ex, 100, label, col, 1);
      ex += (int)strlen(label) * 6 + 10;
    }
  }

  switch (state->status.type) {
    case STATUS_READY:
      status = "METRO RUSH";
      break;
    case STATUS_PAUSED:
      status = "PAUSED";
      break;
    case STATUS_GAME_OVER:
      status = "GAME OVER";
      break;
    case STATUS_RUNNING:
      status = NULL;
      break;
  }

  if (status != NULL) {
    int len = (int)strlen(status);
    int tx = (fb->width - len * 12) / 2;
    int ty = fb->height / 2 - 48;
    fill_rect_hud(fb, tx - 16, ty - 12, tx + len * 12 + 16, ty + 72, COL_HUD_PANEL);
    draw_text(fb, tx, ty, status, COL_HUD, 2);
    if (state->status.type == STATUS_READY) {
      draw_text(fb, (fb->width - 22 * 12) / 2, ty + 28,
                "ENTER TO START", COL_HUD_GOLD, 2);
      draw_text(fb, 8, fb->height - 40,
                "A/D LANE  W/SPACE JUMP  S SLIDE  P PAUSE", COL_HUD_DIM, 1);
      draw_text(fb, 8, fb->height - 28,
                "M MUTE  ` CHEATS 1-4  5 BOARD 6 COINS 7 SCORE", COL_HUD_DIM,
                1);
    } else if (state->status.type == STATUS_GAME_OVER) {
      draw_text(fb, (fb->width - 20 * 12) / 2, ty + 28,
                "ENTER TO RESTART", COL_HUD_DIM, 2);
    } else if (state->status.type == STATUS_PAUSED) {
      draw_text(fb, (fb->width - 16 * 12) / 2, ty + 28, "P TO RESUME",
                COL_HUD_DIM, 2);
    }
  }

  if (g_show_cheats || is_cheat_active(state->cheats)) {
    y = 118;
    draw_text(fb, 8, y, "CHEATS", COL_HUD, 2);
    y += 18;
    sprintf(line, "1 IMMORTAL %s", state->cheats.immortal ? "ON" : "OFF");
    draw_text(fb, 8, y, line, COL_HUD_DIM, 1);
    y += 10;
    sprintf(line, "2 MAGNET %s", state->cheats.infiniteMagnet ? "ON" : "OFF");
    draw_text(fb, 8, y, line, COL_HUD_DIM, 1);
    y += 10;
    sprintf(line, "3 MULT %s",
            state->cheats.infiniteMultiplier ? "ON" : "OFF");
    draw_text(fb, 8, y, line, COL_HUD_DIM, 1);
    y += 10;
    sprintf(line, "4 MAXSPD %s", state->cheats.lockMaxSpeed ? "ON" : "OFF");
    draw_text(fb, 8, y, line, COL_HUD_DIM, 1);
  }

  if (state->muted) {
    draw_text(fb, fb->width - 60, 40, "MUTE", COL_HUD_DIM, 1);
  }
}

void render_init(void) {
  g_cam_ready = 0;
  g_zbuf = NULL;
  g_zbuf_w = 0;
  g_zbuf_h = 0;
  g_show_cheats = 0;
}

void render_toggle_cheat_overlay(void) {
  g_show_cheats = !g_show_cheats;
}

void render_frame(Framebuffer *fb, GameState *state, float dt) {
  GameEntity flat[MAX_FLATTEN_ENTITIES];
  int count;
  int i;
  CamBasis basis;

  if (fb == NULL || state == NULL || fb->pixels == NULL) {
    return;
  }

  ensure_zbuf(fb->width, fb->height);
  clear_framebuffer(fb);

  if (!g_cam_ready) {
    g_cam = create_initial_camera(state);
    g_cam_ready = 1;
  } else {
    g_cam = calculate_camera(state, &g_cam, dt > 0.0f ? dt : 0.016f);
  }

  basis = make_basis(&g_cam, fb->width, fb->height);
  draw_sky_props(fb, &g_cam, &basis, state);
  draw_ground(fb, &g_cam, &basis, state);

  count = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
  for (i = 0; i < count; i++) {
    draw_entity(fb, &g_cam, &basis, &flat[i], state->elapsedSeconds);
  }

  draw_player(fb, &g_cam, &basis, state);
  draw_hud(fb, state);
}
