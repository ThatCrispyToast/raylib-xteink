# Independent user acceptance report

Baseline: `6f7168f` (`Add hardware-validated raylib target for Xteink X4`). Date: 2026-10-26. The tester acted as a first-time Paper user and a developer integrating the target. The baseline findings below are historical; the follow-up section records the completed retest of the QA fixes.

## Method and boundaries

A clean `git archive` was extracted to `/tmp/x4-user-qa/baseline`. The pinned microreader emulator was fetched using the README command, and the project was built with its declared Nix environment in `/tmp/x4-user-qa/build`. Tests used microreader's actual SDL runtime and display, `SDL_VIDEODRIVER=offscreen`, independent virtual SD directories, scripted SDL key events, and screenshot readback. Screenshots were visually inspected. Most cases used `X4_EMULATOR_FAST=1`, so they assess controls, state, rendering, and persistence rather than realistic panel latency.

No physical buttons were pressed by this tester, no firmware was flashed, and the physical reader was not accessed. Deep sleep, physical button ergonomics, battery behavior, and analog gray appearance therefore remain outside this independent pass. The parent agent has separate hardware evidence.

Reproduction harness and individual inputs, logs, SD directories, and screenshots are under `/tmp/x4-user-qa/`. They are temporary local evidence, not a dependency of the product.

## Feature satisfaction

| User goal | Baseline result | Evidence |
|---|---|---|
| Build from a clean source archive | Pass | Emulator fetch and Release build succeed with README's Nix workflow. |
| Discover controls and read the screen | Partial | Text and selection outline are legible. The puzzle never explains that selecting a square toggles its orthogonal neighbors. |
| Move, play, and solve | Pass | Enter changes the board, cursor wraps in both directions, and the five-move solution produces `0 5` in the save file and a solved message. |
| Recover from a mistaken move | Incomplete | Repeating the same selection reverses the board but increases the move count; no undo exists. |
| Play another puzzle or restart | Incomplete | A solved board survives relaunch, with no new-game or restart action. |
| Save and resume progress | Partial | Board and move count survive relaunch, but the app returns to the opening page and resets the selected cell. |
| Continue without writable storage | Partial | A read-only directory permits play, but the puzzle does not reveal that moves are unsaved. Invalid storage-root paths prevent startup. |
| Handle unrelated or invalid existing files | Fail | An existing unrelated `paper.txt` is overwritten after one move; a directory with that name aborts startup. |
| Clean the panel without losing the game | Pass | P requests one additional full refresh and preserves the current page and board. |
| Exit | Pass in emulator | Escape closes normally. The retained final screenshot is not evidence of hardware sleep behavior. |
| Integrate a small custom native application | Pass with documentation gap | A separate CMake consumer using `add_subdirectory`, `raylib::x4`, and `x4_emulator` builds and renders the minimal example with three counted Enter presses. README lacks a complete native consumer recipe. |

## Reproduced findings

### QA-01 — High: a directory at the save filename aborts startup

1. Create an empty virtual SD directory.
2. Create a directory named `paper.txt` inside it.
3. Launch Paper with `X4_SD_ROOT` pointing to that virtual SD directory.

Observed: process exits with SIGABRT (`-6` in Python) and `*** buffer overflow detected ***: terminated`, before presenting the opening screen. Expected: reject an unreadable/nonregular save safely, show an actionable storage warning, and continue without erasing anything.

Evidence: `saveblocked-puzzle/log` and `saveblocked-status/log`. Inspection after reproduction identifies unchecked `fseek`/`ftell` failure in the public upstream `LoadFileText()` path: `-1` is cast to an unsigned length, and allocating `size + 1` wraps. This merits a core file-API guard, not only a Paper workaround.

### QA-02 — High: a generic save name overwrites unrelated user data

1. Write `Personal notes that are not Paper save data.\n` to the virtual SD root's `paper.txt`.
2. Start Paper, press Right, then Enter.

Observed: the notes are silently replaced by `7549310 1\n`. Expected: use an application-specific directory and versioned save format; treat an incompatible existing file as a conflict or import only validated legacy data. An SD card commonly contains unrelated reader documents.

