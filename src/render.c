#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SKY_COLOR 0xFF4FC3F7u
#define COL_PLAYER 0xFFFF1E4Du
#define COL_PLAYER_ACCENT 0xFF2563EBu
#define COL_PLAYER_SKIN 0xFFFFC29Au
#define COL_PLAYER_SHOE 0xFFFBBF24u
#define COL_BARRIER 0xFFFFCC00u
#define COL_LOW_BARRIER 0xFFFFCC00u
#define COL_OVERHEAD 0xFFA855F7u
#define COL_CRATE 0xFFC2410Cu
#define COL_TRAIN 0xFF22C1F0u
#define COL_COIN 0xFFFFD60Au
#define COL_MAGNET 0xFF38BDF8u
#define COL_MULTIPLIER 0xFFFFCC00u
#define COL_INVINCIBLE 0xFFD8B4FEu
#define COL_BOOST 0xFF2DD4BFu
#define COL_LAMP 0xFF475569u
#define COL_LAMP_GLOW 0xFFFFF7EDu
#define COL_SIGN 0xFF22D3EEu
#define COL_PILLAR 0xFFA8A29Eu
#define COL_TRACK 0xFF6B5B4Au
#define COL_TRACK_CENTER 0xFF564737u
#define COL_BALLAST 0xFF8A7A66u
#define COL_RAIL 0xFFE8E8E8u
#define COL_SLEEPER 0xFF6B3F1Du
#define COL_PLATFORM 0xFFB0BEC8u
#define COL_PLATFORM_TOP 0xFFE8EEF3u
#define COL_PLATFORM_EDGE 0xFFFFCC00u
#define COL_WALL 0xFF90A4AEu
#define COL_WALL_TRIM 0xFFFF7A18u
#define COL_HUD 0xFFFFFFFFu
#define COL_HUD_DIM 0xFFE2E8F0u
#define COL_HUD_OUTLINE 0xFF0F172Au

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
  float t = clampf(intensity, 0.15f, 1.0f);
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
  int n = fb->width * fb->height;
  int i;
  for (i = 0; i < n; i++) {
    fb->pixels[i] = SKY_COLOR;
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
    intensity = 0.35f + 0.65f * maxf(0.0f, v3_dot(n, light));
    shaded = shade_color(color, intensity);

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

static void draw_ground(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameState *state) {
  float laneW = GAME_CONFIG.laneWidth;
  const float spacing = 2.0f;
  const float stripLen = 4.0f;
  float halfLen = stripLen * 0.5f;
  float gridZ;
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
      draw_box(fb, cam, basis, vec3(sx * 7.35f, 2.0f, zMid),
               vec3(0.225f, 2.1f, halfLen), COL_WALL);
      draw_box(fb, cam, basis, vec3(sx * 7.35f, 0.65f, zMid),
               vec3(0.24f, 0.14f, halfLen), COL_WALL_TRIM);
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
}

static void draw_player(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameState *state) {
  const PlayerState *p = &state->player;
  float bob = 0.0f;
  float py;
  float bodyH;
  uint32_t bodyColor = COL_PLAYER;
  bool sliding = player_is_sliding(p);
  bool recovering = player_is_board_recovering(p);
  bool invincible =
      has_effect(state->effects, state->effectCount, POWERUP_INVINCIBLE) ||
      state->cheats.immortal;
  bool boost = has_effect(state->effects, state->effectCount, POWERUP_BOOST);

  if (recovering && ((int)(state->elapsedSeconds * 14.0f) & 1) == 0) {
    return;
  }

  if (!sliding && p->movement.type != MOVE_STUNNED &&
      p->movement.type != MOVE_JUMPING && p->movement.type != MOVE_FALLING) {
    bob = absf(sinf(state->elapsedSeconds * (11.0f + state->speed * 0.4f))) *
          0.06f;
  }
  py = p->position.y + bob + ((boost || recovering) ? 0.12f : 0.0f);

  if (invincible) {
    bodyColor = COL_INVINCIBLE;
  } else if (boost || recovering) {
    bodyColor = COL_BOOST;
  } else if (sliding) {
    bodyColor = shade_color(COL_PLAYER, 0.85f);
  }

  bodyH = sliding ? 0.35f : 0.55f;
  /* torso */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.35f : 0.78f), p->position.z),
           vec3(0.28f, bodyH * 0.5f, 0.22f), bodyColor);
  /* head */
  draw_box(fb, cam, basis,
           vec3(p->position.x, py + (sliding ? 0.75f : 1.35f), p->position.z),
           vec3(0.18f, 0.18f, 0.18f), COL_PLAYER_SKIN);
  /* arms */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.38f, py + (sliding ? 0.4f : 0.8f),
                p->position.z),
           vec3(0.08f, 0.28f, 0.08f), COL_PLAYER_ACCENT);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.38f, py + (sliding ? 0.4f : 0.8f),
                p->position.z),
           vec3(0.08f, 0.28f, 0.08f), COL_PLAYER_ACCENT);
  /* legs */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.14f, py + (sliding ? 0.12f : 0.28f),
                p->position.z),
           vec3(0.09f, 0.22f, 0.09f), COL_PLAYER_ACCENT);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.14f, py + (sliding ? 0.12f : 0.28f),
                p->position.z),
           vec3(0.09f, 0.22f, 0.09f), COL_PLAYER_ACCENT);
  /* shoes */
  draw_box(fb, cam, basis,
           vec3(p->position.x - 0.14f, py + 0.05f, p->position.z + 0.06f),
           vec3(0.1f, 0.05f, 0.14f), COL_PLAYER_SHOE);
  draw_box(fb, cam, basis,
           vec3(p->position.x + 0.14f, py + 0.05f, p->position.z + 0.06f),
           vec3(0.1f, 0.05f, 0.14f), COL_PLAYER_SHOE);

  if (boost || recovering) {
    draw_box(fb, cam, basis,
             vec3(p->position.x, py + 0.07f, p->position.z),
             vec3(0.45f, 0.05f, 0.7f), COL_BOOST);
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

static void draw_entity(Framebuffer *fb, const CameraTransform *cam,
                        const CamBasis *basis, const GameEntity *e) {
  float laneX = (float)e->lane * GAME_CONFIG.laneWidth;
  Bounds b;
  Vec3 half;

  switch (e->kind) {
    case ENTITY_OBSTACLE:
      if (!get_entity_bounds(e, &b)) {
        return;
      }
      half = b.halfSize;
      {
        uint32_t col = COL_BARRIER;
        if (e->variant == OBSTACLE_LOW_BARRIER) {
          col = COL_LOW_BARRIER;
        } else if (e->variant == OBSTACLE_OVERHEAD) {
          col = COL_OVERHEAD;
        } else if (e->variant == OBSTACLE_CRATE) {
          col = COL_CRATE;
        }
        draw_box(fb, cam, basis, b.center, half, col);
        if (e->variant == OBSTACLE_BARRIER) {
          draw_box(fb, cam, basis,
                   vec3(b.center.x - half.x + 0.08f, b.center.y * 0.5f,
                        b.center.z),
                   vec3(0.08f, b.center.y * 0.5f, 0.08f), 0xFF111827u);
          draw_box(fb, cam, basis,
                   vec3(b.center.x + half.x - 0.08f, b.center.y * 0.5f,
                        b.center.z),
                   vec3(0.08f, b.center.y * 0.5f, 0.08f), 0xFF111827u);
        } else if (e->variant == OBSTACLE_OVERHEAD) {
          draw_box(fb, cam, basis,
                   vec3(laneX - 1.05f, 1.0f, e->positionZ),
                   vec3(0.1f, 1.0f, 0.1f), 0xFF334155u);
          draw_box(fb, cam, basis,
                   vec3(laneX + 1.05f, 1.0f, e->positionZ),
                   vec3(0.1f, 1.0f, 0.1f), 0xFF334155u);
        }
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
      if (cars < 1) {
        cars = (int)(e->length / carLen + 0.5f);
        if (cars < 1) {
          cars = 1;
        }
      }
      /* Draw per car so long trains don't straddle the near plane as one mesh. */
      for (c = 0; c < cars; c++) {
        float rearZ = e->positionZ - (float)c * carLen;
        float centerZ = rearZ - carLen * 0.5f;
        draw_box(fb, cam, basis, vec3(x, bodyY, centerZ),
                 vec3(halfW, halfH, halfCar), COL_TRAIN);
        draw_box(fb, cam, basis,
                 vec3(x, bodyY + halfH * 0.55f, centerZ),
                 vec3(halfW * 0.95f, 0.12f, halfCar * 0.9f), 0xFFFF8A3Du);
      }
      break;
    }

    case ENTITY_COIN:
      if (e->collected || !get_entity_bounds(e, &b)) {
        return;
      }
      draw_box(fb, cam, basis, b.center, b.halfSize, COL_COIN);
      break;

    case ENTITY_POWER_UP:
      if (e->collected || !get_entity_bounds(e, &b)) {
        return;
      }
      draw_box(fb, cam, basis, b.center, b.halfSize,
               powerup_color(e->powerUpType));
      break;

    case ENTITY_DECORATION: {
      /* Side props sit off the playable lanes (same as ts-subway). */
      float x = (float)e->lane * (GAME_CONFIG.laneWidth * 2.45f);
      if (e->style == DECOR_LAMP) {
        draw_box(fb, cam, basis, vec3(x, 1.55f, e->positionZ),
                 vec3(0.1f, 1.7f, 0.1f), COL_LAMP);
        draw_box(fb, cam, basis, vec3(x, 3.25f, e->positionZ),
                 vec3(0.28f, 0.28f, 0.28f), COL_LAMP_GLOW);
      } else if (e->style == DECOR_SIGN) {
        draw_box(fb, cam, basis, vec3(x, 1.55f, e->positionZ),
                 vec3(0.09f, 1.55f, 0.09f), COL_LAMP);
        draw_box(fb, cam, basis, vec3(x, 3.1f, e->positionZ),
                 vec3(0.08f, 0.55f, 0.9f), COL_SIGN);
      } else {
        draw_box(fb, cam, basis, vec3(x, 2.0f, e->positionZ),
                 vec3(0.375f, 2.2f, 0.375f), COL_PILLAR);
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

static void draw_hud(Framebuffer *fb, const GameState *state) {
  char line[96];
  int y = 8;
  const char *status = NULL;

  sprintf(line, "SCORE %d", (int)state->score);
  draw_text(fb, 8, y, line, COL_HUD, 2);
  y += 18;
  sprintf(line, "COINS %d", state->coins);
  draw_text(fb, 8, y, line, COL_COIN, 2);
  y += 18;
  sprintf(line, "DIST %dm", (int)state->distance);
  draw_text(fb, 8, y, line, COL_HUD_DIM, 2);
  y += 18;
  sprintf(line, "BEST %d", (int)state->highScore);
  draw_text(fb, 8, y, line, COL_HUD_DIM, 2);

  sprintf(line, "BOARD %d", state->boardCharges);
  draw_text(fb, fb->width - 8 - (int)strlen(line) * 12, 8, line, COL_BOOST, 2);

  switch (state->status.type) {
    case STATUS_READY:
      status = "READY";
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
    int ty = fb->height / 2 - 40;
    draw_text(fb, tx, ty, status, COL_HUD, 2);
    if (state->status.type == STATUS_READY) {
      draw_text(fb, (fb->width - 22 * 12) / 2, ty + 28,
                "ENTER TO START", COL_HUD_DIM, 2);
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
    y = 100;
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
    draw_text(fb, fb->width - 60, 28, "MUTE", COL_HUD_DIM, 1);
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
  draw_ground(fb, &g_cam, &basis, state);

  count = flatten_entities(state, flat, MAX_FLATTEN_ENTITIES);
  for (i = 0; i < count; i++) {
    draw_entity(fb, &g_cam, &basis, &flat[i]);
  }

  draw_player(fb, &g_cam, &basis, state);
  draw_hud(fb, state);
}
