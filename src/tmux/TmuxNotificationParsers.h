#ifndef TMUX__TMUXNOTIFICATIONPARSERS_H
#define TMUX__TMUXNOTIFICATIONPARSERS_H

#include <QtCore>

#include "TmuxNotification.h"
#include "TmuxNotificationParseState.h"
#include "TmuxNotificationParser.h"
#include "Utilities.h"
#include "konsoleprivate_export.h"

namespace Konsole
{

/*struct KONSOLEPRIVATE_EXPORT TmuxResponseNotification
  : TmuxNotification
{

    enum State { Begin, Response, End, Error, None };
    TmuxResponseNotification()
        : TmuxResponseNotification("%
        , state(State::Begin)
        , commandResponse{}
    {
    }
    TmuxResponseNotification(const TmuxResponseNotification &) = default;
    TmuxResponseNotification(TmuxResponseNotification &&) = default;
    TmuxResponseNotification &operator=(const TmuxResponseNotification &) = default;
    TmuxResponseNotification &operator=(TmuxResponseNotification &&) = default;

    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        if (state == End)
            m.receiveCommandResponse(std::move(commandResponse));
        else if (state == Error)
            m.receiveCommandError(std::move(commandResponse));
        else
            qDebug() << "Executed TmuxResponseNotification in invalid state " << (int)state;
        state = None;
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        if (state == State::Response) {
            commandResponse.last().push_back(cc);
        }
    }
    State state;
    QList<QVector<uint>> commandResponse;
};*/

struct KONSOLEPRIVATE_EXPORT TmuxBeginNotification : TmuxNotification {
    TmuxBeginNotification()
        : TmuxNotification("%begin")
    {
    }

    virtual void execute(TmuxServerManager &m, TmuxNotificationParseState &s) override
    {
        s.response = true;
    }

    virtual bool push_char(TmuxNotificationParseState &, uint) override
    {
        return true;
    }

    virtual void reset() override
    {
    }

    virtual ~TmuxBeginNotification()
    {
    }
};

struct KONSOLEPRIVATE_EXPORT TmuxEndNotification : TmuxNotification {
    TmuxEndNotification()
        : TmuxNotification("%end")
        , time{}
        , command_number{}
        , flags{}
    {
    }

    virtual void execute(TmuxServerManager &m, TmuxNotificationParseState &s) override
    {
        s.response = true;
    }

    virtual bool push_char(TmuxNotificationParseState &s, uint cc) override
    {
        if (!s.response)
            return false;
        // If someone were to, for instance, name their session %end, there are
        // circumstances where that could appear at the beginning of the line,
        // unescaped. The %end and %error notifications must therefore be buffered
        // due to ambiguous grammar.
        s.lexBuffer.push_back(cc);
        if (cc == ' ') {
            s.arg++;
            return true;
        }
        if (s.arg == 0)
            return parseDecChar(cc, time);
        else if (s.arg == 1)
            return parseDecChar(cc, command_number);
        else if (s.arg == 2)
            return parseDecChar(cc, flags);
        else
            return false;
        return true;
    }

    virtual void reset() override
    {
        time = 0;
        command_number = 0;
        flags = 0;
    }

