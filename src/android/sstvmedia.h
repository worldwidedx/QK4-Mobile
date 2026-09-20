#ifndef SSTVMEDIA_H
#define SSTVMEDIA_H

#include <QString>

// Android-only media acquisition boundary.  The UI receives an imported,
// app-private path rather than handling content URIs, intents, or permissions.
namespace SstvMedia {
bool openGallery(QString *error = nullptr);
bool openCamera(QString *error = nullptr);
bool shareImage(const QString &path, const QString &title, QString *error = nullptr);
QString takeCompletedImagePath();
bool isOperationActive();
QString takeOperationError();
}

#endif // SSTVMEDIA_H
