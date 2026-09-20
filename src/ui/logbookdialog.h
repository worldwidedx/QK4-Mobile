#pragma once
#include "ft8/ft8logbook.h"
class QWidget;
class QrzLogbook;

// Shared ADIF log UI. All entry points use the same production file; practice
// callers explicitly supply their separate in-memory log.
namespace LogbookUi {
QString storagePath();
AdifRecord contact(const QString &call, const QString &mode, qint64 frequencyHz,
                   const QString &operatorCall = {});
bool edit(QWidget *parent, Ft8Logbook &log, const AdifRecord &record,
          int index = -1, bool callsignOnly = false);
void show(QWidget *parent, Ft8Logbook &log, const AdifRecord &defaults = {},
          bool practice = false, QrzLogbook *uploads = nullptr);
}
