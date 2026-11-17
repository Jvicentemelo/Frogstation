#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Frogstation"));
    QApplication::setApplicationName(QStringLiteral("Frogstation"));

    // Tema escuro simples (Fusion), no estilo dos emuladores modernos.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    p.setColor(QPalette::Window, QColor(30, 33, 38));
    p.setColor(QPalette::WindowText, QColor(220, 223, 228));
    p.setColor(QPalette::Base, QColor(22, 24, 28));
    p.setColor(QPalette::AlternateBase, QColor(30, 33, 38));
    p.setColor(QPalette::Text, QColor(220, 223, 228));
    p.setColor(QPalette::Button, QColor(42, 46, 53));
    p.setColor(QPalette::ButtonText, QColor(220, 223, 228));
    p.setColor(QPalette::Highlight, QColor(60, 140, 90));
    p.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(p);

    MainWindow w;
    w.show();
    return app.exec();
}
