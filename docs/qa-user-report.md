# Independent user acceptance report

Baseline: `db93a26` (`Add hardware-validated raylib target for Xteink X4`). Date: 2026-10-26. The tester acted as a first-time Paper user and a developer integrating the target. The baseline findings below are historical; the follow-up section records the completed retest of the QA fixes.

## Method and boundaries

A clean `git archive` was extracted to `/tmp/x4-user-qa/baseline`. The pinned microreader emulator was fetched using the README command, and the project was built with its declared Nix environment in `/tmp/x4-user-qa/build`. Tests used microreader's actual SDL runtime and display, `SDL_VIDEODRIVER=offscreen`, independent virtual SD directories, scripted SDL key events, and screenshot readback. Screenshots were visually inspected. Most cases used `X4_EMULATOR_FAST=1`, so they assess controls, state, rendering, and persistence rather than realistic panel latency.

No physical buttons were pressed by this tester, no firmware was flashed, and the physical reader was not accessed. Deep sleep, physical button ergonomics, battery behavior, and analog gray appearance therefore remain outside this independent pass. Hardware evidence is recorded separately in [validation](validation.md).

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

All seven reproduced baseline findings were addressed in this functional pass. It did not establish acceptable hardware responsiveness: the owner subsequently reported severe sluggishness, prompting the wall-clock acceptance pass below.

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


## Wall-clock input and responsiveness follow-up

The owner's report of sluggish physical interaction exposed a gap in the previous acceptance method. Poll-index input scripts waited until the application polled before injecting the next event. They therefore did not represent a person tapping while rendering or display refresh was blocked. Even the earlier normal-delay check used this readiness-dependent input. The desktop emulator also renders the requested image before applying a fixed 400 ms delay for both full and partial refresh; it does not reproduce ESP32 rendering cost or the physical waveform's visible progression.

### Reproduced timing failures

A separate pthread now injects real SDL key events at absolute monotonic-clock deadlines while the application is blocked. Against the preserved 1.1 build, only two of the initial eight timing checks passed:

- Four Right taps 150 ms apart moved three cells, losing a tap.
- A mixed Right → Enter → Down → Enter sequence lost a flip.
- Back → Down → Down → Enter selected Undo instead of opening Restart confirmation.
- Power → Back → Down → Enter during a refresh lost menu actions.
- Held navigation was limited by refresh cadence; the test does not claim the configured repeat interval is an independently guaranteed physical rate.

The first queue fix passed those eight cases. A further test caught Back being discarded during the first confirmation refresh because the confirmation guard drained every queued key. The final guard preserves cancellation while rejecting Enter events buffered before the confirmation is ready.

### Reproducible acceptance test

Run the checked-in test from the native development shell:

```sh
python3 scripts/test-input-timing.py build --output /tmp/x4-input-timing
```

The script compiles `tests/timed_sdl_input.c` into a temporary preload library using the declared SDL2 toolchain. Each case has independent seeded SD storage. Normal emulator refresh delays remain enabled, and selected cases extend one refresh pause to 1.6 seconds. Logs record input and presentation times; saved records verify state; actual SDL screenshots are compared against slowly paced reference journeys. `--output` preserves a separate run directory containing inputs, logs, screenshots, and `results.json`.

The final retained-redraw build, emulator SHA-256 `30382f9c61b36dcf3787b6b54214ca5f66ba76f02a4600fd32caf08480d9b701`, passed **9/9 checks**:

| Timing goal | Result |
|---|---|
| Four rapid identical taps | All four movements retained. |
| Mixed movement and flip order | Both flips apply to their intended cells, in order. |
| Rapid menu selection | Intended Restart confirmation appears. |
| Enter buffered before/during confirmation refresh | Existing game remains unchanged; confirmation stays visible. |
| Back during confirmation refresh | Returns to the menu without changing the game. |
| Fresh confirmation after the dialog | Restart still works. |
| Menu input during a 1.6-second clean pause | Back, Down, Enter execute Undo correctly. |
| Release during a long clean pause | The original press survives; stale held-key repeats are not replayed after release. |
| Ordinary held navigation | Repeats occur and movement settles within one normal refresh after release. |

Separate timing runs verified the idle-clean policy. Thirteen cursor moves plus one flip produced 14 partial updates and only the initial full refresh while active. Continuing the same run without input produced exactly one additional full clean at 9.815 seconds, about three seconds after the last action at 6.800 seconds. The final game state was identical. This removed the earlier automatic full-clean interruption on the thirteenth navigation update. The owner later requested fewer flashes, and release 1.1.2 supersedes this idle-clean policy with the manual-clean policy tested below.

### Retained drawing correctness

All 25 cursor positions were reached through a continuous snake traversal and compared byte-for-byte with fresh full renders at the same positions. All eight menu selections were compared against the preserved full-redraw implementation. **All 33 comparisons were identical.** Representative dark-cell, white-cell, and final-menu screenshots were also visually inspected; no stale selection outlines or erased labels were found.

Local evidence is under `/tmp/x4-user-qa/timing-baseline/`, `/tmp/x4-user-qa/timing-retained/`, and `/tmp/x4-user-qa/latency/`. The latter includes the separate idle-clean traces and visual comparison results.

### Physical latency acceptance boundary

