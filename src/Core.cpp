#include "Core.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
static void* openLib(const char* p) { return (void*)LoadLibraryA(p); }
static void* findSym(void* l, const char* n) { return (void*)GetProcAddress((HMODULE)l, n); }
static void closeLib(void* l) { FreeLibrary((HMODULE)l); }
#else
#  include <dlfcn.h>
static void* openLib(const char* p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void* findSym(void* l, const char* n) { return dlsym(l, n); }
static void closeLib(void* l) { dlclose(l); }
#endif

// Os callbacks do libretro são funções C puras (sem "user data"),
// então precisamos de uma instância ativa estática.
static Core* g_core = nullptr;

static void RETRO_CALLCONV logCb(enum retro_log_level level, const char* fmt, ...)
{
    if (level < RETRO_LOG_INFO) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}

Core::~Core()
{
    unloadGame();
    if (m_lib) {
        if (retro_deinit) retro_deinit();
        closeLib(m_lib);
        m_lib = nullptr;
    }
    if (g_core == this) g_core = nullptr;
}

bool Core::loadLibrary(const std::string& path, std::string& err)
{
    m_lib = openLib(path.c_str());
    if (!m_lib) { err = "Não foi possível carregar o núcleo: " + path; return false; }

    bool ok = true;
    auto sym = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(findSym(m_lib, name));
        if (!fn) { ok = false; err = std::string("Símbolo ausente no núcleo: ") + name; }
    };
    sym(retro_init, "retro_init");
    sym(retro_deinit, "retro_deinit");
    sym(retro_api_version, "retro_api_version");
    sym(retro_get_system_info, "retro_get_system_info");
    sym(retro_get_system_av_info, "retro_get_system_av_info");
    sym(retro_set_environment, "retro_set_environment");
    sym(retro_set_video_refresh, "retro_set_video_refresh");
    sym(retro_set_audio_sample, "retro_set_audio_sample");
    sym(retro_set_audio_sample_batch, "retro_set_audio_sample_batch");
    sym(retro_set_input_poll, "retro_set_input_poll");
    sym(retro_set_input_state, "retro_set_input_state");
    sym(retro_reset, "retro_reset");
    sym(retro_run, "retro_run");
    sym(retro_load_game, "retro_load_game");
    sym(retro_unload_game, "retro_unload_game");
    sym(retro_get_memory_data, "retro_get_memory_data");
    sym(retro_get_memory_size, "retro_get_memory_size");
    if (!ok) return false;

    // Serialização é opcional: sem ela, os estados ficam indisponíveis.
    auto opt = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(findSym(m_lib, name));
    };
    opt(retro_serialize_size, "retro_serialize_size");
    opt(retro_serialize, "retro_serialize");
    opt(retro_unserialize, "retro_unserialize");

    if (retro_api_version() != RETRO_API_VERSION) {
        err = "Versão da API libretro incompatível";
        return false;
    }

    g_core = this;
    // O environment deve ser registrado ANTES de retro_init.
    retro_set_environment(&Core::environmentCb);
    retro_init();
    retro_set_video_refresh(&Core::videoCb);
    retro_set_audio_sample(&Core::audioSampleCb);
    retro_set_audio_sample_batch(&Core::audioBatchCb);
    retro_set_input_poll(&Core::inputPollCb);
    retro_set_input_state(&Core::inputStateCb);
    return true;
}

bool Core::loadGame(const std::string& romPath, std::string& err)
{
    retro_system_info info{};
    retro_get_system_info(&info);

    retro_game_info game{};
    game.path = romPath.c_str();
    if (!info.need_fullpath) {
        std::ifstream f(romPath, std::ios::binary);
        if (!f) { err = "Não foi possível abrir a ROM: " + romPath; return false; }
        m_romData.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        game.data = m_romData.data();
        game.size = m_romData.size();
    }
    if (!retro_load_game(&game)) { err = "O núcleo recusou a ROM."; return false; }
    m_gameLoaded = true;
    retro_get_system_av_info(&m_av);
    return true;
}

void Core::unloadGame()
{
    if (m_gameLoaded && retro_unload_game) retro_unload_game();
    m_gameLoaded = false;
    m_romData.clear();
}

void Core::reset() { if (m_gameLoaded) retro_reset(); }
void Core::run()   { if (m_gameLoaded) retro_run(); }

float Core::aspectRatio() const
{
    if (m_av.geometry.aspect_ratio > 0.f) return m_av.geometry.aspect_ratio;
    return m_av.geometry.base_height ? float(m_av.geometry.base_width) / float(m_av.geometry.base_height) : 4.f / 3.f;
}

bool Core::saveState(std::vector<uint8_t>& out) const
{
    if (!m_gameLoaded || !retro_serialize_size || !retro_serialize) return false;
    const size_t n = retro_serialize_size();
    if (n == 0) return false;
    out.assign(n, 0);
    return retro_serialize(out.data(), n);
}

bool Core::loadState(const std::vector<uint8_t>& in)
{
    if (!m_gameLoaded || !retro_unserialize || in.empty()) return false;
    return retro_unserialize(in.data(), in.size());
}

std::vector<uint8_t> Core::saveRam() const
{
    if (!m_gameLoaded) return {};
    size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    auto* p = static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    return (p && n) ? std::vector<uint8_t>(p, p + n) : std::vector<uint8_t>{};
}

void Core::setSaveRam(const std::vector<uint8_t>& data)
{
    if (!m_gameLoaded || data.empty()) return;
    size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    auto* p = static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    if (p && n) std::copy_n(data.begin(), std::min(n, data.size()), p);
}

// ---------------------------------------------------------------- callbacks

bool Core::environmentCb(unsigned cmd, void* data)
{
    Core* c = g_core;
    if (!c) return false;
    cmd &= 0xFFFF;  // remove flags EXPERIMENTAL/PRIVATE

    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        auto fmt = *static_cast<const retro_pixel_format*>(data);
        if (fmt == RETRO_PIXEL_FORMAT_XRGB8888 || fmt == RETRO_PIXEL_FORMAT_RGB565) {
            c->m_pixelFormat = fmt;
            return true;
        }
        return false;  // 0RGB1555 não suportado
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char**>(data) = c->m_systemDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = c->m_saveDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = logCb;
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
        return false;  // usa os valores padrão do núcleo
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        return true;
    default:
        return false;
    }
}

void Core::videoCb(const void* data, unsigned w, unsigned h, size_t pitch)
{
    if (!g_core || !data || !g_core->m_video) return;  // data==nullptr: frame duplicado
    g_core->m_video(data, w, h, pitch, g_core->m_pixelFormat);
}

void Core::audioSampleCb(int16_t l, int16_t r)
{
    if (!g_core || !g_core->m_audio) return;
    const int16_t s[2] = {l, r};
    g_core->m_audio(s, 1);
}

size_t Core::audioBatchCb(const int16_t* data, size_t frames)
{
    if (g_core && g_core->m_audio) g_core->m_audio(data, frames);
    return frames;
}

void Core::inputPollCb() {}

int16_t Core::inputStateCb(unsigned port, unsigned device, unsigned /*index*/, unsigned id)
{
    if (!g_core || !g_core->m_input || device != RETRO_DEVICE_JOYPAD) return 0;
    return g_core->m_input(port, id);
}
