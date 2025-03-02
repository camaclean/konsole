#ifndef TMUX__TMUXNOTIFICATION_H
#define TMUX__TMUXNOTIFICATION_H

#include <QtCore>

#include "TmuxNotificationParseState.h"
#include "konsoleprivate_export.h"

namespace Konsole
{

class TmuxServerManager;

class KONSOLEPRIVATE_EXPORT TmuxNotification
{
public:
    TmuxNotification(const char *name)
        : m_name(name)
    {
    }
    TmuxNotification(const TmuxNotification &) = default;
    TmuxNotification(TmuxNotification &&) = default;
    const char *name() const
    {
        return m_name;
    }
    virtual void execute(TmuxServerManager &, TmuxNotificationParseState &) = 0;
    virtual bool push_char(TmuxNotificationParseState &, uint) = 0;
    virtual void reset() = 0;
    virtual ~TmuxNotification()
    {
    }

private:
    const char *m_name;
};

}

#endif
