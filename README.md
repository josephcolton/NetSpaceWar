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
| `NetSpaceWar` | The game: menus and the game window | SDL3 (downloaded automatically) |
| `SpaceWarServer` | Game server | Nothing beyond a C compiler |
| `SpaceWarAI` | Computer-controlled player, no window | Nothing beyond a C compiler |

Keep all three in the same folder: the game starts the server and AI bots from its own folder when you host.

If SDL3 isn't installed, CMake downloads it and links it into the game, so players don't need to install anything. Building SDL3 on Linux needs the X11 and Wayland development headers. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxi-dev libxss-dev libxkbcommon-dev libwayland-dev libegl1-mesa-dev
```

To build only the server and AI (for example on a headless machine), configure with `-DNSW_BUILD_CLIENT=OFF`.

### Windows

The Windows programs are built with MinGW-w64, cross-compiled from Linux (`sudo apt install mingw-w64`, or [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)):

```sh
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-windows
```

These builds link SDL3 and the C runtime statically, so each `.exe` runs on its own on Windows 10 and later. The network protocol is the same on every platform, so Linux and Windows players can join the same game.

Building natively on Windows with MSVC should also work through CMake, but it hasn't been tried yet.

## Playing

Run `NetSpaceWar` (on Windows, double-click `NetSpaceWar.exe`).

- **Host Game** starts a game on your computer. Choose your name, a port, and how many AI bots to add (up to 4; a game holds 5 ships). The screen shows the address other players can use to join. On Windows, allow the firewall prompt so they can reach you.
  - **Game Settings** changes the rules for games you host: number of planets, gravity, missile life, top speed, crash and missile damage, and the energy cost of shots and hyperjumps. Changed values are highlighted, and **Reset to Defaults** puts them all back. Everyone who joins, bots included, gets the host's rules: players see them by holding S.
- **Join Game** connects to someone else's game by address and port.

Press Escape during a game to return to the menu. Your choices are remembered for next time.

### From the command line

The programs also run on their own, for example to keep a dedicated server running (from `build/`):

```sh
./SpaceWarServer 0.0.0.0 27960                 # listen on every network interface
./SpaceWarServer 0.0.0.0 27960 --planets 4 --gravity 150   # change the rules
./SpaceWarServer 0.0.0.0 27960 --verbose      # also print every packet (--silent: print nothing)
./SpaceWarAI     <server> <port> <name>         # add a bot
./NetSpaceWar    <server> <port> <name>         # join directly, skipping the menu
```

The server reports when players join and leave, and every death and respawn. Run `SpaceWarServer` with no arguments to list every rule option with its range and default.

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
| S (hold) | Show the scoreboard and the game rules |
| Escape | Leave the game |

The window can be resized; the game scales to fit.

### Rules

- Missiles cost 2 energy. Energy does not recharge on its own, but shields slowly do, so move power between them with Z and X.
- A missile hit takes 25 shields. Below zero shields, your ship explodes.
- Planets pull you in with gravity, and touching one does heavy damage.
- The universe wraps around at the edges.
- Scoring: 10 points for a hit, 100 for a kill, and -100 for dying (scores never go below 0). Hitting yourself scores nothing. Hold S to see everyone's score, kills and deaths; the scoreboard also appears while you wait to respawn.