These results establish input preservation, ordering, confirmation safety, held-key release behavior, and retained-render correctness under controlled pauses. They do not establish the physical reader's end-to-end response time. Physical firmware measurements are recorded separately in [performance](performance.md).

A physical latency run should timestamp the debounced press, delivery to Paper, completion of state changes and SD sync, drawing, framebuffer conversion, SPI transfer, and completion of panel BUSY. Report ordinary cursor/menu navigation, Enter with writable SD storage, no-card Enter, and full cleaning separately. Include rapid taps, held navigation/release, and at least fourteen consecutive updates so periodic-clean behavior is exercised. Draw timing alone excludes the synchronous save that precedes rendering. Physical acceptance should disclose the actual median, upper-percentile, and worst response times rather than infer responsiveness from correct final screenshots or native frame counts.


The final 1.1.1 release rebuild (`76f4dff3cd0cc2ff53ab1da7803a4e56fe2e657f340e697a435b376e080e08de`) also passed the complete seven-test CTest suite, including all nine wall-clock scenarios. Physical measurements and their scope are now recorded separately in [the performance report](performance.md); ordinary selection updates measured about 0.73 seconds, with the unchanged panel waveform accounting for most of that time.


## Manual-clean policy and retained puzzle updates (1.1.2)

The owner requested fewer full-screen flashes and further speed improvements. Paper now leaves cleaning to its initial display setup and an explicit Power press. Cursor movement, menu navigation, idle time, and the Sleep screen do not request full cleans. The existing `POWER: CLEAN` footer makes recovery from visible ghosting discoverable. This policy applies to Paper; the generic target's `X4SetRefreshInterval()` contract and default remain unchanged.

The checked-in `scripts/test-emulator.py` now enforces this behavior:

| Scenario | Observed result |
|---|---|
| 64 grid moves, 64 menu moves, then long idle | 131 actual partial presentations, exactly one initial full refresh, and 6.2 seconds with no further presentation. |
| The same puzzle with one Power press | Exactly one additional full refresh; final pixels identical to the run without Power. Another 6.2 seconds idle caused no extra presentation. |
| Choose Sleep after a move | No full refresh beyond initialization; board, cursor, move count, and undo history saved successfully. |
| Relaunch the saved game | Exactly one initial full refresh, no partial refreshes, and the saved state preserved. |

The tests check the age of the last screenshot as well as refresh counters, so changing the application's idle-poll interval cannot silently turn the idle scenario into a short pause. The preserved prior build failed the long-navigation test with two full refreshes; the updated policy passed in the native CTest run. These are emulator assertions about refresh requests and application behavior, not photographs or electrical measurements.

A separate new test, `scripts/test-paper-rendering.py`, checks the expanded retained drawing path for game moves. It compares each final retained puzzle image with the same seeded journey followed by Back → Back, which forces the complete puzzle page to redraw without altering the game. Each pair starts from independent identical storage, and save records must remain identical. The script copies the tested executable before starting, keeping all comparisons on the same binary.

```sh
python3 scripts/test-paper-rendering.py build --output /tmp/x4-paper-rendering
```

**All 47 comparisons passed with byte-identical screenshots and unchanged save records.** Coverage includes a flip at every one of the 25 cells, four transitions into the solved state, leaving and navigating a solved board, repeated and overlapping flips, undo and menu transitions, clearing notices, move-counter width changes, the uint32 move limit, save failure, recovery-status changes, and no-card play. The save-failure fixture blocks the alternate journal with a directory, making the failure deterministic without relying on user permissions. Representative solved, unsolved, move-limit, and failed-save screens were also visually inspected and remained clear.

The tested retained-puzzle emulator SHA-256 was `b1e515e6e4fb53eb9003bc5656a067c599ad262e193738f931d5b64ff1689d90`. Local comparison evidence is `/tmp/x4-user-qa/flip-rendering/run-j1gg07c0/`; policy results are recorded in the native CTest log. Physical response-time measurements and the extended partial-refresh soak are described in [performance measurements](performance.md). This independent pass did not access the reader, evaluate panel ghosting, change voltage/LUT settings, or certify physical waveform behavior.


## Factory-waveform responsiveness (1.2.0)

The independent reviewer examined the nonblocking display path as an application author and player: immutable snapshots, latest-frame coalescing, input polling, shutdown, confirmation safety, and Tetris landing grace. The review identified three issues before release: async-to-blocking completion could restart the BUSY timeout; the BUSY assertion guard began before activation command transfer; and Paper grouped two different pages under the same presentation barrier. All three are corrected. Deadline tests now include switching modes near 15 seconds, a late completion with BUSY low, and nonzero command-transfer time.

Ordinary play can process input while the panel refreshes. Critical screens finish before input polling, preserving the existing buffered-Enter safeguards. Grounded and newly spawned pieces also finish presentation before the visible landing clock advances. The added timed Tetris journey sends opposite taps during BUSY and checks both the final position and snapshot coalescing. All 24 Tetris emulator journeys and nine Paper timing scenarios pass, alongside retained-render comparisons.

This review did not operate physical buttons or assess ghosting. Hardware timing, the 200-submission async stress test, and the completed-refresh soak are documented separately in [performance](performance.md). Only factory waveforms are used.
