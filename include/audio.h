#pragma once

#include "raylib.h"

#include <string>

// Centralized audio manager for ArcadeZombieFPS.
// Built on Raylib 5.5's built-in audio (InitAudioDevice / LoadSound /
// LoadMusicStream / PlaySound / PlayMusicStream). All resources are loaded
// once in Init() and released in Shutdown(); both are idempotent and safe to
// call repeatedly.
//
// The class is header-only so it can be added without changing the CMake
// source list. One instance is owned by Game.
//
// Audio assets live in assets/sounds/ and are loaded from a project-relative
// path (the game runs from build/, so ../assets/... is tried first).

class AudioManager {
public:
    enum class Sfx {
        Gunshot,        // player weapon fire
        Reload,         // weapon reload
        EmptyClick,     // empty magazine (optional)
        ZombieIdle,     // occasional zombie growl/moan
        ZombieAttack,   // zombie melee attack on player
        ZombieDeath,    // zombie killed
        PlayerHurt,     // player takes damage
        WaveStart,      // new wave begins
        GameOver,       // player died / game over

        Count
    };

    enum class MusicId { Ambient };

    AudioManager() = default;
    ~AudioManager() { Shutdown(); }

    // Initialize the audio device and load all resources once.
    // Safe to call multiple times (re-initializes cleanly).
    bool Init();
    // Release every loaded sound/music and close the audio device.
    // Safe to call multiple times.
    void Shutdown();
    bool Ready() const { return initialized_; }

    // Call once per frame so the music stream keeps streaming.
    void Update();

    // Playback -----------------------------------------------------------
    void PlaySfx(Sfx id);
    void PlayMusic(MusicId id);
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();

    // --------------------------------------------------------------------
    void SetMasterVolume(float v);
    void SetSfxVolume(float v);
    void SetMusicVolume(float v);
    float MasterVolume() const { return masterVolume_; }
    float SfxVolume() const { return sfxVolume_; }
    float MusicVolume() const { return musicVolume_; }

private:
    static constexpr int kSfxCount = (int)Sfx::Count;

    bool initialized_ = false;

    float masterVolume_ = 1.0f;   // 0..1
    float sfxVolume_ = 1.0f;      // 0..1 multiplier applied to sfx
    float musicVolume_ = 0.45f;   // 0..1 multiplier applied to music (ambient quieter)

    // Loaded resources (zero-initialized until loaded).
    ::Sound sfx_[kSfxCount]{};
    bool sfxLoaded_[kSfxCount]{};
    ::Music musicTrack_{};
    bool musicLoaded_ = false;
    bool musicPlaying_ = false;

    // Per-sfx last-play time to throttle spammy sounds (zombie idle).
    double lastPlay_[kSfxCount]{};

    // Project-relative asset resolution (cwd = build/ at launch).
    static const char* ResolvePath(const char* fileName);

    bool LoadSoundFromFile(Sfx id, const char* fileName);
    void UnloadAllSounds();
    bool LoadMusicFromFile(MusicId /*id*/, const char* fileName);
    void UnloadAllMusic();

    float SfxVolumeWithMaster(Sfx id) const { return masterVolume_ * sfxVolume_; }
    float MusicVolumeWithMaster() const { return masterVolume_ * musicVolume_; }

    // Minimum seconds between repeats of the same sfx (0 = no throttle).
    static float MinRepeat(Sfx id);
};

// ---------------------------------------------------------------------------
// Inline implementation
// ---------------------------------------------------------------------------

inline const char* AudioManager::ResolvePath(const char* fileName) {
    static std::string path;
    static const char* candidates[] = {
        "assets/sounds/",
        "../assets/sounds/",
        "../../assets/sounds/",
    };
    for (const char* dir : candidates) {
        std::string p = std::string(dir) + fileName;
        if (FileExists(p.c_str())) {
            path = p;
            return path.c_str();
        }
    }
    path = std::string("assets/sounds/") + fileName;
    return path.c_str();
}

inline bool AudioManager::LoadSoundFromFile(Sfx id, const char* fileName) {
    int idx = (int)id;
    if (sfxLoaded_[idx]) {
        UnloadSound(sfx_[idx]);
    }
    sfx_[idx] = LoadSound(ResolvePath(fileName));
    bool ok = IsSoundValid(sfx_[idx]);
    if (!ok) {
        sfx_[idx] = {};
        sfxLoaded_[idx] = false;
        return false;
    }
    sfxLoaded_[idx] = true;
    SetSoundVolume(sfx_[idx], SfxVolumeWithMaster(id));
    return true;
}

inline void AudioManager::UnloadAllSounds() {
    for (int i = 0; i < kSfxCount; ++i) {
        if (sfxLoaded_[i]) {
            UnloadSound(sfx_[i]);
            sfx_[i] = {};
            sfxLoaded_[i] = false;
        }
    }
}

