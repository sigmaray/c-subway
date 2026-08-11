#ifdef METRO_WIN32

#ifndef WINVER
#define WINVER 0x0400
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include "platform.h"
#include "render.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Platform {
  HWND hwnd;
  HDC hdc;
  BITMAPINFO bmi;
  int width;
  int height;
  uint32_t *pixels;
  int running;
  InputState pending;
};

static Platform *g_platform;

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

static void handle_vk(WPARAM vk, InputState *input) {
  switch (vk) {
    case 'A':
    case VK_LEFT:
      push_cmd(input, CMD_MOVE_LEFT);
      break;
    case 'D':
    case VK_RIGHT:
      push_cmd(input, CMD_MOVE_RIGHT);
      break;
    case 'W':
    case VK_UP:
    case VK_SPACE:
      push_cmd(input, CMD_JUMP);
      break;
    case 'S':
    case VK_DOWN:
      push_cmd(input, CMD_SLIDE);
      break;
    case 'P':
    case VK_ESCAPE:
      push_cmd(input, CMD_PAUSE);
      break;
    case VK_RETURN:
      push_cmd(input, CMD_RESTART);
      break;
    case 'M':
      push_cmd(input, CMD_TOGGLE_MUTE);
      break;
    case VK_OEM_3: /* ` ~ */
    case VK_F1:
      render_toggle_cheat_overlay();
      break;
    case '1':
    case VK_NUMPAD1:
      push_cheat(input, CHEAT_IMMORTAL);
      break;
    case '2':
    case VK_NUMPAD2:
      push_cheat(input, CHEAT_MAX_SPEED);
      break;
    case '3':
    case VK_NUMPAD3:
      push_cheat(input, CHEAT_FLY);
      break;
    default:
      break;
  }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {
  Platform *p = g_platform;
  switch (msg) {
    case WM_CLOSE:
      if (p != NULL) {
        p->running = 0;
      }
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (p != NULL && (lParam & (1L << 30)) == 0) {
        handle_vk(wParam, &p->pending);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    default:
      return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
}

int platform_init(Platform **out, int width, int height, const char *title) {
  Platform *p;
  WNDCLASSA wc;
  RECT rc;

  if (out == NULL || width <= 0 || height <= 0) {
    return 0;
  }

  p = (Platform *)calloc(1, sizeof(Platform));
  if (p == NULL) {
    return 0;
  }

  p->width = width;
  p->height = height;
  p->running = 1;
  p->pending = empty_input();

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.lpszClassName = "MetroRushClass";
  wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
  wc.style = CS_OWNDC;
  if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    free(p);
    return 0;
  }

  rc.left = 0;
  rc.top = 0;
  rc.right = width;
  rc.bottom = height;
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

  p->hwnd = CreateWindowA(
      "MetroRushClass", title != NULL ? title : "Metro Rush",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
      rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, wc.hInstance, NULL);
  if (p->hwnd == NULL) {
    free(p);
    return 0;
  }

  p->hdc = GetDC(p->hwnd);
  p->pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
  if (p->pixels == NULL) {
    platform_shutdown(p);
    return 0;
  }

  memset(&p->bmi, 0, sizeof(p->bmi));
  p->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  p->bmi.bmiHeader.biWidth = width;
  p->bmi.bmiHeader.biHeight = -height; /* top-down */
  p->bmi.bmiHeader.biPlanes = 1;
  p->bmi.bmiHeader.biBitCount = 32;
  p->bmi.bmiHeader.biCompression = BI_RGB;

  g_platform = p;
  *out = p;
  return 1;
}

void platform_shutdown(Platform *p) {
  if (p == NULL) {
    return;
  }
  if (g_platform == p) {
    g_platform = NULL;
  }
  if (p->hdc != NULL && p->hwnd != NULL) {
    ReleaseDC(p->hwnd, p->hdc);
    p->hdc = NULL;
  }
  if (p->hwnd != NULL) {
    DestroyWindow(p->hwnd);
    p->hwnd = NULL;
  }
  if (p->pixels != NULL) {
    free(p->pixels);
    p->pixels = NULL;
  }
  free(p);
}

int platform_poll(Platform *p, InputState *input) {
  MSG msg;
  if (p == NULL || input == NULL) {
    return 0;
  }

  *input = p->pending;
  p->pending = empty_input();

  while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      p->running = 0;
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  /* Merge any keys that arrived during DispatchMessage. */
  if (p->pending.count > 0) {
    int i;
    for (i = 0; i < p->pending.count && input->count < MAX_COMMANDS; i++) {
      input->commands[input->count++] = p->pending.commands[i];
    }
    p->pending = empty_input();
  }

  return p->running;
}

void platform_present(Platform *p, const Framebuffer *fb) {
  const uint32_t *src;
  if (p == NULL || fb == NULL || fb->pixels == NULL || p->hdc == NULL) {
    return;
  }
  src = fb->pixels;
  if (src != p->pixels) {
    int n = p->width * p->height;
    int count = fb->width * fb->height;
    if (count > n) {
      count = n;
    }
    memcpy(p->pixels, src, (size_t)count * sizeof(uint32_t));
    src = p->pixels;
  }

  StretchDIBits(p->hdc, 0, 0, p->width, p->height, 0, 0, p->width, p->height,
                src, &p->bmi, DIB_RGB_COLORS, SRCCOPY);
}

double platform_time_seconds(void) {
  return (double)GetTickCount() * 0.001;
}

void platform_sleep_ms(int ms) {
  if (ms > 0) {
    Sleep((DWORD)ms);
  }
}

#endif /* METRO_WIN32 */
