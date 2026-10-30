#include "settings/k4macrobackup.h"
#include "ui/fnpopupwidget.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qk4MacroBackup, "qk4.macrobackup")

namespace {

// K4 backup slot name -> QK4 function ID. Explicit rather than prefix surgery: a
// mismatch would import into slots nothing dispatches and still look successful.
const QMap<QString, QString> &slotIdMap() {
    static const QMap<QString, QString> map = {
        {"Kpod.1T", MacroIds::Kpod1T}, {"Kpod.1H", MacroIds::Kpod1H}, {"Kpod.2T", MacroIds::Kpod2T},
        {"Kpod.2H", MacroIds::Kpod2H}, {"Kpod.3T", MacroIds::Kpod3T}, {"Kpod.3H", MacroIds::Kpod3H},
        {"Kpod.4T", MacroIds::Kpod4T}, {"Kpod.4H", MacroIds::Kpod4H}, {"Kpod.5T", MacroIds::Kpod5T},
        {"Kpod.5H", MacroIds::Kpod5H}, {"Kpod.6T", MacroIds::Kpod6T}, {"Kpod.6H", MacroIds::Kpod6H},
        {"Kpod.7T", MacroIds::Kpod7T}, {"Kpod.7H", MacroIds::Kpod7H}, {"Kpod.8T", MacroIds::Kpod8T},
        {"Kpod.8H", MacroIds::Kpod8H}, {"Fn.F1", MacroIds::FnF1},     {"Fn.F2", MacroIds::FnF2},
        {"Fn.F3", MacroIds::FnF3},     {"Fn.F4", MacroIds::FnF4},     {"Fn.F5", MacroIds::FnF5},
        {"Fn.F6", MacroIds::FnF6},     {"Fn.F7", MacroIds::FnF7},     {"Fn.F8", MacroIds::FnF8},
        {"PF1", MacroIds::PF1},        {"PF2", MacroIds::PF2},        {"PF3", MacroIds::PF3},
        {"PF4", MacroIds::PF4},        {"REM.ANT", MacroIds::RemAnt},
    };
    return map;
}

} // namespace

namespace K4MacroBackup {

QMap<QString, MacroEntry> parseJson(const QByteArray &json, QString *error) {
    QMap<QString, MacroEntry> macros;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = parseError.errorString();
        return macros;
    }
    if (!doc.isObject()) {
        if (error)
            *error = "Not a JSON object";
        return macros;
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString functionId = slotIdMap().value(it.key());
        if (functionId.isEmpty()) {
            qCDebug(qk4MacroBackup) << "Skipping unknown K4 macro slot" << it.key();
            continue;
        }
        // Each slot is ["<label>", "<command>"].
        const QJsonArray pair = it.value().toArray();
        if (pair.size() < 2)
            continue;

        MacroEntry entry;
        entry.functionId = functionId;
        entry.label = pair.at(0).toString();
        entry.command = pair.at(1).toString();

        // An unassigned slot, not a macro — RadioSettings would drop it anyway.
        if (entry.command.isEmpty())
            continue;

        macros.insert(functionId, entry);
    }

    return macros;
}

QMap<QString, MacroEntry> parseFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    return parseJson(file.readAll(), error);
}

} // namespace K4MacroBackup
