#pragma once
#include <QHash>
#include <QString>

// Lê o DAT No-Intro (formato clrmamepro) e mapeia CRC32 -> nome canônico do jogo.
class NoIntroDat {
public:
    bool load(const QString& path);
    QString lookup(uint32_t crc) const { return m_byCrc.value(crc); }
    bool isEmpty() const { return m_byCrc.isEmpty(); }

private:
    QHash<uint32_t, QString> m_byCrc;
};
