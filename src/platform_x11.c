#ifdef METRO_LINUX

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform.h"
#include "render.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct Platform {
  Display *display;
  Window window;
  GC gc;
  XImage *image;
  Atom wm_delete;
  int width;
  int height;
  uint32_t *pixels;
  int running;
};

static void push_cmd(InputState *input, CommandType type) {
  GameCommand cmd;
  if (input->count >= MAX_COMMANDS) {
    return;
  }
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = type;
  input->commands[input->count++] = cmd;
}

static void push_cheat(InputState *input, CheatId id) {
  GameCommand cmd;
  if (input->count >= MAX_COMMANDS) {
    return;
  }
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = CMD_TOGGLE_CHEAT;
  cmd.cheatId = id;
  input->commands[input->count++] = cmd;
}

static void push_cheat_action(InputState *input, CheatAction action) {
  GameCommand cmd;
  if (input->count >= MAX_COMMANDS) {
    return;
  }
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = CMD_CHEAT_ACTION;
  cmd.cheatAction = action;
  input->commands[input->count++] = cmd;
}

static void handle_key(KeySym key, InputState *input) {
  switch (key) {
    case XK_a:
    case XK_A:
    case XK_Left:
      push_cmd(input, CMD_MOVE_LEFT);
      break;
    case XK_d:
    case XK_D:
    case XK_Right:
      push_cmd(input, CMD_MOVE_RIGHT);
      break;
    case XK_w:
    case XK_W:
    case XK_Up:
    case XK_space:
      push_cmd(input, CMD_JUMP);
      break;
    case XK_s:
    case XK_S:
    case XK_Down:
      push_cmd(input, CMD_SLIDE);
      break;
    case XK_p:
    case XK_P:
    case XK_Escape:
      push_cmd(input, CMD_PAUSE);
      break;
    case XK_Return:
    case XK_KP_Enter:
      push_cmd(input, CMD_RESTART);
      break;
    case XK_m:
    case XK_M:
      push_cmd(input, CMD_TOGGLE_MUTE);
      break;
    case XK_grave:
    case XK_F1:
      render_toggle_cheat_overlay();
      break;
    case XK_1:
    case XK_KP_1:
      push_cheat(input, CHEAT_IMMORTAL);
      break;
    case XK_2:
    case XK_KP_2:
      push_cheat(input, CHEAT_INFINITE_MAGNET);
      break;
    case XK_3:
    case XK_KP_3:
      push_cheat(input, CHEAT_INFINITE_MULTIPLIER);
      break;
    case XK_4:
    case XK_KP_4:
      push_cheat(input, CHEAT_LOCK_MAX_SPEED);
      break;
    case XK_5:
    case XK_KP_5:
      push_cheat_action(input, CHEAT_ACTION_REFILL_BOARD);
      break;
    case XK_6:
    case XK_KP_6:
      push_cheat_action(input, CHEAT_ACTION_ADD_COINS);
      break;
    case XK_7:
    case XK_KP_7:
      push_cheat_action(input, CHEAT_ACTION_ADD_SCORE);
      break;
    default:
      break;
  }
}

int platform_init(Platform **out, int width, int height, const char *title) {
  Platform *p;
  XSetWindowAttributes attrs;
  int screen;
  Visual *visual;
  int depth;

  if (out == NULL || width <= 0 || height <= 0) {
    return 0;
  }

  p = (Platform *)calloc(1, sizeof(Platform));
  if (p == NULL) {
    return 0;
  }

  p->display = XOpenDisplay(NULL);
  if (p->display == NULL) {
    free(p);
    return 0;
  }

  screen = DefaultScreen(p->display);
  visual = DefaultVisual(p->display, screen);
  depth = DefaultDepth(p->display, screen);
  p->width = width;
  p->height = height;
  p->running = 1;

  attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
  attrs.background_pixel = BlackPixel(p->display, screen);
  attrs.colormap = DefaultColormap(p->display, screen);
  p->window = XCreateWindow(
      p->display, RootWindow(p->display, screen), 0, 0, (unsigned)width,
      (unsigned)height, 0, depth, InputOutput, visual,
      CWEventMask | CWBackPixel | CWColormap, &attrs);

  XStoreName(p->display, p->window, title != NULL ? title : "Metro Rush");
  p->wm_delete = XInternAtom(p->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(p->display, p->window, &p->wm_delete, 1);

  p->gc = XCreateGC(p->display, p->window, 0, NULL);
  p->pixels =
      (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
  if (p->pixels == NULL) {
    platform_shutdown(p);
    return 0;
  }

  p->image =
      XCreateImage(p->display, visual, (unsigned)depth, ZPixmap, 0,
                   (char *)p->pixels, (unsigned)width, (unsigned)height, 32,
                   width * 4);
  if (p->image == NULL) {
    platform_shutdown(p);
    return 0;
  }
  /* We own the pixel buffer; prevent XDestroyImage from freeing it. */
  p->image->data = (char *)p->pixels;

  XMapWindow(p->display, p->window);
  XFlush(p->display);

  *out = p;
  return 1;
}

void platform_shutdown(Platform *p) {
  if (p == NULL) {
    return;
  }
  if (p->image != NULL) {
    p->image->data = NULL;
    XDestroyImage(p->image);
    p->image = NULL;
  }
  if (p->pixels != NULL) {
    free(p->pixels);
    p->pixels = NULL;
  }
  if (p->gc != NULL && p->display != NULL) {
    XFreeGC(p->display, p->gc);
  }
  if (p->window != 0 && p->display != NULL) {
    XDestroyWindow(p->display, p->window);
  }
  if (p->display != NULL) {
    XCloseDisplay(p->display);
  }
  free(p);
}

int platform_poll(Platform *p, InputState *input) {
  XEvent ev;
  if (p == NULL || input == NULL) {
    return 0;
  }
  *input = empty_input();

  while (XPending(p->display) > 0) {
    XNextEvent(p->display, &ev);
    if (ev.type == ClientMessage) {
      if ((Atom)ev.xclient.data.l[0] == p->wm_delete) {
        p->running = 0;
      }
    } else if (ev.type == KeyPress) {
      handle_key(XLookupKeysym(&ev.xkey, 0), input);
    } else if (ev.type == DestroyNotify) {
      p->running = 0;
    }
  }

  return p->running;
}

void platform_present(Platform *p, const Framebuffer *fb) {
  if (p == NULL || fb == NULL || fb->pixels == NULL || p->image == NULL) {
    return;
  }

  if (fb->pixels != p->pixels) {
    int n = p->width * p->height;
    int count = fb->width * fb->height;
    if (count > n) {
      count = n;
    }
    memcpy(p->pixels, fb->pixels, (size_t)count * sizeof(uint32_t));
  }

  XPutImage(p->display, p->window, p->gc, p->image, 0, 0, 0, 0,
            (unsigned)p->width, (unsigned)p->height);
  XFlush(p->display);
}

double platform_time_seconds(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
  }
  return 0.0;
}

void platform_sleep_ms(int ms) {
  struct timespec req;
  if (ms <= 0) {
    return;
  }
  req.tv_sec = ms / 1000;
  req.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&req, NULL);
}

#endif /* METRO_LINUX */
