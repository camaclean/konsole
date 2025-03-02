#ifndef TMUX__TMUXSERVERMANAGER_H
#define TMUX__TMUXSERVERMANAGER_H

#include "TmuxCommand.h"
#include "TmuxNotificationParser.h"
#include "TmuxSessionManager.h"
#include "konsoleprivate_export.h"

#include <QtCore>

namespace Konsole
{

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
    virtual ~TmuxServerManager() override { };

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
            // int escape = false;
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
            updatedSessions.push_back({session, std::move(name)});
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
    TmuxNotificationParser m_parser;
    TmuxCommand m_currentCommand;
    QQueue<TmuxCommand> m_pendingCommands;
    QHash<int, QPointer<TmuxSessionManager>> m_sessions;
    int m_activeSession;
    bool m_initComplete;
};

}

#endif
