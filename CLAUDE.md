# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

NetSpaceWar: a multiplayer UDP network clone of Spacewar!, written in C. Git repo (GitHub `josephcolton/NetSpaceWar`). It was migrated from an old SVN checkout at `/opt/netspacewar-code` (SourceForge, revision 128), which is no longer edited. Sources are in `src/`, with `CMakeLists.txt` at the root.

## Build

```sh
cmake -S . -B build
cmake --build build                          # all targets
cmake --build build --target SpaceWarAI      # one target
cmake -S . -B build -DNSW_BUILD_CLIENT=OFF   # server + AI only
```

- `SpaceWarServer` and `SpaceWarAI` need nothing beyond libm. The AI is headless and does not link `gui.c`.
- `SpaceWarClient` uses **SDL3**. CMake uses an installed SDL3 if found; otherwise it FetchContent-downloads the pinned release and links it statically (`NSW_FETCH_SDL`, default ON). On Linux, SDL's own configure step fails unless X11 or Wayland dev headers are installed (README lists them).
- **Headless testing** (no display, no X11 headers): configure a separate tree with `-DSDL_UNIX_CONSOLE_BUILD=ON` (e.g. `build-headless/`) and run the client with `SDL_VIDEODRIVER=offscreen`. SIGTERM/window-close becomes `SDL_EVENT_QUIT`, which logs out cleanly.
- `OS_<CMAKE_SYSTEM_NAME>` (e.g. `OS_Linux`) is defined for the `#ifdef` blocks in the clients.
- **Windows**: `cmake/mingw-w64-x86_64.cmake` cross-compiles from Linux (needs `x86_64-w64-mingw32-*` on PATH, from distro `mingw-w64` or llvm-mingw). Builds are static, so the `.exe` files need only system DLLs. No Wine is available here, so Windows builds are compile/link-verified only.
- Globals live in the `.c` files and are declared `extern` in the headers. Keep it that way: defining them in headers breaks the link under GCC ≥ 10 (`-fno-common`).
- There are no tests or linters. To smoke-test, run `./SpaceWarServer localhost <port>` and two `./SpaceWarAI localhost <port> botN` from `build/`. With `DEBUG` on, the logs show logins and roughly 20 player/missile updates per second.

## Run

```sh
# from build/
./SpaceWarServer <hostname> <port>
./SpaceWarClient <hostname> <port> <username>
./SpaceWarAI     <hostname> <port> <username>
```

Client keys: arrows turn/thrust, Space or Left Ctrl fires, Tab hyperjumps, Z/X move energy between weapons and shields, Esc quits.

## Architecture

**Authoritative server, thin clients, over UDP.** The server owns all game state and physics. Clients send input actions and render whatever the server broadcasts.

