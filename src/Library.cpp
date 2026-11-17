#include "Library.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QVariant>

#include "Crc32.h"

Library::Library(const QString& dbPath) : m_dbPath(dbPath) {}

bool Library::open()
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("library"));
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) return false;
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS games ("
        " path TEXT PRIMARY KEY, title TEXT NOT NULL, crc TEXT,"
        " size INTEGER, mtime INTEGER,"
        " play_seconds INTEGER DEFAULT 0, last_played INTEGER DEFAULT 0)"));
}

int Library::scan(const QStringList& romDirs, const NoIntroDat& dat,
                  const std::function<bool(int, int)>& progress)
{
    QStringList files;
    for (const QString& dir : romDirs) {
        QDirIterator it(dir, {QStringLiteral("*.nes")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) files << it.next();
    }

    m_db.transaction();
    int changed = 0, i = 0;
    QSqlQuery check(m_db), up(m_db);
    check.prepare(QStringLiteral("SELECT size, mtime FROM games WHERE path = ?"));
    up.prepare(QStringLiteral(
        "INSERT INTO games(path,title,crc,size,mtime) VALUES(?,?,?,?,?) "
        "ON CONFLICT(path) DO UPDATE SET title=excluded.title, crc=excluded.crc,"
        " size=excluded.size, mtime=excluded.mtime"));

    for (const QString& path : files) {
        if (progress && !progress(i++, files.size())) break;
        const QFileInfo fi(path);

        check.bindValue(0, path);
        check.exec();
        if (check.next() && check.value(0).toLongLong() == fi.size()
            && check.value(1).toLongLong() == fi.lastModified().toSecsSinceEpoch())
            continue;  // inalterado: pula o hash

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray bytes = f.readAll();
        const auto* p = reinterpret_cast<const uint8_t*>(bytes.constData());

        // O DAT pode ser "headered" (com os 16 bytes iNES) ou não; testamos os dois.
        const bool hasHeader = bytes.size() > 16 && bytes.startsWith("NES\x1A");
        const uint32_t crcFull = crc32(p, size_t(bytes.size()));
        const uint32_t crcBody = hasHeader ? crc32(p + 16, size_t(bytes.size()) - 16) : crcFull;

        QString title = dat.lookup(crcFull);
        if (title.isEmpty()) title = dat.lookup(crcBody);
        if (title.isEmpty()) title = fi.completeBaseName();

        up.bindValue(0, path);
        up.bindValue(1, title);
        up.bindValue(2, QString::number(crcBody, 16).rightJustified(8, QLatin1Char('0')));
        up.bindValue(3, fi.size());
        up.bindValue(4, fi.lastModified().toSecsSinceEpoch());
        up.exec();
        ++changed;
    }

    // Remove entradas de arquivos que não existem mais.
    QSqlQuery all(m_db);
    all.exec(QStringLiteral("SELECT path FROM games"));
    QStringList gone;
    while (all.next())
        if (!QFile::exists(all.value(0).toString())) gone << all.value(0).toString();
    for (const QString& g : gone) {
        QSqlQuery del(m_db);
        del.prepare(QStringLiteral("DELETE FROM games WHERE path = ?"));
        del.bindValue(0, g);
        del.exec();
    }
    m_db.commit();
    return changed;
}

QVector<GameEntry> Library::games() const
{
    QVector<GameEntry> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT path,title,crc,play_seconds,last_played FROM games ORDER BY title COLLATE NOCASE"));
    while (q.next()) {
        GameEntry g;
        g.path = q.value(0).toString();
        g.title = q.value(1).toString();
        g.crc = q.value(2).toString();
        g.playSeconds = q.value(3).toLongLong();
        const qint64 lp = q.value(4).toLongLong();
        if (lp > 0) g.lastPlayed = QDateTime::fromSecsSinceEpoch(lp);
        out.push_back(g);
    }
    return out;
}

void Library::addPlayTime(const QString& path, qint64 seconds)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE games SET play_seconds = play_seconds + ?, last_played = ? WHERE path = ?"));
    q.bindValue(0, seconds);
    q.bindValue(1, QDateTime::currentSecsSinceEpoch());
    q.bindValue(2, path);
    q.exec();
}
