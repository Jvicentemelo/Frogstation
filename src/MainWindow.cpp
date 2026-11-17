#include "MainWindow.h"

#include <QApplication>
#include "InputDialog.h"
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPainter>
#include <QProgressDialog>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

static QString dataDirPath()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(d);
    return d;
}

MainWindow::MainWindow()
    : m_dataDir(dataDirPath()),
      m_settings(QDir(m_dataDir).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat),
      m_input(&m_settings),
      m_gamepad(&m_input),
      m_library(QDir(m_dataDir).filePath(QStringLiteral("library.db"))),
      m_covers(QDir(m_dataDir).filePath(QStringLiteral("covers")))
{
    setWindowTitle(QStringLiteral("Frogstation"));
    resize(1100, 720);

    m_library.open();

    // --- barra de ferramentas
    auto* tb = addToolBar(QStringLiteral("Principal"));
    tb->setMovable(false);
    tb->addAction(tr("Adicionar pasta de ROMs…"), this, &MainWindow::addRomFolder);
    tb->addAction(tr("Atualizar"), this, &MainWindow::rescan);
    tb->addAction(tr("Controles…"), this, &MainWindow::showControls);
    tb->addSeparator();
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Buscar jogo…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(280);
    tb->addWidget(m_search);
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
    m_zoom = new QSlider(Qt::Horizontal);
    m_zoom->setRange(90, 320);
    m_zoom->setValue(m_settings.value(QStringLiteral("zoom"), 160).toInt());
    m_zoom->setFixedWidth(160);
    m_zoom->setToolTip(tr("Tamanho das capas"));
    tb->addWidget(m_zoom);

    // --- grade de capas
    m_model = new QStandardItemModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view = new QListView;
    m_view->setModel(m_proxy);
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setUniformItemSizes(true);
    m_view->setWordWrap(true);
    m_view->setSpacing(6);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    setCentralWidget(m_view);

    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_zoom, &QSlider::valueChanged, this, [this](int v) {
        m_settings.setValue(QStringLiteral("zoom"), v);
        applyZoom(v);
    });
    connect(m_view, &QListView::activated, this, &MainWindow::launch);  // duplo clique / Enter
    connect(&m_covers, &CoverManager::coverReady, this, [this](const QString& title) {
        if (auto* it = m_itemByTitle.value(title)) it->setIcon(iconFor(title));
    });

    connect(&m_gamepad, &Gamepad::devicesChanged, this, [this] {
        const QStringList n = m_gamepad.names();
        statusBar()->showMessage(n.isEmpty() ? tr("Nenhum controle conectado")
                                             : tr("Controles: %1").arg(n.join(QStringLiteral(", "))), 6000);
    });

    applyZoom(m_zoom->value());
    reloadModel();
    if (m_model->rowCount() == 0)
        QTimer::singleShot(0, this, [this] {
            statusBar()->showMessage(tr("Clique em “Adicionar pasta de ROMs…” para começar."));
        });
}

QString MainWindow::corePath()
{
    QString p = m_settings.value(QStringLiteral("core")).toString();
    if (QFile::exists(p)) return p;

    const QString def = QDir(QApplication::applicationDirPath())
                            .filePath(QStringLiteral("cores/mesen_libretro.dll"));
    if (QFile::exists(def)) { m_settings.setValue(QStringLiteral("core"), def); return def; }

    QMessageBox::information(this, tr("Núcleo não encontrado"),
        tr("Coloque o mesen_libretro.dll em:\n%1\n\nou selecione o arquivo agora.")
            .arg(QDir::toNativeSeparators(def)));
    p = QFileDialog::getOpenFileName(this, tr("Selecionar núcleo libretro"), {},
                                     tr("Núcleo libretro (*_libretro.dll)"));
    if (!p.isEmpty()) m_settings.setValue(QStringLiteral("core"), p);
    return p;
}

