/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VT102EMULATION_H
#define VT102EMULATION_H

// Qt
#include <QHash>
#include <QMap>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPair>
#include <QQueue>
#include <QVector>

// Konsole
#include "Emulation.h"
#include "Screen.h"
#include "keyboardtranslator/KeyboardTranslator.h"

class QTimer;
class QKeyEvent;

#define MODE_AppCuKeys (MODES_SCREEN + 0) // Application cursor keys (DECCKM)
#define MODE_AppKeyPad (MODES_SCREEN + 1) //
#define MODE_Mouse1000 (MODES_SCREEN + 2) // Send mouse X,Y position on press and release
#define MODE_Mouse1001 (MODES_SCREEN + 3) // Use Highlight mouse tracking
#define MODE_Mouse1002 (MODES_SCREEN + 4) // Use cell motion mouse tracking
#define MODE_Mouse1003 (MODES_SCREEN + 5) // Use all motion mouse tracking
#define MODE_Mouse1005 (MODES_SCREEN + 6) // Xterm-style extended coordinates
#define MODE_Mouse1006 (MODES_SCREEN + 7) // 2nd Xterm-style extended coordinates
#define MODE_Mouse1007 (MODES_SCREEN + 8) // XTerm Alternate Scroll mode; also check AlternateScrolling profile property
#define MODE_Mouse1015 (MODES_SCREEN + 9) // Urxvt-style extended coordinates
#define MODE_Ansi (MODES_SCREEN + 10) // Use US Ascii for character sets G0-G3 (DECANM)
#define MODE_132Columns (MODES_SCREEN + 11) // 80 <-> 132 column mode switch (DECCOLM)
#define MODE_Allow132Columns (MODES_SCREEN + 12) // Allow DECCOLM mode
#define MODE_BracketedPaste (MODES_SCREEN + 13) // Xterm-style bracketed paste mode
#define MODE_Sixel (MODES_SCREEN + 14) // Xterm-style bracketed paste mode
#define MODE_Tmux (MODES_SCREEN + 15) // tmux control mode
#define MODE_total (MODES_SCREEN + 16)

namespace Konsole
{
extern unsigned short vt100_graphics[32];

struct CharCodes {
    // coding info
    char charset[4]; //
    int cu_cs; // actual charset.
    bool graphic; // Some VT100 tricks
    bool pound; // Some VT100 tricks
    bool sa_graphic; // saved graphic
    bool sa_pound; // saved pound
};

enum class TmuxNotificationKind {
    None,
    Response,
    Begin,
    End,
    Error,
    ClientDetached,
    ClientSessionChanged,
    ConfigError,
    Continue,
    Exit,
    ExtendedOutput,
    LayoutChange,
    Message,
    Output,
    PaneModeChanged,
    PasteBufferChanged,
    PasteBufferDeleted,
    Pause,
    SessionChanged,
    SessionRenamed,
    SessionWindowChanged,
    SessionsChanged,
    SubscriptionChanged,
    UnlinkedWindowAdd,
    UnlinkedWindowClose,
    UnlinkedWindowRenamed,
    WindowAdd,
    WindowClose,
    WindowPaneChanged,
    WindowRenamed,
};

enum class TmuxCommandKind {
    None,
    Attach,
};

class KONSOLEPRIVATE_EXPORT TmuxWindowManager : public QObject
{
    Q_OBJECT
public:
    TmuxWindowManager();
    virtual ~TmuxWindowManager() override;
};

class KONSOLEPRIVATE_EXPORT TmuxSessionManager : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QString name MEMBER m_name READ name WRITE setName)
    TmuxSessionManager(const QString &name, QObject *parent = nullptr)
        : QObject(parent)
        , m_name{name}
    {
    }
    void setName(const QString &name)
    {
        m_name = name;
    }
    QString name() const
    {
        return m_name;
    }
    virtual ~TmuxSessionManager() override;
public Q_SLOTS:
    void setGuiWindowSize(int, int);

private:
    QString m_name;
};

