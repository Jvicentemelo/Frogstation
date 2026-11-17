#include "Gamepad.h"

#include <QDebug>
#include <SDL.h>

namespace {
constexpr int kAxisDeadzone = 16000;
constexpr int kTriggerThreshold = 16000;
}  // namespace

Gamepad::Gamepad(InputConfig* cfg, QObject* parent) : QObject(parent), m_cfg(cfg)
{
    // Sem janela do SDL, o foco nunca é "dele": permite receber eventos mesmo assim.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    m_sdlOk = SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0;
    if (!m_sdlOk) {
        qWarning() << "SDL_Init falhou:" << SDL_GetError();
        return;
    }
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::update);
    m_timer.start(16);
    update();  // o SDL gera eventos "added" para os controles já conectados
}

Gamepad::~Gamepad()
{
    for (const Pad& p : m_pads)
        if (p.ctrl) SDL_GameControllerClose(static_cast<SDL_GameController*>(p.ctrl));
    m_pads.clear();
    if (m_sdlOk) SDL_Quit();
}

int Gamepad::indexOfInstance(int id) const
{
    for (int i = 0; i < m_pads.size(); ++i)
        if (m_pads[i].instanceId == id) return i;
    return -1;
}

QStringList Gamepad::names() const
{
    QStringList out;
    for (const Pad& p : m_pads) {
        const char* n = SDL_GameControllerName(static_cast<SDL_GameController*>(p.ctrl));
        out << QString::fromUtf8(n ? n : "Controle");
    }
    return out;
}

void Gamepad::update()
{
    if (!m_sdlOk) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERDEVICEADDED: {
            SDL_GameController* c = SDL_GameControllerOpen(e.cdevice.which);
            if (!c) break;
            const int id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
            if (indexOfInstance(id) >= 0) { SDL_GameControllerClose(c); break; }
            Pad p;
            p.ctrl = c;
            p.instanceId = id;
            m_pads.push_back(p);
            emit devicesChanged();
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED: {
            const int i = indexOfInstance(e.cdevice.which);
            if (i < 0) break;
            SDL_GameControllerClose(static_cast<SDL_GameController*>(m_pads[i].ctrl));
            m_pads.remove(i);
            emit devicesChanged();
            break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
            emit buttonPressed(e.cbutton.button);
            break;
        case SDL_CONTROLLERAXISMOTION: {
            const int i = indexOfInstance(e.caxis.which);
            if (i < 0) break;
            Pad& p = m_pads[i];
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
                const bool now = e.caxis.value > kTriggerThreshold;
                if (now && !p.lt) emit buttonPressed(kPadLeftTrigger);
                p.lt = now;
            } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                const bool now = e.caxis.value > kTriggerThreshold;
                if (now && !p.rt) emit buttonPressed(kPadRightTrigger);
                p.rt = now;
            }
            break;
        }
        default:
            break;
        }
    }
}

quint32 Gamepad::mask(int player) const
{
    if (player < 0 || player >= m_pads.size()) return 0;
    auto* c = static_cast<SDL_GameController*>(m_pads[player].ctrl);
    quint32 m = 0;

    for (int a = 0; a < kActionCount; ++a) {
        const int b = m_cfg->pad(Action(a));
        if (b < 0) continue;
        bool down;
        if (b == kPadLeftTrigger)
            down = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kTriggerThreshold;
        else if (b == kPadRightTrigger)
            down = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold;
        else
            down = SDL_GameControllerGetButton(c, SDL_GameControllerButton(b)) != 0;
        if (down) m |= 1u << a;
    }

    // Analógico esquerdo também funciona como direcional.
    const int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    const int ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (ly < -kAxisDeadzone) m |= bit(Action::Up);
    if (ly > kAxisDeadzone) m |= bit(Action::Down);
    if (lx < -kAxisDeadzone) m |= bit(Action::Left);
    if (lx > kAxisDeadzone) m |= bit(Action::Right);
    return m;
}

quint32 Gamepad::hotkeyMask() const
{
    quint32 m = 0;
    for (int i = 0; i < m_pads.size(); ++i) m |= mask(i);
    return m & kHotkeyMask;
}
