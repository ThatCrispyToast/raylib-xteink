# Example applications

Both examples build with the [desktop emulator](emulator.md) and run on the [physical X4](hardware.md). In the emulator, use Escape or Backspace for Back and `P` for Power.

| Input | Paper | Tetris |
|---|---|---|
| Left / Right | Move across the puzzle grid | Move the piece |
| Up / Down | Move through the grid or menu | Rotate / soft-drop |
| Enter | Open the puzzle, flip a cell, confirm | Start, hard-drop, confirm |
| Back | Open or leave the menu | Pause or return |
| Power | Clean the display | Clean the display |

Full cleans happen on startup/wake and an explicit Power press. Navigation and idle time do not schedule extra flashes. A clean returns to the current screen.

## Paper

```sh
./build/x4_demo
```

Paper combines a raylib drawing showcase with Lights Out. Press Enter to open the puzzle. Selecting a square flips it and its orthogonal neighbors; turn every square off to solve the board.

Back opens a menu with undo, restart, new puzzle, help, device status, and sleep. Restart and new puzzle require confirmation. New puzzles are generated from legal moves, so they are solvable.

### Saves and recovery

Paper stores the board, cursor, starting puzzle, move count, and last 64 undo moves in `raylib-x4/paper-a.sav` and `paper-b.sav` on the SD card. Alternating checksummed records preserve the previous save if a write is incomplete. A valid legacy `paper.txt` is imported without modifying it; unrelated files are preserved.

Moves, undo, confirmed restart/new puzzle, and Sleep save progress. Cursor movement alone does not write the card. Save failures and no-card play are shown below the puzzle; without storage, progress lasts only for the current session. On hardware, Power wakes the device and resumes a saved game.

## Tetris

```sh
./build/x4_tetris
```

Choose **Normal** or **Practice** with Up/Down on the title screen, then press Enter. The game has a 10 × 20 board, shuffled seven-piece bags, three upcoming pieces, a landing outline, clockwise rotation, soft/hard drops, scoring, levels, pause, restart, and sleep.

Normal mode falls automatically. Practice waits for input and disables automatic locking; Enter places the piece. The pause menu offers restart, mode switching, help, and sleep. Restart and sleep require confirmation. Games stay in memory and are not saved to the SD card.

### Movement and timing

Holding Left/Right repeats after 180 ms at 120 ms intervals; Down repeats at 60 ms intervals. Repeats catch up across refreshes, bounded to four sideways or twelve downward steps per update. Releasing stops repeats.

Normal mode starts at 0.8 seconds per row and accelerates to a 0.12-second minimum. At higher levels, several rows may advance in one display update. Pausing, cleaning, and showing a new piece freeze the clock. A grounded piece gets 0.55 seconds of visible landing time, with at most 15 movement/rotation resets before locking.

Ordinary airborne movement can process input during a display update. New pieces, grounded pieces, dialogs, and page transitions wait for presentation. These safeguards keep a piece from locking or a dialog from being confirmed before it is shown. Physical refresh costs are documented in [performance](performance.md).

### Scoring and rotation

Clearing one, two, three, or four rows scores 100, 300, 500, or 800 points, multiplied by the level before the clear. Soft/hard drops add one/two points per row. Every ten cleared rows raises the level, capped at 20.

Rotation uses wall/floor position adjustments rather than the full SRS rule set. Hold pieces and T-spin/combo bonuses are outside this example.

### Source and tests

The application is [tetris.c](../examples/tetris.c); its portable game model is [tetris_game.c](../examples/tetris_game.c). Run `ctest --test-dir build -R tetris --output-on-failure` for gameplay, retained rendering, and SDL journeys. [tetris_device.c](../tests/tetris_device.c) exercises the actual app with deterministic input on hardware. See [hardware setup](hardware.md#build-and-switch-to-tetris) to build its firmware.
