#include "InputDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Gamepad.h"

InputDialog::InputDialog(InputConfig* cfg, Gamepad* pad, QWidget* parent)
    : QDialog(parent), m_cfg(cfg), m_pad(pad)
{
    setWindowTitle(tr("Controles"));
    resize(520, 560);
    setFocusPolicy(Qt::StrongFocus);

    m_info = new QLabel;
    m_info->setWordWrap(true);

    m_table = new QTableWidget(kActionCount, 3);
    m_table->setHorizontalHeaderLabels({tr("Ação"), tr("Teclado"), tr("Controle")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);  // as teclas vão para o diálogo durante a captura
    for (int r = 0; r < kActionCount; ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(InputConfig::actionLabel(Action(r))));
        for (int c = 1; c <= 2; ++c) {
            auto* it = new QTableWidgetItem;
            it->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(r, c, it);
        }
    }
    connect(m_table, &QTableWidget::cellClicked, this, [this](int r, int c) {
        if (c == 1 || c == 2) startCapture(r, c);
    });

    auto* hint = new QLabel(tr("Clique numa célula e pressione a tecla ou o botão desejado. "
                               "Delete limpa o mapeamento; Esc cancela."));
    hint->setWordWrap(true);

    auto* resetBtn = new QPushButton(tr("Restaurar padrão"));
    auto* closeBtn = new QPushButton(tr("Fechar"));
    for (auto* b : {resetBtn, closeBtn}) { b->setFocusPolicy(Qt::NoFocus); b->setAutoDefault(false); }
    connect(resetBtn, &QPushButton::clicked, this, [this] { endCapture(); m_cfg->resetDefaults(); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(resetBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(m_info);
    lay->addWidget(hint);
    lay->addWidget(m_table, 1);
    lay->addLayout(buttons);

    connect(m_cfg, &InputConfig::changed, this, &InputDialog::refresh);
    connect(m_pad, &Gamepad::devicesChanged, this, &InputDialog::updateInfo);
    connect(m_pad, &Gamepad::buttonPressed, this, [this](int btn) {
        if (m_capCol != 2) return;
        m_cfg->setPad(Action(m_capRow), btn);
        endCapture();
    });

    refresh();
    updateInfo();
    setFocus();
}

void InputDialog::updateInfo()
{
    const QStringList n = m_pad->names();
    if (!m_pad->available())
        m_info->setText(tr("Gamepad indisponível (falha ao iniciar o SDL2)."));
    else if (n.isEmpty())
        m_info->setText(tr("Nenhum controle detectado. Conecte um gamepad (pode ser feito agora)."));
    else
        m_info->setText(tr("Controles detectados: %1").arg(n.join(QStringLiteral(", "))));
}

void InputDialog::refresh()
{
    for (int r = 0; r < kActionCount; ++r) {
        m_table->item(r, 1)->setText(InputConfig::keyName(m_cfg->key(Action(r))));
        m_table->item(r, 2)->setText(InputConfig::padName(m_cfg->pad(Action(r))));
    }
    if (m_capRow >= 0)
        m_table->item(m_capRow, m_capCol)->setText(tr("Pressione…"));
}

void InputDialog::startCapture(int row, int col)
{
    m_capRow = row;
    m_capCol = col;
    refresh();
    setFocus();
}

void InputDialog::endCapture()
{
    m_capRow = m_capCol = -1;
    refresh();
}

void InputDialog::keyPressEvent(QKeyEvent* e)
{
    if (m_capCol < 0) { QDialog::keyPressEvent(e); return; }
    if (e->isAutoRepeat()) return;

    const Action a = Action(m_capRow);
    const int k = e->key();
    if (k == Qt::Key_Escape) { endCapture(); return; }
    if (k == Qt::Key_Delete || k == Qt::Key_Backspace) {
        if (m_capCol == 1) m_cfg->setKey(a, 0); else m_cfg->setPad(a, -1);
        endCapture();
        return;
    }
    if (m_capCol == 1 && k != Qt::Key_unknown) {  // coluna do teclado
        m_cfg->setKey(a, k);
        endCapture();
    }
    // coluna do controle: espera o sinal Gamepad::buttonPressed
}