Evidence: `foreign-overwrite/`, and the original value is recorded in `scenarios.py`.

### QA-03 — Medium: unsaved play looks successful

1. Give the virtual SD directory mode `0555` as the normal non-root user.
2. Visit the status page before moving.
3. Visit the puzzle and press Enter.

Observed: status initially promises “SD ready. Moves save automatically.” The move fails to save; the gameplay screen offers no indication. Only navigating to the separate status page reveals “Save failed.” Expected: a persistent, concise warning on the active game screen, distinguishing “no card,” “unsaved,” and “saved.”

Evidence: `read-only-before/frame.png`, `read-only-failure-puzzle/frame.png`, and `read-only-failure-status/frame.png`.

### QA-04 — Medium: the playable demo has no replay or recovery workflow

1. Navigate to the puzzle.
2. Select cells 0, 3, 6, 12, and 21 (zero-based row-major order), using Down between cells.
3. Observe the solved message, then attempt to restart through the documented controls or relaunch the app.

Observed: the solved board is persisted, and no restart/new-puzzle control exists. P only cleans; navigation only changes page/cursor. Enter can disturb the solved board but does not establish a new puzzle or reset the score. Likewise, a mistaken move cannot be undone without increasing the count.

Expected: discoverable actions for undo, restart, and a fresh solvable puzzle, with protection against accidental destructive resets. The first gameplay screen should state that Enter flips the selected square and its neighbors. The current sequential cursor scheme is documented but requires up to 24 individual cell moves; a deliberate navigation/action design should account for e-paper refresh cost.

Evidence: `solve/frame.png`, `solved-reopen/frame.png`, and `solved-reset-attempt/`.

### QA-05 — Medium: invalid save values are accepted or silently discarded

Two independently reproduced cases:

- A save containing `8388607 4 garbage\n` is treated as valid, despite trailing junk.
- A save containing `7549277 2147483647\n` loads; one move writes `7549310 -2147483648\n`. On the next launch, the negative count causes the state to be silently ignored and the puzzle appears reset.

Expected: bounded, strict parsing of the full record, overflow-safe counters, and an explicit recovery notice for invalid state. These are corrupted/manual-save edge cases rather than ordinary gameplay duration.

Evidence: `corrupt-tail/`, `maxturn/`, and `maxturn-reopen/`.

### QA-06 — Medium: the emulator cannot model absent storage through an invalid root

1. Create a regular file and set `X4_SD_ROOT` to that file.
2. Launch Paper.

Observed: emulator initialization fails with “cannot create directories: Not a directory”; exit code 1. Expected: clear behavior for an unavailable virtual card, ideally a documented explicit no-card mode, while preserving the hardware promise that Paper works without SD storage.

Evidence: `storage-file/log`. A missing ordinary directory is automatically created, so simply pointing at a nonexistent directory does not test the no-card path.

### QA-07 — Low: four-gray and power behavior are hard to infer on the device

The fourth grayscale swatch visually merges into the page background. Outline the swatches or label them to make all four values perceptible. The footer says `P CLEAN` and `BACK EXIT`, which assumes emulator key names. Device users would benefit from “Power: clean” and “Back: sleep,” with the wake method made visible before or during sleep. README does explain that a cleaning flash returns to the existing page and that e-paper retains its image in sleep, but that explanation is not available from the reader itself.

Evidence: `welcome/frame.png`; hardware-specific clarity is assessed from text, not a new physical test.

## Custom-application workflow

