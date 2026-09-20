#pragma once
#include "ft8/ft8types.h"
#include <QColor>

namespace Ft8ActivityStyle {
// Familiar WSJT-X meanings; selected rows retain these backgrounds.
inline QColor background(const Ft8::Decode &decode, const QString &myCall, bool transmitted, bool worked) {
    if (transmitted)
        return QColor("#ffff00");
    const auto words = decode.message.split(' ', Qt::SkipEmptyParts);
    if (!myCall.isEmpty() && (words.contains(myCall) || words.contains("<" + myCall + ">")))
        return QColor("#ffaaaa");
    if (worked)
        return QColor("#d0d0d0");
    if (Ft8::parseMessage(decode.message).cq)
        return QColor("#aaffaa");
    return QColor("#ffffff");
}
} // namespace Ft8ActivityStyle
