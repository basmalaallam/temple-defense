# Sound integration report

Branch: `game-implementation` (changes are not committed or pushed).
Backup of the version before these changes: `backups/temple-defense-before-sound.zip`.

## Files

| File | Change |
|---|---|
| `audio.h`, `audio.cpp` | New. Audio module on miniaudio: one looping music instance, 6 click voices, victory and game over sounds, global volume and mute. |
| `main.cpp` | Calls into the audio module, adds the Sound Settings screen and the `+`, `-`, `N` keys, shows the volume. No gameplay rules changed. |
| `CMakeLists.txt` | Builds `audio.cpp`, adds the miniaudio include folder, links threads/dl/m, copies `assets/` next to the executable. |
| `build.bat` | New. `build.bat` or `build.bat Debug` builds with CMake + Ninja (tools from `install_tools.py`). |
| `external/miniaudio/miniaudio.h` | New. miniaudio v0.11.22 (public domain / MIT-0). |
| `assets/sounds/*` | New. The supplied sound files. |
| `CODE_STRUCTURE.md` | Short "Sound" section added. |

## Sound used for each event

| Event | File | Notes |
|---|---|---|
| Background music | `background_music.mp3` | Audio track of the supplied `intro.mp4`, converted to MP3 (192 kbps, 5:09). Loops through main menu, Sound Settings, cutscene, gameplay and pause. |
| Menu button click | `click.mp3` | Main menu, Sound Settings, pause and win/lose buttons. Only when a button is hit. |
| Lane selection | `click.mp3` | Pitch 0.9. |
| Guardian card selection | `click.mp3` | Pitch 1.15. Locked cards make no sound. |
| Guardian placement | `click.mp3` | Pitch 0.8. Only when the purchase succeeds. |
| Victory | `victory.mp3` | Plays once on entering the Victory screen; music pauses. |
| Game over | `game_over.mp3` | Plays once on entering the Defeat screen; music pauses. |

The music resumes from where it stopped when you restart or go to the main menu, and a
victory or game over sound still playing is stopped then.

## Volume

One global volume (default 70%) for music and effects, 0% to 100% in steps of 10.
Mute keeps the level, so unmuting restores it. Volume changes while muted change the
kept level and stay muted. Keys: `+` (or numpad +) up, `-` (or numpad -) down, `N` mute,
and holding `+`/`-` repeats. `M` still goes to the main menu. No existing key was changed
(existing keys: W/S/arrows, Esc, Enter, Space, M).

## Builds

| Build | Result |
|---|---|
| Linux Debug (CMake + Ninja, g++) | Built, no errors, no new warnings |
| Linux Release | Built, no errors |
| Windows Release (MinGW-w64 cross compile, same CMakeLists) | Built `OpenGLStarter.exe`, sounds copied to `assets/` |
| `build.bat` | Written, not run: the test machine is Linux. It runs the same CMake commands as the Windows build above. Please run it once on Windows. |

## Tests (real game under a virtual display, sound recorded from the audio output)

All passed. The recording is `sound-test/playback-recording.mp3`, with the timeline in
`sound-test/recording-timeline.txt` (the recording runs about half a second behind it).

- Music starts at the main menu and keeps playing through Sound Settings, cutscene, play and pause.
- Music loop: jumped to 4 s before the end of the track; 6 s later it was at 1.8 s and still playing.
- `N` mute: output fully silent; `N` again: music back at the same level.
- `-` x3 → 40%, `+` x6 → 100%, extra `+` stays 100%, `-` x11 → 0% (silent, stays 0%), `+` x7 → 70%.
- Sound Settings buttons: Volume Up 70→80, Volume Down 80→70, Mute (silent, shows MUTED and UNMUTE), Unmute, Back.
- Measured click peaks with music paused: 100% −0.3 dBFS, 50% −6.3 dBFS (−6 dB expected), 10% −20.3 dBFS (−20 dB expected), muted: silent.
- Lane select, card select and guardian placement each produced an audible click; a locked card produced none.
- Defeat: music paused, game over sound played once; Restart: music resumed.
- Victory (forced through the debugger, since winning by script isn't practical): music paused, fanfare played once; Main Menu: music resumed.
- Pause → Continue and Pause → Restart work, with clicks.
- Missing sound files: the game prints an error and keeps running silently.