class KONSOLEPRIVATE_EXPORT TmuxCommand
{
public:
    TmuxCommand()
        : m_command{}
        , m_responseHandler{}
        , m_errorHandler{}
    {
    }
    template<typename F1 = std::nullptr_t,
             typename F2 = std::nullptr_t,
             typename = std::enable_if_t<std::is_convertible_v<F1, std::function<void(QList<QVector<uint>> &&)>>
                                         && std::is_convertible_v<F2, std::function<void(QList<QVector<uint>> &&)>>>>
    TmuxCommand(const QString &command, F1 &&responseHandler = nullptr, F2 &&errorHandler = nullptr)
        : m_command{command}
        , m_responseHandler{std::forward<F1>(responseHandler)}
        , m_errorHandler{std::forward<F2>(errorHandler)}
    {
        if (!m_errorHandler) {
            m_errorHandler = [this](const QList<QVector<uint>> &response) {
                qDebug() << "Received error in response to tmux command \"" << m_command << "\":";
                if (response.size() == 0)
                    qDebug() << "(empty response)";
                for (const auto &l : response)
                    qDebug() << QString::fromUcs4(l.data(), l.size());
            };
        }
    }
    TmuxCommand(const TmuxCommand &) = default;
    TmuxCommand(TmuxCommand &&) = default;
    TmuxCommand &operator=(const TmuxCommand &) = default;
    TmuxCommand &operator=(TmuxCommand &&) = default;

    QString command() const
    {
        return m_command;
    }
    void execute(QList<QVector<uint>> &&r)
    {
        Q_ASSERT(m_responseHandler);
        m_responseHandler(std::move(r));
        m_responseHandler = nullptr;
    }
    void executeError(QList<QVector<uint>> &&r)
    {
        Q_ASSERT(m_errorHandler);
        m_errorHandler(std::move(r));
        m_errorHandler = nullptr;
    }
    operator bool() const
    {
        return !!m_responseHandler && !!m_errorHandler;
    }

private:
    QString m_command;
    std::function<void(QList<QVector<uint>> &&)> m_responseHandler;
    std::function<void(QList<QVector<uint>> &&)> m_errorHandler;
};

inline void tmuxAttachHandler(QList<QVector<uint>> &&response)
{
    if (response.size() > 0) {
        qDebug() << "Unhandled data during tmux session attach:";
        for (const auto &l : response)
            qDebug() << l;
    }
};

class KONSOLEPRIVATE_EXPORT TmuxServerManager : public QObject
{
    Q_OBJECT
public:
    TmuxServerManager(QObject *parent = nullptr)
        : QObject(parent)
        , m_pendingCommands{{TmuxCommand{"", tmuxAttachHandler}}}
        , m_activeSession{-1}
        , m_initComplete(false)
    {
    }
    virtual ~TmuxServerManager() override;

    void commandResponse(const QList<QVector<uint>> &);
    void commandError(const QList<QVector<uint>> &);

