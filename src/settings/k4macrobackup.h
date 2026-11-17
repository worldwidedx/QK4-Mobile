#ifndef K4MACROBACKUP_H
#define K4MACROBACKUP_H

#include <QMap>
#include <QString>
#include "settings/radiosettings.h" // MacroEntry

/**
 * @brief Reads the `k4macros.json` written by the K4's own backup utility
 *        (Fn > hold BACKUP, category "PF Key and K-Pod Macros").
 *
 * The K4 writes `K4_SN<serial>/k4macros.json` onto the backup medium, a flat object of
 * `"<slot>": ["<label>", "<command>"]`. Slot names are close to but not the same as
 * QK4's MacroIds — `Kpod.1T` vs `K-pod.1T`, `REM.ANT` vs `REM_ANT` — so the mapping
 * is explicit and unknown slots are skipped rather than guessed at.
 *
 * Import is always user-initiated: Settings > K-Pod > "Import from K4 Backup...".
 * QK4 does not go looking for USB volumes on its own.
 */
namespace K4MacroBackup {

// Parsed macros keyed by QK4 function ID (MacroIds). Slots with an empty command
// are omitted: RadioSettings::setMacro() drops those anyway, so they are not macros.
// On failure returns an empty map and, if error is non-null, sets it.
QMap<QString, MacroEntry> parseJson(const QByteArray &json, QString *error = nullptr);

// parseJson() for one file on disk.
QMap<QString, MacroEntry> parseFile(const QString &path, QString *error = nullptr);

} // namespace K4MacroBackup

#endif // K4MACROBACKUP_H
