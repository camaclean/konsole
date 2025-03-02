#include "TmuxLayout.h"
#include "Utilities.h"

using namespace Konsole;

bool TmuxLayoutParser::parseChar(uint cc)
{
    if (_state == ParseChecksum) {
        if (cc == ',') {
            _state = ParseSizeHoriz;
            _layout.checksum = _checksum;
            _checksum = 0;
            _layout.layout = QSharedPointer<TmuxLayoutElement>::create();
            _current = _layout.layout;
            return true;
        } else {
            return parseHexChar(cc, this->_checksum);
        }
    } else if (_state == ParseSizeHoriz) {
        addChecksum(cc);
        if (cc == 'x') {
            _state = ParseSizeVert;
            return true;
        } else
            return parseDecChar(cc, _current->size.rwidth());
    } else if (_state == ParseSizeVert) {
        addChecksum(cc);
        if (cc == ',') {
            _state = ParsePosHoriz;
            return true;
        } else
            return parseDecChar(cc, _current->size.rheight());
    } else if (_state == ParsePosHoriz) {
        addChecksum(cc);
        if (cc == ',') {
            _state = ParsePosVert;
            return true;
        } else
            return parseDecChar(cc, _current->position.rx());
    } else if (_state == ParsePosVert) {
        addChecksum(cc);
        if (cc == ',') {
            _state = ParsePane;
            return true;
        } else if (cc == '{') {
            newFrame(false);
            _state = ParseSizeHoriz;
            return true;
        } else if (cc == '[') {
            newFrame(true);
            _state = ParseSizeHoriz;
            return true;
        } else
            return parseDecChar(cc, _current->position.ry());
    } else if (_state == ParsePane) {
        addChecksum(cc);
        if (!parseDecChar(cc, _current->id)) {
            return end(cc);
        } else
            return true;
    } else if (_state == EndFrame) {
        addChecksum(cc);
        return end(cc);
    }
    return false;
}

bool TmuxLayoutParser::parse(const QString &s)
{
    for (QChar cc : s) {
        if (!parseChar(cc.unicode()))
            return false;
    }
    return isValid();
}

bool TmuxLayoutParser::parse(const QVector<uint> &s)
{
    {
        for (uint cc : s) {
            if (!parseChar(cc))
                return false;
        }
        return isValid();
    }
}

void TmuxLayoutParser::addChecksum(uint cc)
{
    _checksum = (_checksum >> 1) + ((_checksum & 1) << 15) + cc;
}

void TmuxLayoutParser::newPane()
{
    Q_ASSERT(_current);
    auto frame = _current->frame;
    _current->next = QSharedPointer<TmuxLayoutElement>::create();
    _current = _current->next;
    _current->frame = frame;
}

void TmuxLayoutParser::newFrame(bool vertical)
{
    _current->id = vertical ? -3 : -2;
    _current->child = QSharedPointer<TmuxLayoutElement>::create();
    _current->child->frame = _current;
    _current = _current->child;
}

bool TmuxLayoutParser::end(uint cc)
{
    if (cc == ',') {
        newPane();
        _state = ParseSizeHoriz;
        return true;
    } else if (cc == '}') {
        _current = _current->frame;
        _state = EndFrame;
        return true;
    } else if (cc == ']') {
        _current = _current->frame;
        _state = EndFrame;
        return true;
    } else {
        return parseDecChar(cc, _current->id);
    }
    return false;
}