- **Platform layer** (`platform.h`/`platform.c`): all OS differences live here, namely the socket headers, `netInitialize()` (WSAStartup), `netLastError()`, `netIgnoreConnReset()` (turns off Windows' UDP connection-reset errors), and `platformNow()` (a monotonic clock). Other files include it via `global.h`; don't add POSIX-only headers or calls (`bzero`, `gettimeofday`, `errno` for socket errors) elsewhere. Sockets are stored as `int`, even on Windows. `sendto` buffers are cast to `(const char *)` for Winsock.
- **Protocol** (`global.h`): every packet is a `packed` struct whose first field is `message_t mtype` (`MSG_*` constants). Receivers cast the raw buffer to `struct msg_basic` to dispatch, then cast to the specific `msg_*` struct. Variable-length messages (`msg_updateplayers`, `msg_updatemissiles`, `msg_staticobjs`, `msg_clientlist`) carry `obj_count` plus a trailing zero-length array of `dataNode*` structs, and are received into `MAX_PACKET` (64 KB) buffers, since a full missile list is about 2.6 KB. Wire structs use fixed-width types inside `#pragma pack(push, 1)`, and `_Static_assert`s at the end of `global.h` pin their sizes. Update the asserts deliberately when changing the protocol. `msg_clientlist` sends `struct wireClient`, not the in-memory `dataNodeClients`. Client handlers clamp `obj_count` to the `MAX_COUNT_*` limits, and receivers drop packets shorter than a `message_t`. The client sends input as `msg_clientupdate` with an `ACTION_*` code and generic `var1/var2/var3` fields; their meaning depends on the action (for THRUST, var1 = accel and var2 = angle delta; for TRANSPWR, var3 = energy). All tuning constants and capacity limits (`MAX_COUNT_*`) are also in `global.h`.
- **Naming convention**: `shandle_msg_*` / `ssend_msg_*` (server, `server.c`) and `chandle_msg_*` / `csend_msg_*` (client, `client.c`). Adding a message means adding a `MSG_*` constant, a struct in `global.h`, a send/handle pair, and an `if (mtype == …)` line in the `handle_socket()` dispatcher of `SpaceWarServer.c` or `SpaceWarClient.c`/`SpaceWarAI.c`.
- **Server loop** (`SpaceWarServer.c`): `select()` on one UDP socket, with a timeout of `SERVER_TIMEOUT` (tick = `PHYS_SECS_PER_UPDATE`, 50 ms). `lastTimeLeft()` in `sdata.c` subtracts time spent handling packets, and the timeout branch resets `lastTime` so each tick is timed from when it fired. On each tick it runs `checkClientKeepAlive()`, `sdataUpdateAll()`, `physicsUpdate()` (positions, gravity, collisions, shield restore, and scoring in `physics.c`), then `broadcastUpdatePlayers/Missiles()`. Clients are identified by their source `sockaddr_in`.
- **State stores**: `sdata.c` (server) and `cdata.c` (client) mirror each other. Each keeps a fixed-size array of pointers per object type (`*ArrayClients/Ships/Missiles/Planets/Explosions`) plus a count, with Create/Delete/Update/Lookup-by-`objectID` functions. Object IDs come from `sdataNewObjectID()`. Space is a torus from -100 to 100 on both axes (`xcoordLoop`/`ycoordLoop`).
- **Client** (`SpaceWarClient.c`): a `select()` loop with a 100 ms timeout that runs `handle_socket`, then `gui_update_display()` and `gui_get_input()` (SDL3, `gui.c`), and converts input into `csend_msg_clientupdate` calls. `gui.c` draws at a 640x480 logical size that SDL letterboxes into the resizable window. Its drawing code still uses the original Allegro-style calls (`gui_line`, `gui_circle`, `gui_putpixel`, `gui_makecol`…), implemented as small SDL wrappers at the top of the file over a `GuiArea` (a rectangle of the window, used for the main view and the radar). `SDL_PollEvent` runs only in `gui_get_input()`. The `UGLY_MISSILE_DRAW_POS_FIX` flag redraws only after both a players packet and a missiles packet have arrived.
- **AI** (`SpaceWarAI.c` + `ai.c`): a copy of the client main loop with the GUI removed and `clientAIupdate()` added. It links `client.c`/`cdata.c` but not `gui.c`. `clientAIupdate()` runs once per `MSG_UPDATEPLAYERS` (not every loop pass), so it always acts on fresh state. It works by choosing a wanted velocity (close in, hold position, or flee a planet using a course lookahead) and thrusting along the difference; otherwise it turns to a lead-intercept angle and fires. It deliberately limits itself to the human client's `P_GUI_*` turn/accel/fire rates, because the server doesn't enforce them. Tuning constants (`AI_*`) are in `ai.h`.

## Roadmap

- **GUI launcher**: the SDL client should become a normal game front end. It should host a server, choose a number of AI bots, and let others join. The plan is to spawn `SpaceWarServer`/`SpaceWarAI` as child processes with `SDL_CreateProcess`, and draw menu text with `SDL_RenderDebugText`. Keep the server and AI dependency-free for that reason.

## Notes

- `physicsUpdate()` must call `deadShipDetect()` last. If it runs first, ships sit at negative shields for a tick and a power transfer or shield restore can revive them.
- Collision scoring in `physics.c` looks swapped: a kill awards `POINTS_HIT` and a non-fatal hit awards `POINTS_KILL`.
- `DEBUG` is hard-coded to 1 in `global.h`, so the server and clients print every packet they receive.
