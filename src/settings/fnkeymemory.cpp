#include "fnkeymemory.h"

#include <QJsonArray>
#include <QJsonValue>

namespace FnKeyMemory {
namespace {

constexpr int FileVersion = 1;
constexpr int MaximumNameLength = 64;
constexpr int MaximumLabelLength = 12;
constexpr int MaximumCommandLength = 64;

bool fail(QString *error, const QString &message) {
    if (error)
        *error = message;
    return false;
}

bool containsControlCharacter(const QString &text) {
    for (const QChar character : text) {
        if (character.unicode() < 0x20)
            return true;
    }
    return false;
}

bool validCommand(const QString &command, QString *error) {
    if (command.isEmpty())
        return true;
    if (command != command.trimmed())
        return fail(error, QStringLiteral("Command cannot start or end with whitespace"));
    if (command.size() > MaximumCommandLength)
        return fail(error, QStringLiteral("Command exceeds the 64-character FN editor limit"));
    if (!command.endsWith(QLatin1Char(';')))
        return fail(error, QStringLiteral("Command must end with a semicolon"));
    for (const QChar character : command) {
        const ushort value = character.unicode();
        if (value < 0x20 || value > 0x7e)
            return fail(error, QStringLiteral("Command must contain printable ASCII characters only"));
    }
    return true;
}

} // namespace

QStringList keyNames() {
    return {QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"),
            QStringLiteral("F4"), QStringLiteral("F5"), QStringLiteral("F6"),
            QStringLiteral("F7"), QStringLiteral("F8")};
}

bool validate(const Setup &setup, QString *error) {
    const QString name = setup.name.trimmed();
    if (name.isEmpty() || name.size() > MaximumNameLength || containsControlCharacter(name))
        return fail(error, QStringLiteral("FN setup name is invalid"));

    const QStringList expectedKeys = keyNames();
    if (setup.keys.size() != expectedKeys.size())
        return fail(error, QStringLiteral("An FN file must contain exactly F1 through F8"));

    for (const QString &key : expectedKeys) {
        if (!setup.keys.contains(key))
            return fail(error, QStringLiteral("FN file is missing %1").arg(key));
        const Entry &entry = setup.keys.value(key);
        if (entry.label.size() > MaximumLabelLength || containsControlCharacter(entry.label))
            return fail(error, QStringLiteral("%1 label exceeds the 12-character limit").arg(key));
        QString commandError;
        if (!validCommand(entry.command, &commandError))
            return fail(error, QStringLiteral("%1: %2").arg(key, commandError));
    }

    if (error)
        error->clear();
    return true;
}

QJsonObject toJson(const Setup &setup) {
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("qk4-fn-key-mapping"));
    root.insert(QStringLiteral("version"), FileVersion);
    root.insert(QStringLiteral("name"), setup.name);

    QJsonArray keys;
    for (const QString &key : keyNames()) {
        const Entry entry = setup.keys.value(key);
        QJsonObject item;
        item.insert(QStringLiteral("key"), key);
        item.insert(QStringLiteral("label"), entry.label);
        item.insert(QStringLiteral("command"), entry.command);
        keys.append(item);
    }
    root.insert(QStringLiteral("keys"), keys);
    return root;
}

bool fromJson(const QJsonObject &root, Setup *setup, QString *error) {
    if (!setup)
        return fail(error, QStringLiteral("No destination FN setup was provided"));
    if (root.value(QStringLiteral("format")).toString()
        != QStringLiteral("qk4-fn-key-mapping")) {
        return fail(error, QStringLiteral("Not a QK4 FN key file"));
    }
    if (root.value(QStringLiteral("version")).toInt(-1) != FileVersion)
        return fail(error, QStringLiteral("Unsupported QK4 FN key file version"));
    if (!root.value(QStringLiteral("name")).isString())
        return fail(error, QStringLiteral("FN setup name is missing"));
    if (!root.value(QStringLiteral("keys")).isArray())
        return fail(error, QStringLiteral("FN key list is missing"));

    Setup parsed;
    parsed.name = root.value(QStringLiteral("name")).toString().trimmed();
    const QStringList expectedKeys = keyNames();
    const QJsonArray keys = root.value(QStringLiteral("keys")).toArray();
    for (const QJsonValue &value : keys) {
        if (!value.isObject())
            return fail(error, QStringLiteral("Invalid FN key entry"));
        const QJsonObject item = value.toObject();
        if (!item.value(QStringLiteral("key")).isString()
            || !item.value(QStringLiteral("label")).isString()
            || !item.value(QStringLiteral("command")).isString()) {
            return fail(error, QStringLiteral("FN key entry is incomplete"));
        }

        const QString key = item.value(QStringLiteral("key")).toString().toUpper();
        if (!expectedKeys.contains(key))
            return fail(error, QStringLiteral("Unknown FN key %1").arg(key));
        if (parsed.keys.contains(key))
            return fail(error, QStringLiteral("FN key %1 appears more than once").arg(key));

        Entry entry{item.value(QStringLiteral("label")).toString(),
                    item.value(QStringLiteral("command")).toString()};
        if (entry.command.isEmpty())
            entry.label.clear();
        parsed.keys.insert(key, entry);
    }

    if (!validate(parsed, error))
        return false;
    *setup = parsed;
    return true;
}

} // namespace FnKeyMemory