// Baixa o DAT No-Intro uma vez (para identificar as ROMs pelo CRC32).
void MainWindow::ensureDat()
{
    const QString path = QDir(m_dataDir).filePath(QStringLiteral("nes.dat"));
    if (!QFile::exists(path)) {
        QNetworkAccessManager net;
        QNetworkReply* r = net.get(QNetworkRequest(QUrl(QStringLiteral(
            "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/no-intro/"
            "Nintendo%20-%20Nintendo%20Entertainment%20System.dat"))));
        QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (r->isFinished() && r->error() == QNetworkReply::NoError)
            if (QFile f(path); f.open(QIODevice::WriteOnly)) f.write(r->readAll());
        r->deleteLater();
    }
    m_dat.load(path);  // sem DAT: usa o nome do arquivo como título
}

void MainWindow::addRomFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pasta de ROMs do NES"));
    if (dir.isEmpty()) return;
    QStringList dirs = m_settings.value(QStringLiteral("romDirs")).toStringList();
    if (!dirs.contains(dir)) dirs << dir;
    m_settings.setValue(QStringLiteral("romDirs"), dirs);
    rescan();
}

void MainWindow::rescan()
{
    const QStringList dirs = m_settings.value(QStringLiteral("romDirs")).toStringList();
    if (dirs.isEmpty()) { addRomFolder(); return; }

    ensureDat();
    QProgressDialog pd(tr("Procurando jogos…"), tr("Cancelar"), 0, 100, this);
    pd.setWindowModality(Qt::WindowModal);
    pd.setMinimumDuration(400);
    m_library.scan(dirs, m_dat, [&pd](int cur, int total) {
        pd.setMaximum(total);
        pd.setValue(cur);
        return !pd.wasCanceled();
    });
    pd.close();
    reloadModel();
}

QIcon MainWindow::iconFor(const QString& title)
{
    const QPixmap cover = m_covers.cover(title);
    if (!cover.isNull()) return QIcon(cover);

    // Placeholder enquanto a capa baixa (ou se não existir).
    QPixmap ph(300, 420);
    ph.fill(QColor(0x2b, 0x2f, 0x36));
    QPainter p(&ph);
    p.setPen(QColor(0xaa, 0xb2, 0xbd));
    QFont f = p.font();
    f.setPixelSize(26);
    p.setFont(f);
    p.drawText(ph.rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, title);
    return QIcon(ph);
}

void MainWindow::reloadModel()
{
    m_model->clear();
    m_itemByTitle.clear();
    const auto games = m_library.games();
    for (const GameEntry& g : games) {
        auto* it = new QStandardItem(g.title);
        it->setData(g.path, Qt::UserRole);
        it->setToolTip(tr("%1\nJogado: %2 min").arg(g.title).arg(g.playSeconds / 60));
        it->setIcon(iconFor(g.title));
        m_model->appendRow(it);
        m_itemByTitle.insert(g.title, it);
    }
    m_proxy->sort(0);
    statusBar()->showMessage(tr("%1 jogos").arg(games.size()));
}

void MainWindow::applyZoom(int w)
{
    const int h = int(w * 1.4);
    m_view->setIconSize(QSize(w, h));
    m_view->setGridSize(QSize(w + 24, h + 56));
}

void MainWindow::showControls() { openControls(this); }

void MainWindow::openControls(QWidget* parent)
{
    InputDialog dlg(&m_input, &m_gamepad, parent);
    dlg.exec();
}

void MainWindow::launch(const QModelIndex& idx)
{
    const QString rom = idx.data(Qt::UserRole).toString();
    const QString core = corePath();
    if (rom.isEmpty() || core.isEmpty()) return;
    if (m_game) m_game->close();

    auto* w = new GameWindow(core, rom, m_dataDir, &m_input, &m_gamepad);
    if (!w->ok()) {
        QMessageBox::warning(this, tr("Erro"), w->error());
        delete w;
        return;
    }
    connect(w, &GameWindow::finished, this, [this](const QString& path, qint64 secs) {
        m_library.addPlayTime(path, secs);
    });
    connect(w, &GameWindow::requestControls, this, [this, w] { openControls(w); });
    m_game = w;
    w->show();
    w->setFocus();
}
