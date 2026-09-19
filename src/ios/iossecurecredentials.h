#ifndef IOSSECURECREDENTIALS_H
#define IOSSECURECREDENTIALS_H

#include <QString>

namespace IosSecureCredentials {
QString readQrzApiKey(QString *error);
bool writeQrzApiKey(const QString &key, QString *error);
bool clearQrzApiKey(QString *error);
}

#endif // IOSSECURECREDENTIALS_H
