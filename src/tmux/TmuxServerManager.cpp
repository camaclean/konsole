#include "TmuxServerManager.h"

using namespace Konsole;

#if 0
void TmuxServerManager::parseClientDetached(const QList<QVector<uint>>& args)
{
    Q_ASSERT(args.size() == 2);
    emit clientDetached(QString::fromUcs4(args[1].data(), args[1].size()));
}

void TmuxServerManager::parseClientSessionChanged(const QList<QVector<uint>>& args)
{
    Q_ASSERT(args.size() >= 4);
    QString client = QString::fromUcs4(args[1].data(), args[1].size());
    int id = QString::fromUcs4(args[2].data(), args[2].size()).toInt();
    QString name;
    int i = 3;
    for (; i < args.size()-1; ++i) {
        name += QString::fromUcs4(args[i].data(), args[1].size());
        name += ' ';
    }
    name += QString::fromUcs4(args[i].data(), args[1].size());
    emit clientSessionChanged(client, id, name);
}
#endif
