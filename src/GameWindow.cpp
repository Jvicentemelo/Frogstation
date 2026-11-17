#include "GameWindow.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMediaDevices>
#include <QMenu>
#include <QPainter>
#include <QSaveFile>

#include "Gamepad.h"

GameWindow::GameWindow(const QString& corePath, const QString& romPath, const QString& dataDir,
                       InputConfig* input, Gamepad* gamepad, QWidget* parent)
    : QWidget(parent, Qt::Window),
      m_input(input), m_gamepad(gamepad), m_romPath(romPath), m_dataDir(dataDir)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(256, 240);
    setWindowTitle(QFileInfo(romPath).completeBaseName());

    // id do botão no libretro -> Action (só os 8 botões do NES)
    for (signed char& v : m_retroToAction) v = -1;
    for (int a = 0; a < kNesButtonCount; ++a) m_retroToAction[retroIdFor(Action(a))] = static_cast<signed char>(a);

    QDir d(dataDir);
    d.mkpath(QStringLiteral("system"));
    d.mkpath(QStringLiteral("saves"));
    d.mkpath(QStringLiteral("states"));
    m_saveDir = d.filePath(QStringLiteral("saves"));
    m_stateDir = d.filePath(QStringLiteral("states"));
    m_core.setSystemDir(d.filePath(QStringLiteral("system")).toStdString());
    m_core.setSaveDir(m_saveDir.toStdString());

    m_core.setVideoCallback([this](const void* pixels, unsigned w, unsigned h, size_t pitch, unsigned fmt) {
        const auto f = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? QImage::Format_RGB16 : QImage::Format_RGB32;
        // copy(): o buffer do núcleo só vale durante o callback
        m_frame = QImage(static_cast<const uchar*>(pixels), int(w), int(h), int(pitch), f).copy();
    });
    m_core.setAudioCallback([this](const int16_t* s, size_t frames) {
        if (!m_audioIo) return;
        const qint64 bytes = qint64(frames) * 2 * sizeof(int16_t);
        if (m_sink && m_sink->bytesFree() >= bytes)  // buffer cheio: descarta em vez de bloquear
            m_audioIo->write(reinterpret_cast<const char*>(s), bytes);
    });
    m_core.setInputCallback([this](unsigned port, unsigned id) -> int16_t {
        if (port > 1 || id >= 16) return 0;
        const int a = m_retroToAction[id];
        return (a >= 0) ? int16_t((m_mask[port] >> a) & 1u) : 0;
    });

    std::string err;
    if (!m_core.loadLibrary(corePath.toStdString(), err) ||
        !m_core.loadGame(romPath.toStdString(), err)) {
        m_error = QString::fromStdString(err);
        return;
    }

    // Restaura SRAM de bateria, se existir.
    if (QFile f(sramPath()); f.open(QIODevice::ReadOnly)) {
        const QByteArray b = f.readAll();
        m_core.setSaveRam({b.begin(), b.end()});
    }

    resize(int(m_core.baseWidth()) * 3, int(m_core.baseHeight()) * 3);
    startAudio();

    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &GameWindow::tick);
    m_timer.start(2);
    m_clock.start();
    m_played.start();
    m_ok = true;
}

GameWindow::~GameWindow() = default;

QString GameWindow::sramPath() const
{
    return QDir(m_saveDir).filePath(QFileInfo(m_romPath).completeBaseName() + QStringLiteral(".srm"));
}

QString GameWindow::statePath(int slot) const
{
    return QDir(m_stateDir).filePath(QFileInfo(m_romPath).completeBaseName()
                                     + QStringLiteral(".state%1").arg(slot));
}

QString GameWindow::thumbPath(int slot) const { return statePath(slot) + QStringLiteral(".png"); }

void GameWindow::startAudio()
{
    QAudioFormat fmt;
    fmt.setSampleRate(int(m_core.sampleRate()));
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
    m_sink = std::make_unique<QAudioSink>(QMediaDevices::defaultAudioOutput(), fmt);
    m_sink->setBufferSize(fmt.sampleRate() * 4 / 10);  // ~100 ms
    m_audioIo = m_sink->start();
}

