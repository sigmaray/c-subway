#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef METRO_WIN32
#include <windows.h>
#endif

#define SAVE_VERSION 1

static void default_save(SaveData *out) {
  memset(out, 0, sizeof(*out));
  out->highScore = 0.0f;
  out->totalCoins = 0;
  out->muted = false;
  out->cheats = DEFAULT_CHEATS;
}

static void build_save_path(char *buf, size_t buflen) {
#ifdef METRO_LINUX
  const char *home = getenv("HOME");
  if (home != NULL && home[0] != '\0' && strlen(home) + 20 < buflen) {
    sprintf(buf, "%s/.metro-rush-save", home);
    return;
  }
  strncpy(buf, "metro-rush.save", buflen - 1);
  buf[buflen - 1] = '\0';
#elif defined(METRO_WIN32)
  char module[MAX_PATH];
  char *slash;
  DWORD n = GetModuleFileNameA(NULL, module, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    slash = strrchr(module, '\\');
    if (slash == NULL) {
      slash = strrchr(module, '/');
    }
    if (slash != NULL) {
      *(slash + 1) = '\0';
      if (strlen(module) + 16 < buflen) {
        sprintf(buf, "%smetro-rush.save", module);
        return;
      }
    }
  }
  strncpy(buf, "metro-rush.save", buflen - 1);
  buf[buflen - 1] = '\0';
#else
  strncpy(buf, "metro-rush.save", buflen - 1);
  buf[buflen - 1] = '\0';
#endif
}

void storage_load(SaveData *out) {
  char path[512];
  FILE *f;
  int version = 0;
  int muted = 0;
  int immortal = 0;
  int infiniteMagnet = 0;
  int infiniteMultiplier = 0;
  int lockMaxSpeed = 0;
  float highScore = 0.0f;
  int totalCoins = 0;

  if (out == NULL) {
    return;
  }
  default_save(out);
  build_save_path(path, sizeof(path));

  f = fopen(path, "r");
  if (f == NULL) {
    return;
  }

  if (fscanf(f, "version %d\n", &version) != 1) {
    fclose(f);
    return;
  }
  if (version != SAVE_VERSION) {
    fclose(f);
    return;
  }
  if (fscanf(f, "highScore %f\n", &highScore) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "totalCoins %d\n", &totalCoins) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "muted %d\n", &muted) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "immortal %d\n", &immortal) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "infiniteMagnet %d\n", &infiniteMagnet) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "infiniteMultiplier %d\n", &infiniteMultiplier) != 1) {
    fclose(f);
    return;
  }
  if (fscanf(f, "lockMaxSpeed %d\n", &lockMaxSpeed) != 1) {
    fclose(f);
    return;
  }
  fclose(f);

  out->highScore = highScore;
  out->totalCoins = totalCoins;
  out->muted = muted != 0;
  out->cheats.immortal = immortal != 0;
  out->cheats.infiniteMagnet = infiniteMagnet != 0;
  out->cheats.infiniteMultiplier = infiniteMultiplier != 0;
  out->cheats.lockMaxSpeed = lockMaxSpeed != 0;
}

void storage_save(const SaveData *data) {
  char path[512];
  FILE *f;
  if (data == NULL) {
    return;
  }
  build_save_path(path, sizeof(path));
  f = fopen(path, "w");
  if (f == NULL) {
    return;
  }
  fprintf(f, "version %d\n", SAVE_VERSION);
  fprintf(f, "highScore %.3f\n", data->highScore);
  fprintf(f, "totalCoins %d\n", data->totalCoins);
  fprintf(f, "muted %d\n", data->muted ? 1 : 0);
  fprintf(f, "immortal %d\n", data->cheats.immortal ? 1 : 0);
  fprintf(f, "infiniteMagnet %d\n", data->cheats.infiniteMagnet ? 1 : 0);
  fprintf(f, "infiniteMultiplier %d\n",
          data->cheats.infiniteMultiplier ? 1 : 0);
  fprintf(f, "lockMaxSpeed %d\n", data->cheats.lockMaxSpeed ? 1 : 0);
  fclose(f);
}
