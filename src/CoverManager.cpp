#include "CoverManager.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

static const char* kBase =
    "https://thumbnails.libretro.com/Nintendo%20-%20Nintendo%20Entertainment%20System/Named_Boxarts/";

CoverManager::CoverManager(const QString& cacheDir, QObject* parent)
    : QObject(parent), m_dir(cacheDir)
{
    m_dir.mkpath(QStringLiteral("."));
}

// O libretro-thumbnails troca estes caracteres por '_' nos nomes de arquivo.
QString CoverManager::thumbName(const QString& title)
{
    QString s = title;
    for (QChar& c : s)
        if (QStringLiteral("&*/:`<>?\\|\"").contains(c)) c = QLatin1Char('_');
    return s;
}

QString CoverManager::fileFor(const QString& title) const
{
    return m_dir.filePath(thumbName(title) + QStringLiteral(".png"));
}

QPixmap CoverManager::cover(const QString& title)
{
    if (auto it = m_mem.constFind(title); it != m_mem.constEnd()) return *it;

    const QString file = fileFor(title);
    if (QFile::exists(file)) {
        QPixmap pm(file);
        if (!pm.isNull()) { m_mem.insert(title, pm); return pm; }
    }
    // ".miss" evita repetir tentativas de capas que não existem no servidor.
    if (!QFile::exists(file + QStringLiteral(".miss")) && !m_queued.contains(title)) {
        m_queued.insert(title);
        m_queue.enqueue(title);
        pump();
    }
    return {};
}

void CoverManager::pump()
{
    while (m_inFlight < 4 && !m_queue.isEmpty()) {
        const QString title = m_queue.dequeue();
        const QUrl url(QString::fromLatin1(kBase)
                       + QString::fromLatin1(QUrl::toPercentEncoding(thumbName(title)))
                       + QStringLiteral(".png"));
        ++m_inFlight;
        QNetworkReply* r = m_net.get(QNetworkRequest(url));
        connect(r, &QNetworkReply::finished, this, [this, r, title] {
            r->deleteLater();
            --m_inFlight;
            const QString file = fileFor(title);
            if (r->error() == QNetworkReply::NoError) {
                const QByteArray data = r->readAll();
                QPixmap pm;
                if (pm.loadFromData(data)) {
                    QFile f(file);
                    if (f.open(QIODevice::WriteOnly)) f.write(data);
                    m_mem.insert(title, pm);
                    emit coverReady(title);
                }
            } else if (r->error() == QNetworkReply::ContentNotFoundError) {
                QFile miss(file + QStringLiteral(".miss"));
                (void)miss.open(QIODevice::WriteOnly);
            }  // outros erros (rede): tenta de novo na próxima execução
            pump();
        });
    }
}
