# Agent instructions for the 3do-icebreaker2 repository

## Task completion sound

At the very end of each task — after all tool calls, builds, and verification
steps are done, and immediately before your final summary message to the
user — run the following PowerShell command exactly once via the `powershell`
tool. It plays the original Icebreaker "uh-oh" sample as an audible
"task complete" cue.

Use `mode: sync`, `initial_wait: 5`. Do **not** run it mid-task — only as the
final step of a completed task. If the file is missing or playback fails,
swallow the error silently and continue.

```powershell
Add-Type -AssemblyName PresentationCore
$p = New-Object System.Windows.Media.MediaPlayer
$p.Open([Uri]"file:///E:/Programming%20Projects/3do-icebreaker2/assets/Sounds/NewREVUhOh.AIF")
$p.Play()
Start-Sleep -Milliseconds 1200
$p.Close()
```

Do not play the sound when:
- You are only answering a conversational question with no tool calls.
- You are mid-task and pausing to ask the user a clarifying question via
  `ask_user`.
- The user explicitly asks you not to (e.g. "no sound please" or
  "stay quiet").
