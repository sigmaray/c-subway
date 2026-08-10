# Metro Rush (C)

C99 port of the [Metro Rush](../ts-subway) endless runner. Software-rendered 3D, no third-party libraries.

## Platforms

| Target | Command | Notes |
|--------|---------|--------|
| Linux | `make` | Needs system `libX11` (no other third-party libs) |
| Windows 98+ | `make win98` | MinGW i686 PE, subsystem 4.0, GDI blit |

Needs an X server on Linux (`DISPLAY`). On Windows uses GDI only (`user32` / `gdi32` / `kernel32` / `msvcrt`).

## Build

```bash
make              # Linux (libX11)
make win98        # requires i686-w64-mingw32-gcc
```

## Controls

- `A` / `←` — left lane
- `D` / `→` — right lane
- `W` / `↑` / `Space` — jump
- `S` / `↓` — slide
- `P` / `Esc` — pause
- `Enter` — start / restart
- `M` — mute
- `` ` `` / `F1` — cheat overlay; `1`–`7` cheat actions

## Layout

- `include/metro.h` — game types and API
- `src/systems_*.c` — gameplay (ported from TypeScript)
- `src/render.c` — software renderer + HUD
- `src/platform_x11.c` / `platform_win32.c` — OS backends
