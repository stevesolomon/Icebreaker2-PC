# Agent instructions for the 3do-icebreaker2 repository

## Always rebuild after code changes

Whenever you modify any C/C++ source or header in `src/` (or change anything
that affects the build, such as `CMakeLists.txt`), you MUST rebuild the
binary before declaring the task complete. The user runs `Icebreaker2.exe`
themselves to test, so a stale exe leads to confusing bad states.

Build command (Windows, MSVC):

```powershell
cmd /c "cd /d ""E:\Programming Projects\3do-icebreaker2"" && ""C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"" -arch=amd64 >nul && ""C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build build 2>&1"
```

After building, verify `build\Icebreaker2.exe`'s `LastWriteTime` is newer
than the latest source file you touched. If a build fails, fix it before
declaring the task complete — never hand the user a broken binary or a
stale binary.

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