    void init()
    {
        sendCommand({QString("ls -F '#{session_id} #{q:session_name}'"), [this](QList<QVector<uint>> &&response) {
                         updateSessions(std::move(response));
                     }});
    }
    void updateSessions(QList<QVector<uint>> &&response)
    {
        QVector<std::tuple<int, QString>> updatedSessions;
        for (const auto &sessionString : response) {
            int counter = 0;
            int i = 0;
            int session = 0;
            QString name{};
            int escape = false;
            for (uint cc : sessionString) {
                if (counter == 0 && cc == '$') {
                    counter++;
                    i++;
                } else if (counter == 1 && cc >= 0x30 && cc <= 0x39) {
                    session = session * 10 + (cc - 0x30);
                    i++;
                } else if (counter == 1 && cc == ' ') {
                    counter++;
                    i++;
                }
            }
            Q_ASSERT(counter == 2);
            name = QString::fromUcs4(sessionString.data() + i, sessionString.size() - i);
            updatedSessions.push_back(session, std::move(name));
        }
    }
    void receiveCommandError(QList<QVector<uint>> &&response)
    {
        if (m_currentCommand) {
            m_currentCommand.executeError(std::move(response));
        } else {
            qDebug() << "Received error in response to unexpected tmux command:";
            if (response.size() == 0)
                qDebug() << "(empty response)";
            for (const auto &l : response)
                qDebug() << QString::fromUcs4(l.data(), l.size());
        }
    }
    void receiveCommandResponse(QList<QVector<uint>> &&response)
    {
        if (m_currentCommand) {
            m_currentCommand.execute(std::move(response));
            if (m_pendingCommands.size() > 0) {
                m_currentCommand = m_pendingCommands.dequeue();
                emit doSendCommand(m_currentCommand.command());
            }
        } else {
            qDebug() << "Unexpected command response:";
            if (response.size() == 0) {
                qDebug() << "(empty)";
            } else {
                for (const auto &l : response)
                    qDebug() << l;
            }
        }
    }
    void receiveClientDetached(const QString &);
    void receiveClientSessionChanged(const QString &, int, const QString &);
    void receiveConfigError(const QString &);
    void receiveContinue(int);
    void receiveExit(const QString &);
    void receiveExtendedOutput(int, unsigned long long, const QVector<uint> &);
    void receiveLayoutChange(int, const QString &, const QString &, const QString &);
    void receiveOutput(int, const QVector<uint> &);
    void receivePaneModeChanged(int);
    void receivePasteBufferChanged(const QString &);
    void receivePasteBufferDeleted(const QString &);
    void receivePause(int);
    void receiveSessionChanged(int session, const QString &name)
    {
        if (m_sessions.contains(session)) {
            m_sessions[session]->setName(name);
        } else {
            // m_sessions.emplace(session, name);
            m_sessions.insert(session, new TmuxSessionManager(name));
            sendCommand({QString("show -v -q -t $%1 @konsole_size").arg(m_activeSession), [this, session](const QList<QVector<uint>> &response) {
                             Q_ASSERT(response.size() <= 1);
                             if (response.size() == 1) {
                                 int arg = 0;
                                 int width = 0;
                                 int height = 0;
                                 for (uint cc : response[0]) {
                                     if (cc >= 0x30 && cc <= 0x39) {
                                         if (arg == 0)
                                             width = width * 10 + (cc - 0x30);
                                         else
                                             height = height * 10 + (cc - 0x30);
                                     } else if (arg == 0 && cc == ',') {
                                         arg++;
                                     } else {
                                         qDebug() << "Unexpected character " << cc
                                                  << " when parsing @konsole_size string: " << QString::fromUcs4(response[0].data(), response[0].size());
                                         return;
                                     }
                                 }
                                 m_sessions[session]->setGuiWindowSize(width, height);
                             }
                         }});
        }
        m_activeSession = session;
    }
    void receiveSessionRenamed(const QString &);
    void receiveSessionWindowChanged(int, int);
    void receiveSessionsChanged();
    void receiveSubscriptionChanged(const QString &, int, int, int, int, const QVector<uint> &);
    void receiveUnlinkedWindowAdd(int);
    void receiveUnlinkedWindowClose(int);
    void receiveUnlinkedWindowRenamed(int);
    void receiveWindowAdd(int);
    void receiveWindowClose(int);
    void receiveWindowPaneChanged(int, int);
    void receiveWindowRenamed(int, const QString &);
public Q_SLOTS:
    void sendCommand(const TmuxCommand &command)
    {
        if (!m_currentCommand) {
            m_currentCommand = command;
            emit doSendCommand(command.command());
        } else {
            m_pendingCommands.enqueue(command);
        }
    }
Q_SIGNALS:
    void doSendCommand(const QString &);
    void clientDetached(const QString &);
    void clientSessionChanged(const QString &client, int id, const QString &name);
    void configError(const QString &);

private:
    TmuxCommand m_currentCommand;
    QQueue<TmuxCommand> m_pendingCommands;
    QHash<int, QPointer<TmuxSessionManager>> m_sessions;
    int m_activeSession;
    bool m_initComplete;
};

struct KONSOLEPRIVATE_EXPORT TmuxNullNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::None;
    constexpr void execute(TmuxServerManager &)
    {
    }
    constexpr void push_char(uint)
    {
    }
};

