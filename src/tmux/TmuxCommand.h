#ifndef TMUX__TMUXCOMMAND_H
#define TMUX__TMUXCOMMAND_H

#include <QtCore>

#include <type_traits>

#include "konsoleprivate_export.h"

namespace Konsole
{

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

}

#endif
