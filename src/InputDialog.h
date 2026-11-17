#pragma once
#include <QDialog>

#include "InputConfig.h"

class Gamepad;
class QLabel;
class QTableWidget;

// Tela "Controles": remapeia teclado e gamepad para cada ação.
class InputDialog : public QDialog {
    Q_OBJECT
public:
    InputDialog(InputConfig* cfg, Gamepad* pad, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void refresh();
    void startCapture(int row, int col);
    void endCapture();
    void updateInfo();

    InputConfig* m_cfg;
    Gamepad* m_pad;
    QTableWidget* m_table;
    QLabel* m_info;
    int m_capRow = -1, m_capCol = -1;
};
