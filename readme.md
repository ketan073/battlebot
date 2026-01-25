# BattleBot Engine

A 20-round survival game where 5 bots compete for water through sealed-bid auctions.

> **Note**: The engine uses POSIX APIs (fork, pipes) and runs only on **Linux** or **macOS**. Windows users should use WSL (Windows Subsystem for Linux) or a Linux VM.

## Quick Start

```bash
make
make run
```

Or compile manually:
```bash
g++ -std=c++17 -O2 -o engine engine.cpp
g++ -std=c++17 -O2 -o sample_bot sample_bot.cpp
./engine ./sample_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot
```

## Creating Your Bot

1. Copy `sample_bot.cpp` -> `my_bot.cpp`
2. Write your strategy
3. Compile: `g++ -std=c++17 -O2 -o my_bot my_bot.cpp`
4. Run: `./engine ./my_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot`
5. (Optional) Edit the `run` target in `Makefile` to use your bot paths

## Running Different Bots

```bash
./engine ./bot1 ./bot2 ./bot3 ./bot4 ./bot5
```

## Replay Viewer

Visualize game replays with charts, player stats, and timeline controls.

**Build and run viewer:**
```bash
make viewer          # First time: builds the viewer (fetches dependencies)
make run-viewer      # Run the viewer
```

Or run directly:
```bash
./viewer/build/viewer
```

Click the "Load Replay" button to open a file dialog and select any `.json` replay file.

**Requirements:** `zenity` (usually pre-installed on Ubuntu/Debian). If missing: `sudo apt install zenity`

## Windows Users

The engine does not run natively on Windows. Use one of:
- **WSL** (Windows Subsystem for Linux) - recommended
- **Linux VM** (VirtualBox, VMware, etc.)
- **Dual-boot Linux** - install Ubuntu/Mint alongside Windows

See `BattleBot.pdf` for game rules and I/O format.
