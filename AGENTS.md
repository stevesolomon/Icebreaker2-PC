# Agent instructions for the 3do-icebreaker2 repository

## Reference source trees and original assets

When investigating ports, parser quirks, or legacy game logic, you have full
access to all four reference trees on this machine:

| Purpose                              | Path                                                                 |
| ------------------------------------ | -------------------------------------------------------------------- |
| **IB1 source code** (3DO original)   | `E:\Programming Projects\3do-icebreaker-master`                      |
| **IB1 assets** (unpacked ISO)        | `E:\Programming Projects\icebreaker_iso\Icebreaker.bin.unpacked`     |
| **IB2 source code** (this repo/port) | `E:\Programming Projects\3do-icebreaker2`                            |
| **IB2 assets** (unpacked ISO)        | `E:\Programming Projects\3do-icebreaker2\iso_assets`                 |

Use these to:

- Verify whether a "bug" exists in the original asset files vs is a
  porting/transcription artifact (e.g., compare `iso_assets/IceFiles/...`
  with `assets/...`).
- Cross-reference original 3DO C++ logic against the ported version.
- Recover or re-extract assets if the working `assets/` copy gets corrupted.

## Always rebuild after code changes

Whenever you modify **anything** that could affect what the user runs —
C/C++ source, headers, `CMakeLists.txt`, OR any file under `assets/` — you
MUST run the build step before declaring the task complete. No exceptions,
even if you "only" touched data files. The user runs `Icebreaker2.exe`
themselves to test, so a stale exe or stale `build/assets/` copy leads to
confusing bad states and wasted user time.

Run BOTH of the following after every change, in order:

1. **Build the binary** (rebuilds the exe AND triggers the POST_BUILD asset
   copy if the target relinks):

```powershell
cmd /c "cd /d ""E:\Programming Projects\3do-icebreaker2"" && ""C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"" -arch=amd64 >nul && ""C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build build 2>&1"
```

2. **Force-sync assets** — usually unnecessary now that `CMakeLists.txt`
   declares `add_custom_target(sync_assets ALL ...)` which copies
   `assets/` → `build/assets/` on **every** build (even when ninja reports
   "no work to do"). If you ever need to sync manually:

```powershell
cmd /c """C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" -E copy_directory ""E:\Programming Projects\3do-icebreaker2\assets"" ""E:\Programming Projects\3do-icebreaker2\build\assets"""
```

After both steps:

- Verify `build\Icebreaker2.exe`'s `LastWriteTime` is newer than the latest
  source file you touched.
- For any asset you edited, verify the file size or mtime under
  `build\assets\...` matches the source under `assets\...`.

If a build fails, fix it before declaring the task complete — never hand
the user a broken binary or a stale binary or stale assets.

## Task completion sound

At the very end of each task — after **all** other tool calls (including
`store_memory`, builds, verifications, etc.) have completed and immediately
before your final summary message to the user — run the following PowerShell
command exactly once via the `powershell` tool. It plays the original
Icebreaker "uh-oh" sample as an audible "task complete" cue.

**Critical rules:**
- The sound-playing tool call MUST be in its OWN response (not bundled in
  parallel with any other tool calls). Bundling it with other tools tends
  to swallow the audio output.
- It MUST be the very last tool call you make for the task, immediately
  followed by your final text-only summary message.
- Use `mode: sync`, `initial_wait: 5`.
- If the file is missing or playback fails, swallow the error silently and
  continue to the summary.

```powershell
(New-Object System.Media.SoundPlayer "$env:USERPROFILE\.copilot\sounds\done.wav").PlaySync()
```

The WAV is generated once from `assets/Sounds/NewREVUhOh.AIF` and lives in
the user's Copilot config directory; if it's missing, regenerate it by
converting the AIF (8-bit signed PCM, 22050 Hz, mono → 8-bit unsigned WAV).
WPF `MediaPlayer` cannot decode this AIFF variant correctly, so prefer the
WAV + `System.Media.SoundPlayer` path.

Do not play the sound when:
- You are only answering a conversational question with no tool calls.
- You are mid-task and pausing to ask the user a clarifying question via
  `ask_user`.
- The user explicitly asks you not to (e.g. "no sound please" or
  "stay quiet").
