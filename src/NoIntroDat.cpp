#include "NoIntroDat.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

bool NoIntroDat::load(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    static const QRegularExpression nameRe(R"re(^\s*name\s+"(.*)"\s*$)re");
    static const QRegularExpression crcRe(R"re(\bcrc\s+([0-9A-Fa-f]{8})\b)re");

    QTextStream in(&f);
    QString currentGame;
    bool inGame = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QLatin1String("game ("))) { inGame = true; currentGame.clear(); continue; }
        if (line.startsWith(QLatin1Char(')'))) { inGame = false; continue; }
        if (!inGame) continue;

        if (currentGame.isEmpty()) {
            auto m = nameRe.match(line);
            if (m.hasMatch()) { currentGame = m.captured(1); continue; }
        }
        if (line.contains(QLatin1String("rom ("))) {
            auto m = crcRe.match(line);
            if (m.hasMatch() && !currentGame.isEmpty())
                m_byCrc.insert(m.captured(1).toUInt(nullptr, 16), currentGame);
        }
    }
    return !m_byCrc.isEmpty();
}
