#include "frequencyentryparser.h"

#include <QStringList>

namespace FrequencyEntryParser {

namespace {
bool isDigits(const QString &text) {
    if (text.isEmpty())
        return false;
    for (const QChar character : text) {
        if (!character.isDigit())
            return false;
    }
    return true;
}
} // namespace

bool parse(const QString &text, quint64 *hertz) {
    if (!hertz)
        return false;

    const QString input = text.trimmed();
    if (input.isEmpty())
        return false;

    if (!input.contains('.')) {
        bool ok = false;
        const quint64 rawHertz = input.toULongLong(&ok);
        if (!ok || !isDigits(input))
            return false;
        *hertz = rawHertz;
        return true;
    }

    const QStringList groups = input.split('.', Qt::KeepEmptyParts);
    if (groups.size() < 2 || groups.size() > 3 || !isDigits(groups[0]))
        return false;

    QString fractionalMhz;
    if (groups.size() == 2) {
        // Decimal-MHz shorthand: up to six fractional digits.
        if (!isDigits(groups[1]) || groups[1].size() > 6)
            return false;
        fractionalMhz = groups[1].leftJustified(6, '0');
    } else {
        // Optional grouped form MHz.kHz.Hz. Each incomplete group implies
        // trailing zeros: 7.2.5 means 7.200.500 MHz/Hz grouping.
        if (!isDigits(groups[1]) || !isDigits(groups[2]) ||
            groups[1].size() > 3 || groups[2].size() > 3) {
            return false;
        }
        fractionalMhz = groups[1].leftJustified(3, '0')
                + groups[2].leftJustified(3, '0');
    }

    bool mhzOk = false;
    bool fractionOk = false;
    const quint64 wholeMhz = groups[0].toULongLong(&mhzOk);
    const quint64 fractionHz = fractionalMhz.toULongLong(&fractionOk);
    if (!mhzOk || !fractionOk || wholeMhz > 9999)
        return false;

    *hertz = wholeMhz * 1000000ULL + fractionHz;
    return true;
}

} // namespace FrequencyEntryParser