    unsigned long long time;
    unsigned long long command_number;
    unsigned long long flags;
};

/*
struct KONSOLEPRIVATE_EXPORT TmuxClientDetachedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ClientDetached;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveClientDetached(QString::fromUcs4(s.lexBuffer.data(), s.lexBuffer.size()));
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        s.lexBuffer.push_back(cc);
    }
};

};
struct KONSOLEPRIVATE_EXPORT TmuxClientSessionChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ClientSessionChanged;
    TmuxClientSessionChangedNotification()
        : arg{}
        , client{}
        , session{-2}
    {
    }
    TmuxClientSessionChangedNotification(const TmuxClientSessionChangedNotification &) = default;
    TmuxClientSessionChangedNotification(TmuxClientSessionChangedNotification &&) = default;
    TmuxClientSessionChangedNotification &operator=(const TmuxClientSessionChangedNotification &) = default;
    TmuxClientSessionChangedNotification &operator=(TmuxClientSessionChangedNotification &&) = default;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveClientSessionChanged(client, session, QString::fromUcs4(s.lexBuffer.data(), s.lexBuffer.size()));
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        if (cc == ' ' && arg == 0) {
            client = QString::fromUcs4(s.lexBuffer.data(), s.lexBuffer.size());
            s.lexBuffer.clear();
            arg++;
        } else if (arg == 1) {
            if (cc == '$') {
                arg++;
            } else {
                qDebug() << "unexpected character parsing session id: " << cc;
                session = -2;
                arg++;
            }
        } else if (arg == 2) {
            if (cc == ' ')
                arg++;
            else if (session == 0 && cc == '*')
                session = -1;
            else if (cc >= 0x30 && cc <= 0x39)
                session = session * 10 + (cc - 0x30);
            else
                qDebug() << "unexpected character parsing session id: " << cc;
        } else {
            s.lexBuffer.push_back(cc);
        }
    }

    int arg;

    QString client;
    int session;
};

struct KONSOLEPRIVATE_EXPORT TmuxConfigErrorNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ConfigError;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveConfigError(QString::fromUcs4(s.lexBuffer.data(), s.lexBuffer.size()));
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        s.lexBuffer.push_back(cc);
    }
};

struct KONSOLEPRIVATE_EXPORT TmuxContinueNotification {
    TmuxContinueNotification()
      : pane{-4}
    {}
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Continue;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveContinue(pane);
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        if (pane == -3 && cc == '%')
            pane = -3;
        else if (pane == -3 && cc >= 0x30 && cc <= 0x39)
            pane = cc - 0x30;
        else if (pane == -3 && cc == '*')
            pane = -2;
        else if (cc >= 0x30 && cc <= 0x39)
            pane = pane * 10 + cc - 0x30;
        else
            qDebug() << "Unexpected character parsing tmux %continue: " << cc;
    }
    int pane;
};

struct KONSOLEPRIVATE_EXPORT TmuxExitNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Exit;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveExit(QString::fromUcs4(s.lexBuffer.data(), s.lexBuffer.size()));
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        s.lexBuffer.push_back(cc);
    }
};

struct KONSOLEPRIVATE_EXPORT TmuxExtendedOutputNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ExtendedOutput;
    void execute(TmuxServerManager &m, TmuxNotificationParseState &s)
    {
        m.receiveExtendedOutput(pane, age, s.lexBuffer);
    }
    void push_char(TmuxNotificationParseState &s, uint cc)
    {
        if (arg == 0) {
            if (cc == '%') {
                arg = 1;
            } else {
                qDebug() << "Expected pane character %, got " << cc;
                arg = -1;
            }
        } else if (arg == 1) {
            if (cc == '*') {
                pane = -1;
            } else if (cc == ' ') {
                arg = 2;
            } else if (cc >= 0x30 && cc <= 0x39) {
                pane = pane * 10 + (cc - 0x30);
            } else {
                qDebug() << "Unexpected character parsing pane id for tmux %extended-output: " << cc;
                arg = -1;
            }
        } else if (arg == 2) {
            if (cc == ' ') {
                arg = 3;
            } else if (cc >= 0x30 && cc <= 0x39) {
                age = age * 10 + (cc - 0x30);
            } else {
                qDebug() << "Unexpected character parsing age for tmux %extended-output: " << cc;
                arg = -1;
            }
        } else if (arg == 3) {
            if (cc == ':')
                arg = 4;
            // ignore everything else;
        } else if (arg == 4) {
            // TODO: confirm %extended-output is escaped like %output
            if (cc == '\\') {
                octParse = 1;
            } else if (octParse > 0 && octParse < 4) {
                if (cc >= 0x30 && cc <= 0x37) {
                    octParseChar = octParseChar * 8 + (cc - 0x30);
                    octParse++;
                    if (octParse == 4) {
                        s.output.push_back(octParseChar);
                        octParseChar = 0;
                        octParse = 0;
                    }
                } else {
                    // TODO: should octal always be 3 digits?
                    s.output.push_back(octParseChar);
                    octParseChar = 0;
                    octParse = 0;
                    s.output.push_back(cc);
                }
            } else {
                s.output.push_back(cc);
            }
        }
    }
    int arg;
    int octParse;
    uint octParseChar;

    int pane;
    unsigned long long age;
};

struct KONSOLEPRIVATE_EXPORT TmuxLayoutChangeNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::LayoutChange;
    void execute(TmuxServerManager &m)
    {
        m.receiveLayoutChange(window, windowLayout, windowVisibleLayout, QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            bool ok;
            window = parseTmuxWindowId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %layout-change: invalid window-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 1 && cc == ' ') {
            // TODO: make this a TmuxWindowLayout
            windowLayout = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 2 && cc == ' ') {
            // TODO: make this a TmuxWindowLayout
            windowVisibleLayout = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        }
    }
    int arg;
    QVector<uint> lexBuffer;

    int window;
    QString windowLayout;
    QString windowVisibleLayout;
};

struct KONSOLEPRIVATE_EXPORT TmuxMessageNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Message;
    void execute(TmuxServerManager &)
    {
        QMessageBox::information(nullptr, "Message", QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxOutputNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Output;
    void execute(TmuxServerManager &m)
    {
        m.receiveOutput(pane, lexBuffer);
    }
    void push_char(uint cc)
    {
        if (arg == 0) {
            if (cc == ' ') {
                bool ok;
                pane = parseTmuxPaneId(lexBuffer, &ok);
                if (!ok)
                    qDebug() << "Error in tmux %output: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
                lexBuffer.clear();
                arg = 1;
            } else {
                lexBuffer.push_back(cc);
            }
        } else {
            if (cc == '\\') {
                octParse = 1;
            } else if (octParse > 0 && octParse < 4) {
                if (cc >= 0x30 && cc <= 0x39) {
                    octParseChar = octParseChar * 8 + (cc - 0x30);
                    octParse++;
                    if (octParse == 4) {
                        lexBuffer.push_back(octParseChar);
                        octParseChar = 0;
                        octParse = 0;
                    }
                } else {
                    lexBuffer.push_back(octParseChar);
                    octParseChar = 0;
                    octParse = 0;
                    lexBuffer.push_back(cc);
                }
            } else {
                lexBuffer.push_back(cc);
            }
        }
    }

    int arg;
    int octParse;
    uint octParseChar;
    QVector<uint> lexBuffer;

    int pane;
};

struct KONSOLEPRIVATE_EXPORT TmuxPaneModeChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::PaneModeChanged;
    void execute(TmuxServerManager &m)
    {
        bool ok;
        int pane = parseTmuxPaneId(lexBuffer, &ok);
        if (ok)
            m.receivePaneModeChanged(pane);
        else
            qDebug() << "Error in tmux %pane-mode-changed: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxPasteBufferChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::PasteBufferChanged;
    void execute(TmuxServerManager &m)
    {
        m.receivePasteBufferChanged(QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxPasteBufferDeletedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::PasteBufferDeleted;
    void execute(TmuxServerManager &m)
    {
        m.receivePasteBufferDeleted(QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxPauseNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Pause;
    void execute(TmuxServerManager &m)
    {
        bool ok;
        int pane = parseTmuxPaneId(lexBuffer, &ok);
        if (ok)
            m.receivePause(pane);
        else
            qDebug() << "Error in tmux %pause: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxSessionChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::SessionChanged;
    void execute(TmuxServerManager &m)
    {
        m.receiveSessionChanged(session, QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            bool ok;
            session = parseTmuxSessionId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %session-changed: invalid session-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else {
            lexBuffer.push_back(cc);
        }
    }
    int arg;
    QVector<uint> lexBuffer;

    int session;
};

struct KONSOLEPRIVATE_EXPORT TmuxSessionRenamedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::SessionRenamed;
    void execute(TmuxServerManager &m)
    {
        m.receiveSessionRenamed(QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxSessionWindowChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::SessionWindowChanged;
    void execute(TmuxServerManager &m)
    {
        bool ok;
        int window = parseTmuxWindowId(lexBuffer, &ok);
        if (ok)
            m.receiveSessionWindowChanged(session, window);
        else
            qDebug() << "Error in tmux %session-window-changed: invalid window-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            bool ok;
            session = parseTmuxSessionId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %session-window-changed: invalid session-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else {
            lexBuffer.push_back(cc);
        }
    }

    int arg;
    QVector<uint> lexBuffer;

    int session;
};

struct KONSOLEPRIVATE_EXPORT TmuxSessionsChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::SessionsChanged;
    void execute(TmuxServerManager &m)
    {
        m.receiveSessionsChanged();
    }
    void push_char(uint)
    {
    }
};

struct KONSOLEPRIVATE_EXPORT TmuxSubscriptionChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::SubscriptionChanged;
    void execute(TmuxServerManager &m)
    {
        m.receiveSubscriptionChanged(name, session, window, window_index, pane, lexBuffer);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            name = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 1 && cc == ' ') {
            bool ok;
            session = parseTmuxSessionId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %subscription-changed: invalid session-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 2 && cc == ' ') {
            bool ok;
            window = parseTmuxWindowId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %subscription-changed: invalid window-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 3 && cc == ' ') {
            bool ok;
            QString str = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            window_index = str.toInt(&ok);
            if (!ok)
                qDebug() << "Error in tmux %subscription-changed: invalid window-index: " << str;
            lexBuffer.clear();
            arg++;
        } else if (arg == 4 && cc == ' ') {
            bool ok;
            pane = parseTmuxPaneId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %subscription-changed: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 5 && cc == ' ') {
            if (lexBuffer.size() == 1 && lexBuffer[0] == ':') {
                lexBuffer.clear();
                arg++;
            } else {
                lexBuffer.clear();
            }
        } else {
            lexBuffer.push_back(cc);
        }
    }
    int arg;
    QVector<uint> lexBuffer;

    QString name;
    int session;
    int window;
    int window_index;
    int pane;
};

struct KONSOLEPRIVATE_EXPORT TmuxUnlinkedWindowAddNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::UnlinkedWindowAdd;
    void execute(TmuxServerManager &m)
    {
        m.receiveUnlinkedWindowAdd(window);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        }
        if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else {
            arg = 2;
            window = -2;
            qDebug() << "Expected window ID when receiving %unlinked-window-add: Unexpected character: " << cc;
        }
    }
    int arg;
    int window;
};

struct KONSOLEPRIVATE_EXPORT TmuxUnlinkedWindowCloseNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::UnlinkedWindowClose;
    void execute(TmuxServerManager &m)
    {
        m.receiveUnlinkedWindowClose(window);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        } else if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else {
            arg = 2;
            window = -2;
            qDebug() << "Expected window ID when receiving %unlinked-window-close: Unexpected character: " << cc;
        }
    }
    int arg;
    int window;
};

struct KONSOLEPRIVATE_EXPORT TmuxUnlinkedWindowRenamedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::UnlinkedWindowRenamed;
    void execute(TmuxServerManager &m)
    {
        m.receiveUnlinkedWindowRenamed(window);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        } else if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else {
            arg = 2;
            window = -2;
            qDebug() << "Expected window ID when receiving %unlinked-window-renamed: Unexpected character: " << cc;
        }
    }
    int arg;
    int window;
};

struct KONSOLEPRIVATE_EXPORT TmuxWindowAddNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::WindowAdd;
    void execute(TmuxServerManager &m)
    {
        m.receiveWindowAdd(window);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        } else if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else {
            arg = 2;
            window = -2;
            qDebug() << "Expected window ID when receiving %window-add: Unexpected character: " << cc;
        }
    }
    int arg;
    int window;
};

struct KONSOLEPRIVATE_EXPORT TmuxWindowCloseNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::WindowClose;
    void execute(TmuxServerManager &m)
    {
        m.receiveWindowClose(window);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        } else if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else {
            arg = 2;
            window = -2;
            qDebug() << "Expected window ID when receiving %window-close: Unexpected character: " << cc;
        }
    }
    int arg;
    int window;
};

struct KONSOLEPRIVATE_EXPORT TmuxWindowPaneChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::WindowPaneChanged;
    void execute(TmuxServerManager &m)
    {
        m.receiveWindowPaneChanged(window, pane);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == '@') {
            arg = 1;
        } else if (arg == 1 && cc > 0x30 && cc <= 0x39) {
            window = window * 10 + (cc - 0x30);
        } else if (arg == 1 && cc == ' ') {
            arg = 2;
        } else if (arg == 2 && cc == '%') {
            arg = 3;
        } else if (arg == 3 && cc > 0x30 && cc <= 0x39) {
            pane = pane * 10 + (cc - 0x30);
        } else {
            arg = 4;
            window = -2;
            qDebug() << "Expected character when receiving %window-pane-changed: " << cc;
        }
    }
    int arg;
    int window;
    int pane;
};

struct KONSOLEPRIVATE_EXPORT TmuxWindowRenamedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::WindowRenamed;
    void execute(TmuxServerManager &m)
    {
        m.receiveWindowRenamed(window, QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            bool ok;
            window = parseTmuxWindowId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %window-renamed: invalid window-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else {
            lexBuffer.push_back(cc);
        }
    }
    int arg;
    QVector<uint> lexBuffer;

    int window;
};
*/
#endif
