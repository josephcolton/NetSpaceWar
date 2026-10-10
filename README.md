# NetSpaceWar
Network based Space War: a multiplayer clone of Spacewar! Fly around a looping universe, dodge planets, and shoot down other players and AI bots.

## Building

NetSpaceWar uses CMake. From the repository root:

```sh
cmake -S . -B build
cmake --build build
```

This builds three programs in `build/`:

| Program | What it is | Needs |
|---|---|---|
| `SpaceWarServer` | Game server | Nothing beyond a C compiler |
| `SpaceWarAI` | Computer-controlled player, no window | Nothing beyond a C compiler |
| `SpaceWarClient` | The game window for human players | SDL3 (downloaded automatically) |

If SDL3 isn't installed, CMake downloads it and links it into the client, so players don't need to install anything. Building SDL3 on Linux needs the X11 and Wayland development headers. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxi-dev libxss-dev libxkbcommon-dev libwayland-dev libegl1-mesa-dev
```

To build only the server and AI, configure with `-DNSW_BUILD_CLIENT=OFF`.

### Windows

The Windows programs are built with MinGW-w64, cross-compiled from Linux (`sudo apt install mingw-w64`, or [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)):

```sh
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-windows
```

These builds link SDL3 and the C runtime statically, so each `.exe` runs on its own on Windows 10 and later. The network protocol is the same on every platform, so Linux and Windows players can join the same game.

Building natively on Windows with MSVC should also work through CMake, but it hasn't been tried yet.

## Playing

Start a server, then connect clients and bots to it (from `build/`):

```sh
./SpaceWarServer <hostname> <port>
./SpaceWarClient <hostname> <port> <username>
./SpaceWarAI     <hostname> <port> <username>
```

For example, a local game against two bots:

```sh
./SpaceWarServer localhost 27960 &
./SpaceWarAI localhost 27960 bot1 &
./SpaceWarAI localhost 27960 bot2 &
./SpaceWarClient localhost 27960 player
```

The AI hunts the nearest ship, leads its shots, steers clear of planets, and moves power between shields and energy. It plays under the same turn, thrust and fire limits as a human.

### Controls

| Key | Action |
|---|---|
| Left / Right arrow | Turn |
| Up arrow | Thrust |
| Space or Left Ctrl | Fire missile |
| Tab | Hyperjump (also respawns after you die) |
| Z | Move energy to shields |
| X | Move shields to energy |
| Escape | Quit |

The window can be resized; the game scales to fit.

### Rules

- Missiles cost 2 energy. Energy does not recharge on its own, but shields slowly do, so move power between them with Z and X.
- A missile hit takes 25 shields. Below zero shields, your ship explodes.
- Planets pull you in with gravity, and touching one does heavy damage.
- The universe wraps around at the edges.
- Scoring: 10 points for a hit, 100 for a kill, and -100 for dying.