struct KONSOLEPRIVATE_EXPORT TmuxResponseNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Response;

    enum State { Begin, Response, End, Error, None };
    TmuxResponseNotification()
        : state(State::Begin)
        , commandResponse{}
    {
    }
    TmuxResponseNotification(const TmuxResponseNotification &) = default;
    TmuxResponseNotification(TmuxResponseNotification &&) = default;
    TmuxResponseNotification &operator=(const TmuxResponseNotification &) = default;
    TmuxResponseNotification &operator=(TmuxResponseNotification &&) = default;

    void execute(TmuxServerManager &m)
    {
        if (state == End)
            m.receiveCommandResponse(std::move(commandResponse));
        else if (state == Error)
            m.receiveCommandError(std::move(commandResponse));
        else
            qDebug() << "Executed TmuxResponseNotification in invalid state " << (int)state;
        state = None;
    }
    void push_char(uint cc)
    {
        if (state == State::Response) {
            commandResponse.last().push_back(cc);
        }
    }
    State state;
    QList<QVector<uint>> commandResponse;
};

struct KONSOLEPRIVATE_EXPORT TmuxClientDetachedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ClientDetached;
    void execute(TmuxServerManager &m)
    {
        m.receiveClientDetached(QString::fromUcs4(client.data(), client.size()));
    }
    void push_char(uint cc)
    {
        client.push_back(cc);
    }
    QVector<uint> client;
};

template<char C>
inline int parseTmuxId(const QVector<uint> lexBuffer, bool *ok = nullptr)
{
    int ret;
    if (lexBuffer.size() == 2 && lexBuffer[0] == C && lexBuffer[1] == '*') {
        ret = -1;
        if (ok)
            *ok = true;
    } else if (lexBuffer.size() >= 2 && lexBuffer[0] == C) {
        auto str = QString::fromUcs4(lexBuffer.data() + 1, lexBuffer.size() - 1);
        ret = str.toInt(ok);
    } else if (ok) {
        *ok = false;
        ret = -2;
    }
    return ret;
}

inline int parseTmuxSessionId(const QVector<uint> lexBuffer, bool *ok = nullptr)
{
    return parseTmuxId<'$'>(lexBuffer, ok);
}

inline int parseTmuxWindowId(const QVector<uint> lexBuffer, bool *ok = nullptr)
{
    return parseTmuxId<'@'>(lexBuffer, ok);
}

inline int parseTmuxPaneId(const QVector<uint> lexBuffer, bool *ok = nullptr)
{
    return parseTmuxId<'%'>(lexBuffer, ok);
}

struct KONSOLEPRIVATE_EXPORT TmuxClientSessionChangedNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ClientSessionChanged;
    TmuxClientSessionChangedNotification()
        : arg{}
        , lexBuffer{}
        , client{}
        , session{-2}
    {
    }
    TmuxClientSessionChangedNotification(const TmuxClientSessionChangedNotification &) = default;
    TmuxClientSessionChangedNotification(TmuxClientSessionChangedNotification &&) = default;
    TmuxClientSessionChangedNotification &operator=(const TmuxClientSessionChangedNotification &) = default;
    TmuxClientSessionChangedNotification &operator=(TmuxClientSessionChangedNotification &&) = default;
    void execute(TmuxServerManager &m)
    {
        m.receiveClientSessionChanged(client, session, QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        if (cc == ' ' && arg == 0) {
            client = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (cc == ' ' && arg == 1) {
            bool ok;
            session = parseTmuxSessionId(lexBuffer, &ok);
            if (!ok) {
                qDebug() << "Error in tmux %client-session-changed: invalid session: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
                session = -2;
            }
            lexBuffer.clear();
            arg++;
        } else {
            lexBuffer.push_back(cc);
        }
    }

    int arg;
    QVector<uint> lexBuffer;

    QString client;
    int session;
};

struct KONSOLEPRIVATE_EXPORT TmuxConfigErrorNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ConfigError;
    void execute(TmuxServerManager &m)
    {
        m.receiveConfigError(QString::fromUcs4(error.data(), error.size()));
    }
    void push_char(uint cc)
    {
        error.push_back(cc);
    }
    QVector<uint> error;
};

