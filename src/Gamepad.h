#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "InputConfig.h"

// Leitura de gamepads via SDL2 (SDL_GameController): hot-plug, até 2 jogadores,
// analógico esquerdo como direcional e gatilhos LT/RT como botões.
class Gamepad : public QObject {
    Q_OBJECT
public:
    Gamepad(InputConfig* cfg, QObject* parent = nullptr);
    ~Gamepad() override;

    bool available() const { return m_sdlOk; }
    QStringList names() const;

    // Processa eventos do SDL. Pode ser chamado a qualquer momento (é idempotente).
    void update();

    // Máscara de ações pressionadas (bit(Action)) para o jogador 0 ou 1.
    quint32 mask(int player) const;
    // Atalhos pressionados em qualquer controle.
    quint32 hotkeyMask() const;

signals:
    void buttonPressed(int button);  // botão SDL (0..20) ou kPadLeftTrigger/kPadRightTrigger
    void devicesChanged();

private:
    struct Pad {
        void* ctrl = nullptr;  // SDL_GameController*
        int instanceId = -1;
        bool lt = false, rt = false;
    };
    int indexOfInstance(int id) const;

    InputConfig* m_cfg;
    bool m_sdlOk = false;
    QVector<Pad> m_pads;
    QTimer m_timer;
};
