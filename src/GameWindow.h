#pragma once
#include <QAudioSink>
#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QWidget>
#include <memory>

#include "Core.h"
#include "InputConfig.h"

class Gamepad;

// Janela de emulação: roda o núcleo, desenha o vídeo, toca o áudio, lê teclado/gamepad
// e gerencia save states (10 slots por jogo).
class GameWindow : public QWidget {
    Q_OBJECT
public:
    GameWindow(const QString& corePath, const QString& romPath, const QString& dataDir,
               InputConfig* input, Gamepad* gamepad, QWidget* parent = nullptr);
    ~GameWindow() override;

    bool ok() const { return m_ok; }
    QString error() const { return m_error; }
    QString romPath() const { return m_romPath; }

signals:
    void finished(const QString& romPath, qint64 playSeconds);
    void requestControls();  // o menu de contexto pediu a tela de controles

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void closeEvent(QCloseEvent*) override;

private:
    static constexpr int kSlotCount = 10;

    void tick();
    void startAudio();
    void handleHotkeys(quint32 pressed);
    void toggleFullscreen();
    void showOsd(const QString& text);

    QString sramPath() const;
    QString statePath(int slot) const;
    QString thumbPath(int slot) const;
    void saveState();
    void loadState();
    void setSlot(int slot);

    Core m_core;
    InputConfig* m_input;
    Gamepad* m_gamepad;
    bool m_ok = false;
    QString m_error, m_romPath, m_dataDir, m_saveDir, m_stateDir;

    QImage m_frame;
    QTimer m_timer;
    QElapsedTimer m_clock, m_played, m_osdClock;
    qint64 m_framesRun = 0;
    bool m_paused = false;

    std::unique_ptr<QAudioSink> m_sink;
    QIODevice* m_audioIo = nullptr;

    quint32 m_keyMask = 0;   // bit(Action) pressionado no teclado
    quint32 m_prevHot = 0;   // para detectar o "aperto" dos atalhos
    quint32 m_mask[2] = {0, 0};  // estado dos botões do NES, por jogador, no frame atual
    signed char m_retroToAction[16];

    int m_slot = 0;
    QString m_osd;
    bool m_finishedEmitted = false;
};
