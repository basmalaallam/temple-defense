#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include <miniaudio.h>

#include "audio.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

// Sound files live in assets/sounds next to the executable. These prefixes also
// let the game find them when it is started from the project or build folder.
const char* const searchFolders[] = {"", "../", "../../", "../../../"};
const char* const musicFile = "assets/sounds/background_music.mp3";
const char* const clickFile = "assets/sounds/click.mp3";
const char* const victoryFile = "assets/sounds/victory.mp3";
const char* const gameOverFile = "assets/sounds/game_over.mp3";

constexpr int clickVoiceCount = 6; // Lets quick clicks overlap instead of cutting off.

ma_engine engine;
bool engineReady = false;

ma_sound music;
bool musicReady = false;
ma_sound clickVoices[clickVoiceCount];
bool clickReady[clickVoiceCount] = {};
int nextClickVoice = 0;
ma_sound victory;
bool victoryReady = false;
ma_sound gameOver;
bool gameOverReady = false;

int volume = 70;
bool muted = false;

// Set TEMPLE_AUDIO_LOG=1 to print every audio event (used for testing).
bool logEvents = false;

void logEvent(const char* text) {
    if (logEvents) {
        std::printf("[audio] %s (volume %d%%%s)\n", text, volume, muted ? ", muted" : "");
        std::fflush(stdout);
    }
}

std::string findFile(const char* relativePath) {
    for (const char* folder : searchFolders) {
        std::string path = std::string(folder) + relativePath;
        if (FILE* file = std::fopen(path.c_str(), "rb")) {
            std::fclose(file);
            return path;
        }
    }
    return relativePath;
}

bool loadSound(ma_sound& sound, const char* relativePath, ma_uint32 flags) {
    std::string path = findFile(relativePath);
    ma_result result = ma_sound_init_from_file(&engine, path.c_str(), flags,
                                               nullptr, nullptr, &sound);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "Audio: could not load %s (%s)\n",
                     path.c_str(), ma_result_description(result));
        return false;
    }
    return true;
}

void applyVolume() {
    if (engineReady)
        ma_engine_set_volume(&engine, muted ? 0.0f : volume / 100.0f);
}

void restart(ma_sound& sound, bool ready, const char* name) {
    if (!ready) return;
    ma_sound_seek_to_pcm_frame(&sound, 0);
    if (ma_sound_start(&sound) != MA_SUCCESS)
        std::fprintf(stderr, "Audio: could not play %s\n", name);
}

} // namespace

namespace audio {

bool initialize() {
    if (engineReady) return true;
    const char* logSetting = std::getenv("TEMPLE_AUDIO_LOG");
    logEvents = logSetting && logSetting[0] == '1';

    ma_result result = ma_engine_init(nullptr, &engine);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "Audio: no sound device (%s); continuing without sound\n",
                     ma_result_description(result));
        return false;
    }
    engineReady = true;

    // Music is streamed from disk; short effects are decoded into memory once
    // and shared between all click voices.
    musicReady = loadSound(music, musicFile, MA_SOUND_FLAG_STREAM);
    if (musicReady) ma_sound_set_looping(&music, MA_TRUE);
    for (int voice = 0; voice < clickVoiceCount; ++voice)
        clickReady[voice] = loadSound(clickVoices[voice], clickFile, MA_SOUND_FLAG_DECODE);
    victoryReady = loadSound(victory, victoryFile, MA_SOUND_FLAG_DECODE);
    gameOverReady = loadSound(gameOver, gameOverFile, MA_SOUND_FLAG_DECODE);

    applyVolume();
    logEvent("initialized");
    return true;
}

void shutdown() {
    if (!engineReady) return;
    if (musicReady) ma_sound_uninit(&music);
    for (int voice = 0; voice < clickVoiceCount; ++voice)
        if (clickReady[voice]) ma_sound_uninit(&clickVoices[voice]);
    if (victoryReady) ma_sound_uninit(&victory);
    if (gameOverReady) ma_sound_uninit(&gameOver);
    musicReady = victoryReady = gameOverReady = false;
    std::fill(std::begin(clickReady), std::end(clickReady), false);
    ma_engine_uninit(&engine);
    engineReady = false;
}

void playMusic() {
    if (!musicReady || ma_sound_is_playing(&music)) return; // Never a second copy.
    if (ma_sound_start(&music) != MA_SUCCESS)
        std::fprintf(stderr, "Audio: could not play background music\n");
    logEvent("music playing");
}

void pauseMusic() {
    if (!musicReady || !ma_sound_is_playing(&music)) return;
    ma_sound_stop(&music); // Stopping keeps the position, so playMusic resumes.
    logEvent("music paused");
}

void playEffect(SoundEffect effect) {
    // All interactions use the supplied click; a small pitch change tells them apart.
    float pitch = 1.0f;
    const char* name = "menu click";
    switch (effect) {
    case SoundEffect::MenuClick: break;
    case SoundEffect::LaneSelect: pitch = 0.9f; name = "lane select"; break;
    case SoundEffect::CardSelect: pitch = 1.15f; name = "card select"; break;
    case SoundEffect::GuardianPlaced: pitch = 0.8f; name = "guardian placed"; break;
    }
    int voice = nextClickVoice;
    nextClickVoice = (nextClickVoice + 1) % clickVoiceCount;
    if (!clickReady[voice]) return;
    ma_sound_set_pitch(&clickVoices[voice], pitch);
    restart(clickVoices[voice], true, name);
    logEvent(name);
}

void playResult(ResultSound sound) {
    stopResults();
    if (sound == ResultSound::Victory) {
        restart(victory, victoryReady, "victory");
        logEvent("victory sound");
    } else {
        restart(gameOver, gameOverReady, "game over");
        logEvent("game over sound");
    }
}

void stopResults() {
    if (victoryReady) ma_sound_stop(&victory);
    if (gameOverReady) ma_sound_stop(&gameOver);
}

void volumeUp() {
    volume = std::min(100, volume + volumeStep);
    applyVolume();
    logEvent("volume up");
}

void volumeDown() {
    volume = std::max(0, volume - volumeStep);
    applyVolume();
    logEvent("volume down");
}

void toggleMute() {
    muted = !muted; // The volume level itself is kept, so unmuting restores it.
    applyVolume();
    logEvent(muted ? "muted" : "unmuted");
}

int volumePercent() { return volume; }
bool isMuted() { return muted; }

} // namespace audio
