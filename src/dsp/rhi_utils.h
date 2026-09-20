#ifndef RHI_UTILS_H
#define RHI_UTILS_H

#include <QFile>
#include <rhi/qshader.h>
#include <QDebug>

namespace RhiUtils {

// Shared std140 layout used by both PAN renderers and waterfall.vert/.frag.
struct alignas(16) WaterfallUniforms {
    float scrollOffset, binCount, textureWidth, padding;
    float viewStart, viewWidth, pad1, pad2;
};
static_assert(sizeof(WaterfallUniforms) == 32);

// Load a compiled shader from a .qsb resource file
// Returns invalid QShader on failure (check with isValid())
inline QShader loadShader(const QString &path) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        return QShader::fromSerialized(f.readAll());
    }
    qWarning() << "RhiUtils: Failed to load shader:" << path;
    return QShader();
}

// K4 spectrum calibration constant (shared between panadapter and minipan)
// dBm = raw_byte - K4_DBM_OFFSET
constexpr float K4_DBM_OFFSET = 146.0f;

} // namespace RhiUtils

#endif // RHI_UTILS_H
