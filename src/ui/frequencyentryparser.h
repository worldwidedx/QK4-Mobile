#ifndef FREQUENCYENTRYPARSER_H
#define FREQUENCYENTRYPARSER_H

#include <QString>
#include <QtGlobal>

namespace FrequencyEntryParser {

// Parse either raw Hz digits or radio-style MHz entry. For dotted entry,
// omitted precision is zero-filled: 7.2 -> 7,200,000 Hz and
// 7.215 -> 7,215,000 Hz. A second grouping dot remains optional.
bool parse(const QString &text, quint64 *hertz);

} // namespace FrequencyEntryParser

#endif // FREQUENCYENTRYPARSER_H
