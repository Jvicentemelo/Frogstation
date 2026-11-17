#pragma once
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "NoIntroDat.h"

struct GameEntry {
    QString path;
    QString title;       // nome No-Intro (ou nome do arquivo, se não identificado)
    QString crc;         // CRC32 hex (sem header iNES quando houver)
    qint64 playSeconds = 0;
    QDateTime lastPlayed;
};

class Library {
public:
    explicit Library(const QString& dbPath);
    bool open();

    // Varre as pastas; chama progress(atual, total). Retorna nº de jogos novos/atualizados.
    int scan(const QStringList& romDirs, const NoIntroDat& dat,
             const std::function<bool(int, int)>& progress = {});

    QVector<GameEntry> games() const;
    void addPlayTime(const QString& path, qint64 seconds);

private:
    QSqlDatabase m_db;
    QString m_dbPath;
};