The following separate consumer project worked against the clean baseline archive:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_x4_app C CXX)
add_subdirectory(/absolute/path/to/raylib-x4 x4)
add_executable(my_x4_app /absolute/path/to/raylib-x4/examples/minimal.c)
target_link_libraries(my_x4_app PRIVATE raylib::x4 x4_emulator)
```

It was configured with `BUILD_TESTING=OFF` and built in `/tmp/x4-user-qa/custom-build`. Three Enter presses produced “3 button presses” with a clean exit. A documented native `X4_APP_SOURCE` option would simplify trying the same application on emulator and hardware.


## Follow-up user acceptance pass

The revised 1.1.0 application was tested with the same actual microreader SDL runtime, isolated virtual SD directories, and screenshot inspection. The tested emulator binary SHA-256 was `b8d3cd749127d86af4082704f43f69986ee27464cbb59074ef72d5b555f50c70`. The current workspace `build/x4_demo` was used for application scenarios; a separate `/tmp/x4-user-qa/native-custom-build` rebuilt the target for the custom-application check. The original clean archive and baseline evidence remain separate.

All seven reproduced baseline findings have been addressed in this pass. No remaining functional blocker was found in the tested user journeys.

| Finding / goal | Retest result |
|---|---|
| QA-01: save filename directory | Pass. The old `paper.txt` directory remains intact; Paper starts and can save a new journal without crashing. |
| QA-02: unrelated file preservation | Pass. An unrelated `paper.txt` remains byte-identical after play. A valid legacy save loads and migrates into the new directory, preserving the old file. |
| QA-03: save feedback | Pass. Read-only storage and a regular file blocking the application directory both permit play and show `NOT SAVED. Check SD card.` on the puzzle. No-card play is also explicitly labeled. |
| QA-04: recovery and replay | Pass. Grid arrows move spatially. Undo restores the board and decrements the move count, including after relaunch. Undo with no history gives a clear notice. Restart cancellation preserves the game; confirmation restores its initial board with zero moves. A fresh puzzle was solved using an independent row-chasing solver, then undone and replaced through the menu. |
| QA-05: malformed and extreme saves | Pass. Trailing junk and an out-of-range count are rejected with an invalid-save notice. The old signed maximum increments to 2147483648 safely. UINT32_MAX refuses a further move and directs the user to Restart without changing the save. |
| QA-06: absent/bad storage | Pass. `X4_SD_ROOT=''` and a root that is a regular file both allow gameplay without persistence. The no-card message also remains visible on the sleep screen. |
| QA-07: grayscale and power clarity | Pass for emulator visual/text checks. All four swatches are outlined and labeled. Help explains Power cleaning, and the sleep screen explains retained imagery, waking with Power, and the normal wake flash. Physical appearance and actual wake behavior were not independently retested here. |
| Resume | Pass. The saved puzzle opens directly with its board, move count, and cursor. Moving the cursor and choosing Sleep also persists the new cursor position. |
| Interrupted save recovery | Pass. After two versions were saved, the newer slot was truncated to 15 bytes. Relaunch restored the prior board/count/cursor and displayed `Recovered the previous valid save.` A later move successfully replaced the broken slot. Corrupting both slots produced an explicit invalid-save notice and a fresh game. |
| Native custom application | Pass. `X4_APP_SOURCE` plus `X4_APP_EXTRA_SOURCES` compiled a separate two-file app and rendered `Custom modules work.` Its three applicable CTest checks passed. The README now includes complete native selection and external-consumer recipes. |
| Screenshot workflow | Pass. `scripts/capture.py` now produces the intended puzzle and status pages with its documented page arguments. |

A further play → clean → undo scenario ran with microreader’s normal refresh delays enabled and exited cleanly with nine frames, two full refreshes, seven partial updates, and zero errors. Scripted input is still injected at poll boundaries, so this does not test physical presses during blocking panel refreshes.

The revised welcome, puzzle, solved, menu, help, device-status, save-failure, recovery, and sleep screens were visually inspected at the emulator's 480 × 786 output size. Text, control hints, and cursor outlines were readable without clipping. The maximum-count warning also fits. The menu provides a complete replay/recovery workflow without requiring undocumented key combinations.

Follow-up reproduction scripts are `/tmp/x4-user-qa/retest.py` and `/tmp/x4-user-qa/solve-retest.py`; screenshots and logs are under `/tmp/x4-user-qa/retest/`. The interrupted-save test modifies virtual files between launches; it does not simulate SD-card electrical failure or prove filesystem durability under physical power loss. Likewise, the sleep screen plus process exit tests the application flow, not the hardware's deep-sleep current or wake circuitry.
