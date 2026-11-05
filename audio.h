#pragma once

// Game audio built on miniaudio. Every function is safe to call even when the
// audio device or a sound file failed to load: it then does nothing.

enum class SoundEffect {
    MenuClick,
    LaneSelect,
    CardSelect,
    GuardianPlaced
};

enum class ResultSound {
    Victory,
    GameOver
};

namespace audio {

constexpr int volumeStep = 10; // Percent per volume up/down press.

bool initialize();   // Returns false if no audio is available; the game still runs.
void shutdown();

// Background music: one looping instance for the whole game.
void playMusic();    // Starts or resumes the music where it stopped.
void pauseMusic();

void playEffect(SoundEffect effect);
void playResult(ResultSound sound); // Plays once from the start.
void stopResults();

// One global volume for music and effects, from 0 to 100 percent.
void volumeUp();
void volumeDown();
void toggleMute();
int volumePercent(); // The level that is kept while muted.
bool isMuted();

} // namespace audio
