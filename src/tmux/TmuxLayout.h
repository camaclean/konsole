#ifndef TMUX__TMUXLAYOUT_H
#define TMUX__TMUXLAYOUT_H

#include "Utilities.h"
#include "konsoleprivate_export.h"

#include <QPoint>
#include <QSharedPointer>
#include <QSize>

namespace Konsole
{

struct KONSOLEPRIVATE_EXPORT TmuxLayoutElement {
    QSize size;
    QPoint position;
    int id;
    QSharedPointer<TmuxLayoutElement> next;
    QSharedPointer<TmuxLayoutElement> child;
    QSharedPointer<TmuxLayoutElement> frame;
};

struct KONSOLEPRIVATE_EXPORT TmuxLayout {
    ushort checksum;
    QSharedPointer<TmuxLayoutElement> layout;
};

class KONSOLEPRIVATE_EXPORT TmuxLayoutParser
{
    enum State {
        ParseChecksum,
        ParseSizeHoriz,
        ParseSizeVert,
        ParsePosHoriz,
        ParsePosVert,
        ParsePane,
        EndFrame,
    };

public:
    ushort checksum() const
    {
        return _checksum;
    }
    const TmuxLayout &layout() const &
    {
        return _layout;
    }
    TmuxLayout &&layout() &&
    {
        return std::move(_layout);
    }

    bool parseChar(uint cc);
    bool parse(const QString &s);
    bool parse(const QVector<uint> &s);

    bool isValid() const
    {
        return _checksum == _layout.checksum && !_current->frame;
    }

private:
    void addChecksum(uint cc);
    void newPane();
    void newFrame(bool vertical);
    bool end(uint cc);

    ushort _checksum;
    TmuxLayout _layout;
    State _state;
    QSharedPointer<TmuxLayoutElement> _current;
};

}

#endif
