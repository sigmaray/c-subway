#ifndef PLATFORM_H
#define PLATFORM_H

#include "metro.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Platform Platform;

typedef struct {
  int width, height;
  uint32_t *pixels; /* ARGB8888 or XRGB, row-major */
} Framebuffer;

int  platform_init(Platform **out, int width, int height, const char *title);
void platform_shutdown(Platform *p);
int  platform_poll(Platform *p, InputState *input); /* returns 0 to quit */
void platform_present(Platform *p, const Framebuffer *fb);
double platform_time_seconds(void); /* monotonic-ish */
void platform_sleep_ms(int ms);

typedef struct SaveData {
  float highScore;
  int totalCoins;
  bool muted;
  CheatFlags cheats;
} SaveData;

void storage_load(SaveData *out);
void storage_save(const SaveData *data);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
