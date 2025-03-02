#ifndef TMUX__UTILITIES_H
#define TMUX__UTILITIES_H

#include <QtCore>

#include <concepts>

namespace Konsole
{

template<std::integral T>
inline bool parseHexChar(uint cc, T &num)
{
    if (cc >= 0x30 && cc <= 0x39) {
        cc -= 0x30;
    } else if (cc >= 0x61 && cc <= 0x66) {
        cc = cc - 0x61 + 10;
    } else if (cc >= 0x41 && cc <= 0x46) {
        cc = cc - 0x41 + 10;
    } else {
        return false;
    }
    num = (num << 4) | static_cast<T>(cc);
    return true;
}

template<std::integral T>
inline bool parseDecChar(uint cc, T &num)
{
    if (cc >= 0x30 && cc <= 0x39) {
        num = num * 10 + (cc - 0x30);
        return true;
    } else {
        return false;
    }
}

}

#endif
