#pragma once
// Wrapper fino sobre um núcleo libretro (ex.: mesen_libretro.dll).
// Sem dependência de Qt: só biblioteca padrão + libretro.h.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "libretro.h"

class Core {
public:
    // pixelFormat: RETRO_PIXEL_FORMAT_XRGB8888 ou RETRO_PIXEL_FORMAT_RGB565
    using VideoCb = std::function<void(const void* data, unsigned w, unsigned h, size_t pitch, unsigned pixelFormat)>;
    using AudioCb = std::function<void(const int16_t* interleavedStereo, size_t frames)>;
    using InputCb = std::function<int16_t(unsigned port, unsigned id)>;

    Core() = default;
    ~Core();
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    bool loadLibrary(const std::string& path, std::string& err);
    bool loadGame(const std::string& romPath, std::string& err);
    void unloadGame();
    void reset();
    void run();  // roda um frame

    void setVideoCallback(VideoCb cb) { m_video = std::move(cb); }
    void setAudioCallback(AudioCb cb) { m_audio = std::move(cb); }
    void setInputCallback(InputCb cb) { m_input = std::move(cb); }
    void setSystemDir(std::string d) { m_systemDir = std::move(d); }
    void setSaveDir(std::string d) { m_saveDir = std::move(d); }

    double fps() const { return m_av.timing.fps > 1.0 ? m_av.timing.fps : 60.0988; }
    double sampleRate() const { return m_av.timing.sample_rate > 1.0 ? m_av.timing.sample_rate : 44100.0; }
    unsigned baseWidth() const { return m_av.geometry.base_width; }
    unsigned baseHeight() const { return m_av.geometry.base_height; }
    float aspectRatio() const;

    // Save states (serialização do núcleo)
    bool saveState(std::vector<uint8_t>& out) const;
    bool loadState(const std::vector<uint8_t>& in);

    // Memória persistente (SRAM de bateria)
    std::vector<uint8_t> saveRam() const;
    void setSaveRam(const std::vector<uint8_t>& data);

private:
    static bool environmentCb(unsigned cmd, void* data);
    static void videoCb(const void* data, unsigned w, unsigned h, size_t pitch);
    static void audioSampleCb(int16_t l, int16_t r);
    static size_t audioBatchCb(const int16_t* data, size_t frames);
    static void inputPollCb();
    static int16_t inputStateCb(unsigned port, unsigned device, unsigned index, unsigned id);

    void* m_lib = nullptr;
    bool m_gameLoaded = false;
    unsigned m_pixelFormat = RETRO_PIXEL_FORMAT_XRGB8888;
    retro_system_av_info m_av{};
    std::string m_systemDir, m_saveDir;
    std::vector<uint8_t> m_romData;  // precisa viver enquanto o jogo estiver carregado

    VideoCb m_video;
    AudioCb m_audio;
    InputCb m_input;

    // Ponteiros de função do núcleo
    void (*retro_init)() = nullptr;
    void (*retro_deinit)() = nullptr;
    unsigned (*retro_api_version)() = nullptr;
    void (*retro_get_system_info)(retro_system_info*) = nullptr;
    void (*retro_get_system_av_info)(retro_system_av_info*) = nullptr;
    void (*retro_set_environment)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state)(retro_input_state_t) = nullptr;
    void (*retro_reset)() = nullptr;
    void (*retro_run)() = nullptr;
    bool (*retro_load_game)(const retro_game_info*) = nullptr;
    void (*retro_unload_game)() = nullptr;
    void* (*retro_get_memory_data)(unsigned) = nullptr;
    size_t (*retro_get_memory_size)(unsigned) = nullptr;
    size_t (*retro_serialize_size)() = nullptr;                  // opcionais
    bool (*retro_serialize)(void*, size_t) = nullptr;
    bool (*retro_unserialize)(const void*, size_t) = nullptr;
};