struct KONSOLEPRIVATE_EXPORT TmuxContinueNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Continue;
    void execute(TmuxServerManager &m)
    {
        bool ok;
        int pane = parseTmuxPaneId(lexBuffer, &ok);
        if (ok) {
            m.receiveContinue(pane);
        } else {
            qDebug() << "Error in tmux %continue: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
        }
        lexBuffer.clear();
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxExitNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::Exit;
    void execute(TmuxServerManager &m)
    {
        m.receiveExit(QString::fromUcs4(lexBuffer.data(), lexBuffer.size()));
    }
    void push_char(uint cc)
    {
        lexBuffer.push_back(cc);
    }
    QVector<uint> lexBuffer;
};

struct KONSOLEPRIVATE_EXPORT TmuxExtendedOutputNotification {
    static constexpr TmuxNotificationKind Kind = TmuxNotificationKind::ExtendedOutput;
    void execute(TmuxServerManager &m)
    {
        m.receiveExtendedOutput(pane, age, lexBuffer);
    }
    void push_char(uint cc)
    {
        if (arg == 0 && cc == ' ') {
            bool ok;
            pane = parseTmuxPaneId(lexBuffer, &ok);
            if (!ok)
                qDebug() << "Error in tmux %extended-output: invalid pane-id: " << QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            lexBuffer.clear();
            arg++;
        } else if (arg == 1 && cc == ' ') {
            bool ok;
            auto str = QString::fromUcs4(lexBuffer.data(), lexBuffer.size());
            age = str.toULongLong(&ok);
            if (!ok)
                qDebug() << "Error in tmux %extended-output: invalid age: " << str;
            lexBuffer.clear();
            arg++;
        } else if (arg == 2 && cc == ' ') {
            if (lexBuffer.size() == 1 && lexBuffer[0] == ':') {
                lexBuffer.clear();
                arg++;
            } else {
                lexBuffer.clear();
            }
        } else if (arg < 2) {
            lexBuffer.push_back(cc);
        } else if (arg == 3) {
            // TODO: confirm %extended-output is escaped like %output
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

using TmuxNotificationVariant = std::variant<TmuxNullNotification,
                                             TmuxResponseNotification,
                                             TmuxClientDetachedNotification,
                                             TmuxClientSessionChangedNotification,
                                             TmuxConfigErrorNotification,
                                             TmuxContinueNotification,
                                             TmuxExitNotification,
                                             TmuxExtendedOutputNotification,
                                             TmuxLayoutChangeNotification,
                                             TmuxMessageNotification,
                                             TmuxOutputNotification,
                                             TmuxPaneModeChangedNotification,
                                             TmuxPasteBufferChangedNotification,
                                             TmuxPasteBufferDeletedNotification,
                                             TmuxPauseNotification,
                                             TmuxSessionChangedNotification,
                                             TmuxSessionRenamedNotification,
                                             TmuxSessionWindowChangedNotification,
                                             TmuxSessionsChangedNotification,
                                             TmuxSubscriptionChangedNotification,
                                             TmuxUnlinkedWindowAddNotification,
                                             TmuxUnlinkedWindowCloseNotification,
                                             TmuxUnlinkedWindowRenamedNotification,
                                             TmuxWindowAddNotification,
                                             TmuxWindowCloseNotification,
                                             TmuxWindowPaneChangedNotification,
                                             TmuxWindowRenamedNotification>;

TmuxNotificationVariant makeNotification(TmuxNotificationKind);

extern const QHash<QVector<uint>, TmuxNotificationKind> tmuxCommandLookup;

/**
 * Provides an xterm compatible terminal emulation based on the DEC VT102 terminal.
 * A full description of this terminal can be found at https://vt100.net/docs/vt102-ug/
 *
 * In addition, various additional xterm escape sequences are supported to provide
 * features such as mouse input handling.
 * See https://invisible-island.net/xterm/ctlseqs/ctlseqs.html for a description of xterm's escape
 * sequences.
 *
 */
class KONSOLEPRIVATE_EXPORT Vt102Emulation : public Emulation
{
    Q_OBJECT

public:
    /** Constructs a new emulation */
    Vt102Emulation();
    ~Vt102Emulation() override;

    // reimplemented from Emulation
    void clearEntireScreen() override;
    void reset(bool softReset = false, bool preservePrompt = false) override;
    char eraseChar() const override;

public Q_SLOTS:
    // reimplemented from Emulation
    void sendString(const QByteArray &string) override;
    void sendText(const QString &text) override;
    void sendKeyEvent(QKeyEvent *) override;
    void sendMouseEvent(int buttons, int column, int line, int eventType) override;
    void focusChanged(bool focused) override;
    void clearHistory() override;

protected:
    // reimplemented from Emulation
    void setMode(int mode) override;
    void resetMode(int mode) override;
    void receiveChars(const QVector<uint> &chars, int start = 0, int end = -1) override;

private Q_SLOTS:
    // Causes sessionAttributeChanged() to be emitted for each (int,QString)
    // pair in _pendingSessionAttributesUpdates.
    // Used to buffer multiple attribute updates in the current session
    void updateSessionAttributes();
    void deletePlayer(QMediaPlayer::MediaStatus);

private:
    unsigned int applyCharset(uint c);
    void setCharset(int n, int cs);
    void useCharset(int n);
    void setAndUseCharset(int n, int cs);
    void saveCursor();
    void restoreCursor();
    void resetCharset(int scrno);

    void setMargins(int top, int bottom);
    // set margins for all screens back to their defaults
    void setDefaultMargins();

    // returns true if 'mode' is set or false otherwise
    bool getMode(int mode);
    // saves the current boolean value of 'mode'
    void saveMode(int mode);
    // restores the boolean value of 'mode'
    void restoreMode(int mode);
    // resets all modes
    // (except MODE_Allow132Columns)
    void resetModes();

    void resetTokenizer();
#define MAX_TOKEN_LENGTH 256 // Max length of tokens (e.g. window title)
    void addToCurrentToken(uint cc);
    int tokenBufferPos;

protected:
    uint tokenBuffer[MAX_TOKEN_LENGTH]; // FIXME: overflow?

private:
#define MAXARGS 16
    void addDigit(int dig);
    void addArgument();
    void addSub();

    struct subParam {
        int value[MAXARGS]; // value[0] unused, it would correspond to the containing param value
        int count;
    };

    struct {
        int value[MAXARGS];
        struct subParam sub[MAXARGS];
        int count;
        bool hasSubParams;
    } params = {};

    void initTokenizer();

    enum ParserStates {
        Ground,
        Escape,
        EscapeIntermediate,
        CsiEntry,
        CsiParam,
        CsiIntermediate,
        CsiIgnore,
        DcsEntry,
        DcsParam,
        DcsIntermediate,
        DcsPassthrough,
        DcsIgnore,
        OscString,
        SosPmApcString,

        TmuxRead,
        TmuxConsume,
        TmuxError,

        Vt52Escape,
        Vt52CupRow,
        Vt52CupColumn,
    };

    enum {
        Sos,
        Pm,
        Apc,
    } _sosPmApc;

    enum osc {
        // https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Operating-System-Commands
        ReportColors = 4,
        ResetColors = 104,
        // https://gitlab.freedesktop.org/Per_Bothner/specifications/blob/master/proposals/semantic-prompts.md
        SemanticPrompts = 133,
        // https://chromium.googlesource.com/apps/libapps/+/master/hterm/doc/ControlSequences.md#OSC
        Notification = 777,
        Image = 1337,
    };

    ParserStates _state = Ground;
    bool _ignore = false;
    int _nIntermediate = 0;
    unsigned char _intermediate[1];

    void switchState(const ParserStates newState, const uint cc);
    void esc_dispatch(const uint cc);
    void clear();
    void collect(const uint cc);
    void param(const uint cc);
    void csi_dispatch(const uint cc);
    void osc_start();
    void osc_put(const uint cc);
    void osc_end(const uint cc);
    void hook(const uint cc);
    void unhook();
    void put(const uint cc);
    void apc_start(const uint cc);
    void apc_put(const uint cc);
    void apc_end();
    void tmux_lex(const uint cc);
    void tmux_parse();
    void tmux_reset_response();
    void tmux_reset_notification();

    // State machine for escape sequences containing large amount of data
    int tokenState;
    const char *tokenStateChange;
    int tokenPos;
    QByteArray tokenData;

    // Set of flags for each of the ASCII characters which indicates
    // what category they fall into (printable character, control, digit etc.)
    // for the purposes of decoding terminal output
    int charClass[256];

    QByteArray imageData;
    quint32 imageId;
    QMap<char, qint64> savedKeys;

protected:
    virtual void reportDecodingError(int token);

    virtual void processToken(int code, int p, int q);
    virtual void processSessionAttributeRequest(const int tokenSize, const uint terminator);
    virtual void processChecksumRequest(int argc, int argv[]);

private:
    void processGraphicsToken(int tokenSize);

    void sendGraphicsReply(const QString &params, const QString &error);
    void reportTerminalType();
    void reportTertiaryAttributes();
    void reportSecondaryAttributes();
    void reportVersion();
    void reportStatus();
    void reportAnswerBack();
    void reportCursorPosition();
    void reportPixelSize();
    void reportCellSize();
    void iTermReportCellSize();
    void reportSize();
    void reportColor(int c, QColor color);
    void reportTerminalParms(int p);

    void emulateUpDown(bool up, KeyboardTranslator::Entry entry, QByteArray &textToSend, int toCol = -1);

    // clears the screen and resizes it to the specified
    // number of columns
    void clearScreenAndSetColumns(int columnCount);

    CharCodes _charset[2];

    class TerminalState
    {
    public:
        // Initializes all modes to false
        TerminalState()
        {
            memset(&mode, 0, MODE_total * sizeof(bool));
        }

        bool mode[MODE_total];
    };

    TerminalState _currentModes;
    TerminalState _savedModes;

    QList<QVector<uint>> _tmuxCommandNotification;
    QList<QVector<uint>> _tmuxCommandResponse;
    QVector<uint> _tmuxNotificationLine;
    TmuxNotificationKind _tmuxNotification;
    int _tmuxNotificationArgc;
    TmuxServerManager *_tmuxServerManager;
    TmuxNotificationVariant _tmuxNotificationVariant;

    // Hash table and timer for buffering calls to update certain session
    // attributes (e.g. the name of the session, window title).
    // These calls occur when certain escape sequences are detected in the
    // output from the terminal. See Emulation::sessionAttributeChanged()
    QHash<int, QString> _pendingSessionAttributesUpdates;
    QTimer *_sessionAttributesUpdateTimer;

    bool _reportFocusEvents;

    QColor colorTable[256];

    // Sixel:
#define MAX_SIXEL_COLORS 256
#define MAX_IMAGE_DIM 16384
    void sixelQuery(int query);
    bool processSixel(uint cc);
    void SixelModeEnable(int width, int height /*, bool preserveBackground*/);
    void SixelModeAbort();
    void SixelModeDisable();
    void SixelColorChangeRGB(const int index, int red, int green, int blue);
    void SixelColorChangeHSL(const int index, int hue, int saturation, int value);
    void SixelCharacterAdd(uint8_t character, int repeat = 1);
    bool m_SixelPictureDefinition = false;
    bool m_SixelStarted = false;
    QImage m_currentImage;
    int m_currentX = 0;
    int m_verticalPosition = 0;
    uint8_t m_currentColor = 0;
    bool m_preserveBackground = true;
    int m_previousWidth = 128;
    int m_previousHeight = 32;
    QPair<int, int> m_aspect = qMakePair(1, 1);
    bool m_SixelScrolling = true;
    QSize m_actualSize; // For efficiency reasons, we keep the image in memory larger than what the end result is

    // Kitty
    QHash<int, QPixmap> _graphicsImages;
    // For kitty graphics protocol - image cache
    int getFreeGraphicsImageId();

    QMediaPlayer *player;
};

}

#endif // VT102EMULATION_H
