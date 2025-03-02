#ifndef TMUX__TMUXNOTIFICATIONPARSESTATE_H
#define TMUX__TMUXNOTIFICATIONPARSESTATE_H

#include "konsoleprivate_export.h"

#include <QtCore>

namespace Konsole
{

struct KONSOLEPRIVATE_EXPORT TmuxNotificationParseState {
    enum class Mode : uint {
        ParseCommandName,
        ParseCommandArgs,
        ResponseNewline,
        ResponseBuffer,
    };
    void reset()
    {
        lexBuffer.clear();
        output.clear();
        arg = 0;
        for (auto &c : commandResponse) {
            c.clear();
        }
    }
    QVector<uint> lexBuffer;
    QList<QVector<uint>> commandResponse;
    QString output;
    bool response;
    int arg;
    Mode mode;
};

}

#endif
