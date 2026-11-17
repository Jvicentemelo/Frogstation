#pragma once
#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QQueue>
#include <QSet>

// Baixa capas do libretro-thumbnails (por nome No-Intro) e mantém cache em disco.
class CoverManager : public QObject {
    Q_OBJECT
public:
    explicit CoverManager(const QString& cacheDir, QObject* parent = nullptr);

    // Retorna a capa se já estiver em cache; senão enfileira o download e devolve QPixmap nulo.
    QPixmap cover(const QString& title);

signals:
    void coverReady(const QString& title);

private:
    QString fileFor(const QString& title) const;
    static QString thumbName(const QString& title);
    void pump();

    QDir m_dir;
    QNetworkAccessManager m_net;
    QHash<QString, QPixmap> m_mem;
    QSet<QString> m_queued;
    QQueue<QString> m_queue;
    int m_inFlight = 0;
};
