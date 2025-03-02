#ifndef TMUX__TMUXSESSIONMANAGER_H
#define TMUX__TMUXSESSIONMANAGER_H

namespace Konsole
{

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
    virtual ~TmuxSessionManager() override { };
public Q_SLOTS:
    void setGuiWindowSize(int, int);

private:
    QString m_name;
};

}

#endif // TMUX__TMUXSESSIONMANAGER_H
