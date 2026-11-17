#include "InputConfig.h"

#include <QKeySequence>

#include "libretro.h"

int retroIdFor(Action a)
{
    static const int ids[kNesButtonCount] = {
        RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_A,      RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START};
    const int i = int(a);
    return (i >= 0 && i < kNesButtonCount) ? ids[i] : -1;
}

namespace {
const char* const kIds[kActionCount] = {"up",   "down",      "left",     "right",    "a",
                                         "b",    "select",    "start",    "savestate", "loadstate",
                                         "slotprev", "slotnext", "reset", "fullscreen"};

const char* const kLabels[kActionCount] = {
    "Cima", "Baixo", "Esquerda", "Direita", "A", "B", "Select", "Start",
    "Salvar estado", "Carregar estado", "Slot anterior", "Próximo slot", "Reiniciar", "Tela cheia"};

const int kDefaultKeys[kActionCount] = {
    Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_X, Qt::Key_Z, Qt::Key_Shift,
    Qt::Key_Return, Qt::Key_F2, Qt::Key_F4, Qt::Key_F6, Qt::Key_F7, Qt::Key_F1, Qt::Key_F11};

// SDL_CONTROLLER_BUTTON_*: A=0 B=1 X=2 Y=3 BACK=4 GUIDE=5 START=6 LSTICK=7 RSTICK=8
//                          LB=9 RB=10 DPAD_UP=11 DOWN=12 LEFT=13 RIGHT=14
// NES "A" fica no botão da direita (SDL B) e NES "B" no de baixo (SDL A), como em outros emuladores.
const int kDefaultPads[kActionCount] = {11, 12, 13, 14, 1, 0, 4, 6, -1, -1, 9, 10, -1, -1};

const char* const kPadNames[] = {
    "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3", "LB", "RB",
    "D-Pad ↑", "D-Pad ↓", "D-Pad ←", "D-Pad →", "Misc", "Paddle 1", "Paddle 2", "Paddle 3",
    "Paddle 4", "Touchpad"};
}  // namespace

InputConfig::InputConfig(QSettings* settings, QObject* parent) : QObject(parent), m_settings(settings)
{
    load();
}

QString InputConfig::actionLabel(Action a) { return QString::fromUtf8(kLabels[int(a)]); }

int InputConfig::normalizeKey(int key) { return key == Qt::Key_Enter ? int(Qt::Key_Return) : key; }

QString InputConfig::keyName(int key)
{
    if (key == 0) return QStringLiteral("—");
    const QString s = QKeySequence(key).toString(QKeySequence::NativeText);
    return s.isEmpty() ? QStringLiteral("Tecla 0x%1").arg(key, 0, 16) : s;
}

QString InputConfig::padName(int btn)
{
    if (btn < 0) return QStringLiteral("—");
    if (btn == kPadLeftTrigger) return QStringLiteral("LT");
    if (btn == kPadRightTrigger) return QStringLiteral("RT");
    if (btn < int(sizeof(kPadNames) / sizeof(kPadNames[0]))) return QString::fromUtf8(kPadNames[btn]);
    return QStringLiteral("Botão %1").arg(btn);
}

void InputConfig::setKey(Action a, int key)
{
    key = normalizeKey(key);
    if (key != 0)
        for (int i = 0; i < kActionCount; ++i)
            if (i != int(a) && m_key[size_t(i)] == key) m_key[size_t(i)] = 0;
    m_key[size_t(a)] = key;
    save();
    emit changed();
}

void InputConfig::setPad(Action a, int btn)
{
    if (btn >= 0)
        for (int i = 0; i < kActionCount; ++i)
            if (i != int(a) && m_pad[size_t(i)] == btn) m_pad[size_t(i)] = -1;
    m_pad[size_t(a)] = btn;
    save();
    emit changed();
}

int InputConfig::actionForKey(int key) const
{
    key = normalizeKey(key);
    if (key == 0) return -1;
    for (int i = 0; i < kActionCount; ++i)
        if (m_key[size_t(i)] == key) return i;
    return -1;
}

void InputConfig::resetDefaults()
{
    for (int i = 0; i < kActionCount; ++i) {
        m_key[size_t(i)] = kDefaultKeys[i];
        m_pad[size_t(i)] = kDefaultPads[i];
    }
    save();
    emit changed();
}

void InputConfig::load()
{
    for (int i = 0; i < kActionCount; ++i) {
        const QString id = QString::fromLatin1(kIds[i]);
        m_key[size_t(i)] = m_settings->value(QStringLiteral("input/key/") + id, kDefaultKeys[i]).toInt();
        m_pad[size_t(i)] = m_settings->value(QStringLiteral("input/pad/") + id, kDefaultPads[i]).toInt();
    }
}

void InputConfig::save()
{
    for (int i = 0; i < kActionCount; ++i) {
        const QString id = QString::fromLatin1(kIds[i]);
        m_settings->setValue(QStringLiteral("input/key/") + id, m_key[size_t(i)]);
        m_settings->setValue(QStringLiteral("input/pad/") + id, m_pad[size_t(i)]);
    }
    m_settings->sync();
}
