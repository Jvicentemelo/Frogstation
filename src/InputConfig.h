#pragma once
#include <QObject>
#include <QSettings>
#include <QString>
#include <array>

// Ações configuráveis: 8 botões do NES + atalhos do emulador.
enum class Action : int {
    Up, Down, Left, Right, A, B, Select, Start,            // botões do NES (0..7)
    SaveState, LoadState, SlotPrev, SlotNext, Reset, Fullscreen,
    Count
};
constexpr int kActionCount = int(Action::Count);
constexpr int kNesButtonCount = 8;
constexpr quint32 kNesMask = (1u << kNesButtonCount) - 1;
constexpr quint32 kHotkeyMask = ((1u << kActionCount) - 1) & ~kNesMask;

// "Botões" de gatilho analógico, além dos botões do SDL (0..20).
constexpr int kPadLeftTrigger = 100;
constexpr int kPadRightTrigger = 101;

constexpr quint32 bit(Action a) { return 1u << int(a); }

// ID do botão no libretro (RETRO_DEVICE_ID_JOYPAD_*) para botões do NES; -1 para atalhos.
int retroIdFor(Action a);

// Guarda e persiste os mapeamentos de teclado e de gamepad (SDL_GameControllerButton).
class InputConfig : public QObject {
    Q_OBJECT
public:
    explicit InputConfig(QSettings* settings, QObject* parent = nullptr);

    static QString actionLabel(Action a);
    static QString keyName(int key);   // "—" se não houver
    static QString padName(int btn);   // "—" se não houver
    static int normalizeKey(int key);  // Enter do teclado numérico -> Return

    int key(Action a) const { return m_key[size_t(a)]; }  // 0 = nenhuma
    int pad(Action a) const { return m_pad[size_t(a)]; }  // -1 = nenhum
    void setKey(Action a, int key);  // remove a mesma tecla de outras ações
    void setPad(Action a, int btn);
    int actionForKey(int key) const;  // -1 se a tecla não estiver mapeada
    void resetDefaults();

signals:
    void changed();

private:
    void load();
    void save();

    QSettings* m_settings;
    std::array<int, kActionCount> m_key{};
    std::array<int, kActionCount> m_pad{};
};
