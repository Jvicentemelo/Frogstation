#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <memory>

#include "CoverManager.h"
#include "GameWindow.h"
#include "Gamepad.h"
#include "InputConfig.h"
#include "Library.h"
#include "NoIntroDat.h"

class QLineEdit;
class QListView;
class QSlider;
class QSortFilterProxyModel;
class QStandardItemModel;
class QStandardItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private:
    void addRomFolder();
    void showControls();
    void openControls(QWidget* parent);
    void rescan();
    void reloadModel();
    void applyZoom(int width);
    void launch(const QModelIndex& proxyIndex);
    QString corePath();
    void ensureDat();
    QIcon iconFor(const QString& title);

    QString m_dataDir;
    QSettings m_settings;
    InputConfig m_input;   // depende de m_settings
    Gamepad m_gamepad;     // depende de m_input
    Library m_library;
    NoIntroDat m_dat;
    CoverManager m_covers;

    QListView* m_view;
    QStandardItemModel* m_model;
    QSortFilterProxyModel* m_proxy;
    QLineEdit* m_search;
    QSlider* m_zoom;
    QHash<QString, QStandardItem*> m_itemByTitle;
    QPointer<GameWindow> m_game;
};