// Ritmo: o timer dispara a cada ~2 ms e executa quantos frames forem devidos
// segundo o relógio, o que dá 60.0988 fps exatos (QTimer só tem resolução de 1 ms).
void GameWindow::tick()
{
    if (m_paused) return;

    const double due = double(m_clock.nsecsElapsed()) * 1e-9 * m_core.fps();
    if (due - double(m_framesRun) > 8.0) m_framesRun = qint64(due);  // ficou muito para trás
    if (double(m_framesRun) >= due) return;  // ainda não é hora do próximo frame

    // Entrada: lida uma vez por "lote" de frames.
    m_gamepad->update();
    const quint32 hot = (m_keyMask | m_gamepad->hotkeyMask()) & kHotkeyMask;
    const quint32 pressed = hot & ~m_prevHot;
    m_prevHot = hot;
    if (pressed) handleHotkeys(pressed);
    if (m_paused) return;  // um atalho pode ter aberto algo que pausa

    m_mask[0] = (m_keyMask | m_gamepad->mask(0)) & kNesMask;
    m_mask[1] = m_gamepad->mask(1) & kNesMask;

    int n = 0;
    while (double(m_framesRun) < due && n < 3) {
        m_core.run();
        ++m_framesRun;
        ++n;
    }
    update();
}

void GameWindow::handleHotkeys(quint32 pressed)
{
    if (pressed & bit(Action::SaveState)) saveState();
    if (pressed & bit(Action::LoadState)) loadState();
    if (pressed & bit(Action::SlotPrev)) setSlot((m_slot + kSlotCount - 1) % kSlotCount);
    if (pressed & bit(Action::SlotNext)) setSlot((m_slot + 1) % kSlotCount);
    if (pressed & bit(Action::Reset)) { m_core.reset(); showOsd(tr("Jogo reiniciado")); }
    if (pressed & bit(Action::Fullscreen)) toggleFullscreen();
}

void GameWindow::toggleFullscreen()
{
    if (isFullScreen()) showNormal(); else showFullScreen();
}

// ----------------------------------------------------------------- save states

void GameWindow::setSlot(int slot)
{
    m_slot = slot;
    showOsd(tr("Slot %1 (%2)").arg(slot + 1).arg(QFile::exists(statePath(slot)) ? tr("ocupado") : tr("vazio")));
}

void GameWindow::saveState()
{
    std::vector<uint8_t> buf;
    if (!m_core.saveState(buf)) { showOsd(tr("Este núcleo não suporta estados salvos")); return; }

    QSaveFile f(statePath(m_slot));  // gravação atômica: não corrompe o estado anterior se falhar
    if (!f.open(QIODevice::WriteOnly)) { showOsd(tr("Falha ao gravar o estado")); return; }
    f.write(reinterpret_cast<const char*>(buf.data()), qint64(buf.size()));
    if (!f.commit()) { showOsd(tr("Falha ao gravar o estado")); return; }

    if (!m_frame.isNull())  // miniatura do momento salvo
        m_frame.scaledToWidth(256, Qt::SmoothTransformation).save(thumbPath(m_slot), "PNG");
    showOsd(tr("Estado salvo no slot %1").arg(m_slot + 1));
}

void GameWindow::loadState()
{
    QFile f(statePath(m_slot));
    if (!f.open(QIODevice::ReadOnly)) { showOsd(tr("Slot %1 vazio").arg(m_slot + 1)); return; }
    const QByteArray b = f.readAll();
    const std::vector<uint8_t> buf(b.begin(), b.end());
    if (m_core.loadState(buf))
        showOsd(tr("Estado carregado do slot %1").arg(m_slot + 1));
    else
        showOsd(tr("Não foi possível carregar o slot %1").arg(m_slot + 1));
}

void GameWindow::showOsd(const QString& text)
{
    m_osd = text;
    m_osdClock.restart();
    update();
}

// ------------------------------------------------------------------- desenho

void GameWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_frame.isNull()) return;

    // Mantém a proporção do núcleo (NES: 8:7 de pixel => ~4:3 na TV) e centraliza.
    const float ar = m_core.aspectRatio();
    QSize s(width(), int(width() / ar));
    if (s.height() > height()) s = QSize(int(height() * ar), height());
    const QRect target(QPoint((width() - s.width()) / 2, (height() - s.height()) / 2), s);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);  // pixels nítidos; filtro/shaders: etapa futura
    p.drawImage(target, m_frame);

    if (m_osdClock.isValid() && m_osdClock.elapsed() < 2500) {  // mensagens na tela
        QFont f = font();
        f.setPointSize(11);
        f.setBold(true);
        p.setFont(f);
        QRect r = p.fontMetrics().boundingRect(m_osd).adjusted(-10, -6, 10, 6);
        r.moveTopLeft(QPoint(12, 12));
        p.fillRect(r, QColor(0, 0, 0, 170));
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, m_osd);
    }
}

// ------------------------------------------------------------------- entrada

void GameWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { close(); return; }
    if (e->isAutoRepeat()) return;
    const int a = m_input->actionForKey(e->key());
    if (a >= 0) m_keyMask |= 1u << a;
}

void GameWindow::keyReleaseEvent(QKeyEvent* e)
{
    if (e->isAutoRepeat()) return;
    const int a = m_input->actionForKey(e->key());
    if (a >= 0) m_keyMask &= ~(1u << a);
}

// Sem foco não chegam eventos de tecla soltando; zera para não "grudar" botões.
void GameWindow::focusOutEvent(QFocusEvent*) { m_keyMask = 0; }

void GameWindow::contextMenuEvent(QContextMenuEvent* e)
{
    m_paused = true;
    m_keyMask = 0;

    QMenu menu(this);
    const auto keyTip = [this](Action a) { return InputConfig::keyName(m_input->key(a)); };

    menu.addAction(tr("Salvar estado (slot %1)\t%2").arg(m_slot + 1).arg(keyTip(Action::SaveState)),
                   this, [this] { saveState(); });
    menu.addAction(tr("Carregar estado (slot %1)\t%2").arg(m_slot + 1).arg(keyTip(Action::LoadState)),
                   this, [this] { loadState(); });

    QMenu* slotMenu = menu.addMenu(tr("Slot do estado"));
    auto* group = new QActionGroup(slotMenu);
    for (int i = 0; i < kSlotCount; ++i) {
        QAction* a = slotMenu->addAction(
            tr("Slot %1%2").arg(i + 1).arg(QFile::exists(statePath(i)) ? QString() : tr(" (vazio)")));
        a->setCheckable(true);
        a->setChecked(i == m_slot);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { setSlot(i); });
    }

    menu.addSeparator();
    menu.addAction(tr("Reiniciar\t%1").arg(keyTip(Action::Reset)), this, [this] {
        m_core.reset();
        showOsd(tr("Jogo reiniciado"));
    });
    menu.addAction(tr("Tela cheia\t%1").arg(keyTip(Action::Fullscreen)), this, [this] { toggleFullscreen(); });
    menu.addAction(tr("Controles…"), this, [this] { emit requestControls(); });
    menu.addSeparator();
    menu.addAction(tr("Fechar jogo\tEsc"), this, [this] { close(); });

    menu.exec(e->globalPos());

    m_clock.restart();  // não tenta "recuperar" o tempo em que ficou pausado
    m_framesRun = 0;
    m_paused = false;
}

void GameWindow::closeEvent(QCloseEvent* e)
{
    m_timer.stop();
    if (m_ok) {
        const auto ram = m_core.saveRam();
        if (!ram.empty())
            if (QFile f(sramPath()); f.open(QIODevice::WriteOnly))
                f.write(reinterpret_cast<const char*>(ram.data()), qint64(ram.size()));
        if (m_sink) m_sink->stop();
        m_core.unloadGame();
        if (!m_finishedEmitted) {
            m_finishedEmitted = true;
            emit finished(m_romPath, m_played.elapsed() / 1000);
        }
    }
    e->accept();
}