inline bool AudioManager::LoadMusicFromFile(MusicId /*id*/, const char* fileName) {
    if (musicLoaded_) {
        UnloadMusicStream(musicTrack_);
    }
    musicTrack_ = LoadMusicStream(ResolvePath(fileName));
    bool ok = IsMusicValid(musicTrack_);
    if (!ok) {
        musicTrack_ = {};
        musicLoaded_ = false;
        return false;
    }
    musicLoaded_ = true;
    ::SetMusicVolume(musicTrack_, MusicVolumeWithMaster());
    return true;
}

inline void AudioManager::UnloadAllMusic() {
    if (musicLoaded_) {
        StopMusicStream(musicTrack_);
        UnloadMusicStream(musicTrack_);
        musicTrack_ = {};
        musicLoaded_ = false;
    }
}

inline bool AudioManager::Init() {
    Shutdown(); // idempotent: clean any previous state first

    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
    }
    if (!IsAudioDeviceReady()) {
        initialized_ = false;
        return false;
    }

    // Sound effects
    LoadSoundFromFile(Sfx::Gunshot, "gunshot.wav");
    LoadSoundFromFile(Sfx::Reload, "reload.wav");
    LoadSoundFromFile(Sfx::EmptyClick, "empty_click.wav");
    LoadSoundFromFile(Sfx::ZombieIdle, "zombie_moans.ogg");
    LoadSoundFromFile(Sfx::ZombieAttack, "zombie_pain.ogg");   // repurposed pain as attack bite
    LoadSoundFromFile(Sfx::ZombieDeath, "zombie_pain.ogg");
    LoadSoundFromFile(Sfx::PlayerHurt, "zombie_pain.ogg");
    LoadSoundFromFile(Sfx::WaveStart, "wave_start.wav");
    LoadSoundFromFile(Sfx::GameOver, "gameover.wav");

    // Music
    LoadMusicFromFile(MusicId::Ambient, "ambient_loop.ogg");

    initialized_ = true;
    return true;
}

inline void AudioManager::Shutdown() {
    StopMusic();
    UnloadAllMusic();
    UnloadAllSounds();
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    initialized_ = false;
    for (int i = 0; i < kSfxCount; ++i) lastPlay_[i] = 0.0;
}

inline void AudioManager::Update() {
    if (!initialized_) return;
    if (musicLoaded_) {
        UpdateMusicStream(musicTrack_);
    }
}

inline float AudioManager::MinRepeat(Sfx id) {
    switch (id) {
        case Sfx::ZombieIdle: return 2.5f; // occasional ambient growls
        default: return 0.0f;
    }
}

inline void AudioManager::PlaySfx(Sfx id) {
    if (!initialized_) return;
    int idx = (int)id;
    if (!sfxLoaded_[idx]) return;

    // Throttle spammy sounds.
    float minRepeat = MinRepeat(id);
    if (minRepeat > 0.0f) {
        double now = GetTime();
        if (now - lastPlay_[idx] < minRepeat) return;
        lastPlay_[idx] = now;
    }

    SetSoundVolume(sfx_[idx], SfxVolumeWithMaster(id));
    PlaySound(sfx_[idx]);
}

inline void AudioManager::PlayMusic(MusicId id) {
    if (!initialized_) return;
    if (!musicLoaded_) return;
    if (musicPlaying_) return;
    ::SetMusicVolume(musicTrack_, MusicVolumeWithMaster());
    PlayMusicStream(musicTrack_);
    musicPlaying_ = true;
}

inline void AudioManager::StopMusic() {
    if (!initialized_ || !musicLoaded_) return;
    StopMusicStream(musicTrack_);
    musicPlaying_ = false;
}

inline void AudioManager::PauseMusic() {
    if (!initialized_ || !musicLoaded_) return;
    PauseMusicStream(musicTrack_);
    musicPlaying_ = false;
}

inline void AudioManager::ResumeMusic() {
    if (!initialized_ || !musicLoaded_) return;
    ResumeMusicStream(musicTrack_);
    musicPlaying_ = true;
}

inline void AudioManager::SetMasterVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    masterVolume_ = v;
    ::SetMasterVolume(v); // raylib listener volume
    // Re-apply to currently-loaded sounds/music so they reflect the change.
    for (int i = 0; i < kSfxCount; ++i) {
        if (sfxLoaded_[i]) SetSoundVolume(sfx_[i], SfxVolumeWithMaster((Sfx)i));
    }
    if (musicLoaded_) ::SetMusicVolume(musicTrack_, MusicVolumeWithMaster());
}

inline void AudioManager::SetSfxVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    sfxVolume_ = v;
    for (int i = 0; i < kSfxCount; ++i) {
        if (sfxLoaded_[i]) SetSoundVolume(sfx_[i], SfxVolumeWithMaster((Sfx)i));
    }
}

inline void AudioManager::SetMusicVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    musicVolume_ = v;
    if (musicLoaded_) ::SetMusicVolume(musicTrack_, MusicVolumeWithMaster());
}
