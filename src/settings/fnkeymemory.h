#ifndef FNKEYMEMORY_H
#define FNKEYMEMORY_H

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace FnKeyMemory {

struct Entry {
    QString label;
    QString command;

    bool operator==(const Entry &other) const {
        return label == other.label && command == other.command;
    }
};

struct Setup {
    QString name = QStringLiteral("QK4 FN Key Setup");
    QMap<QString, Entry> keys;

    bool operator==(const Setup &other) const {
        return name == other.name && keys == other.keys;
    }
};

QStringList keyNames();
bool validate(const Setup &setup, QString *error = nullptr);
QJsonObject toJson(const Setup &setup);
bool fromJson(const QJsonObject &object, Setup *setup, QString *error = nullptr);

} // namespace FnKeyMemory

#endif // FNKEYMEMORY_H
