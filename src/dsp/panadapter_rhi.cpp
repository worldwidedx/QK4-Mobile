#include "panadapter_rhi.h"
#include "rhi_utils.h"
#include "ui/k4styles.h"
#include <QFile>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QtMath>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <functional>
#include <vector>

// Transparent overlay widget for dBm/S-unit scale labels
class DbmScaleOverlay : public QWidget {
public:
    DbmScaleOverlay(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setDbRange(float minDb, float maxDb) {
        m_minDb = minDb;
        m_maxDb = maxDb;
        update();
    }

    void setUseSUnits(bool useSUnits) {
        if (m_useSUnits != useSUnits) {
            m_useSUnits = useSUnits;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont scaleFont = K4Styles::Fonts::dataFont(K4Styles::Dimensions::FontSizeNormal, QFont::Normal);
        painter.setFont(scaleFont);
        painter.setPen(Qt::white);

        const int labelCount = 9; // Keep 9 divisions for grid alignment
        const float dbRange = m_maxDb - m_minDb;
        const int leftMargin = 4;
        const int h = height();

        QFontMetrics fm(scaleFont);
        int textHeight = fm.height();

        for (int i = 0; i < labelCount; ++i) {
            // Skip top and bottom labels for breathing room
            if (i == 0 || i == labelCount - 1)
                continue;

            float dbValue = m_maxDb - (static_cast<float>(i) / 8.0f) * dbRange;
            int yPos = h * i / 8;

            QString label;
            if (m_useSUnits) {
                label = dbmToSUnits(dbValue);
            } else {
                label = QString("%1 dBm").arg(static_cast<int>(dbValue));
            }

            // Vertically center on grid line
            int textY = yPos + textHeight / 3;

            painter.drawText(leftMargin, textY, label);
        }
    }

private:
    // Convert dBm to S-unit string
    // S9 = -73 dBm, each S-unit below is 6 dB
    QString dbmToSUnits(float dbm) const {
        const float s9Dbm = -73.0f;
        const float dbPerSUnit = 6.0f;

        if (dbm >= s9Dbm) {
            // Above S9: show as S9+XX
            int dbOver = static_cast<int>(std::round(dbm - s9Dbm));
            if (dbOver == 0) {
                return "S9";
            }
            return QString("S9+%1").arg(dbOver);
        } else {
            // Below S9: calculate S-unit (S1-S9)
            float sUnits = 9.0f + (dbm - s9Dbm) / dbPerSUnit;
            int sValue = static_cast<int>(std::round(sUnits));
            if (sValue < 1)
                sValue = 1;
            if (sValue > 9)
                sValue = 9;
            return QString("S%1").arg(sValue);
        }
    }

    float m_minDb = -138.0f;
    float m_maxDb = -58.0f;
    bool m_useSUnits = false;
};

// Transparent overlay widget for frequency scale labels at spectrum/waterfall boundary
class FrequencyScaleOverlay : public QWidget {
public:
    void setAudioUnits(bool enabled) { m_audioUnits = enabled; update(); }
    FrequencyScaleOverlay(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setFrequencyRange(qint64 centerFreq, int spanHz, int cwPitch, const QString &mode) {
        m_centerFreq = centerFreq;
        m_spanHz = spanHz;
        m_cwPitch = cwPitch;
        m_mode = mode;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (m_spanHz <= 0)
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        QFont scaleFont = K4Styles::Fonts::dataFont(K4Styles::Dimensions::FontSizeNormal, QFont::Normal);
        painter.setFont(scaleFont);
        painter.setPen(Qt::white);

        QFontMetrics fm(scaleFont);
        const int textHeight = fm.height();
        const int w = width();
        const int h = height();

        // Calculate effective center frequency (CW mode applies pitch offset)
        qint64 effectiveCenter = m_centerFreq;
        if (m_mode == "CW") {
            effectiveCenter = m_centerFreq + m_cwPitch;
        } else if (m_mode == "CW-R") {
            effectiveCenter = m_centerFreq - m_cwPitch;
        }

        qint64 startFreq = effectiveCenter - m_spanHz / 2;
        qint64 endFreq = effectiveCenter + m_spanHz / 2;

        // Get appropriate interval for this span (targets ~20-30 labels)
        int interval = calculateLabelInterval(m_spanHz);

        // Find first label frequency (round up to next interval boundary)
        qint64 firstLabel = (startFreq / interval) * interval;
        if (firstLabel < startFreq)
            firstLabel += interval;

        // Measure sample label width for spacing check
        QString sampleLabel = formatFrequency(firstLabel);
        int labelWidth = fm.horizontalAdvance(sampleLabel);
        int minSpacing = m_audioUnits ? 14 : labelWidth + 12; // Minimum gap between labels

        // Draw labels at each interval
        int lastDrawnX = -1000; // Track last drawn position for overlap prevention
        for (qint64 freq = firstLabel; freq <= endFreq; freq += interval) {
            // Convert frequency to X pixel position
            float normalized = static_cast<float>(freq - startFreq) / static_cast<float>(m_spanHz);
            int x = static_cast<int>(normalized * w);

            QString label = formatFrequency(freq);
            int textWidth = fm.horizontalAdvance(label);

            // Center text horizontally on the frequency position
            int textX = x - textWidth / 2;

            // Skip if too close to previous label or clipped at edges
            if (textX < 2 || textX + textWidth > w - 2)
                continue;
            if (textX < lastDrawnX + minSpacing)
                continue;

            // Draw text centered vertically in overlay
            int textY = (h + textHeight) / 2 - 2;
            painter.drawText(textX, textY, label);
            lastDrawnX = textX + textWidth;
        }
    }

private:
    // Calculate frequency intervals to get ~20-30 labels across the span
    int calculateLabelInterval(int spanHz) const {
        // Target approximately 25 labels
        int targetLabels = 25;
        int rawInterval = spanHz / targetLabels;

        // Round to "nice" intervals (multiples that look clean on display)
        // Use intervals that result in clean MHz decimal values
        static const int niceIntervals[] = {
            100,    // 0.0001 MHz - for very narrow spans
            200,    // 0.0002 MHz
            500,    // 0.0005 MHz
            1000,   // 0.001 MHz (1 kHz)
            2000,   // 0.002 MHz (2 kHz)
            5000,   // 0.005 MHz (5 kHz)
            6000,   // 0.006 MHz (6 kHz) - common on K4
            10000,  // 0.010 MHz (10 kHz)
            12000,  // 0.012 MHz (12 kHz)
            20000,  // 0.020 MHz (20 kHz)
            25000,  // 0.025 MHz (25 kHz)
            50000,  // 0.050 MHz (50 kHz)
            100000, // 0.100 MHz (100 kHz)
        };

        // Find the smallest nice interval that gives <= targetLabels
        for (int nice : niceIntervals) {
            if (spanHz / nice <= targetLabels + 5) {
                return nice;
            }
        }
        return 100000; // Default for very wide spans
    }

    // Format frequency as MHz string with adaptive decimal places
    // Narrow spans need more precision to avoid duplicate labels
    QString formatFrequency(qint64 freqHz) const {
        if (m_audioUnits) return QString::number(freqHz);
        double freqMHz = freqHz / 1000000.0;
        // Use 4 decimals for spans <= 20 kHz, 3 decimals for wider spans
        int decimals = (m_spanHz <= 20000) ? 4 : 3;
        return QString::number(freqMHz, 'f', decimals);
    }

    bool m_audioUnits = false;
    qint64 m_centerFreq = 0;
    int m_spanHz = 10000;
    int m_cwPitch = 500;
    QString m_mode = "USB";
};

PanadapterRhiWidget::PanadapterRhiWidget(QWidget *parent) : QRhiWidget(parent) {
    // The enclosing phone console reserves the available height for its
    // fixed two-row operating dock. A 200 px renderer minimum overrode that
    // contract and pushed the tuning row below the viewport after connection
    // populated the live VFO/antenna rows. The container retains a 90 px
    // compact minimum, while desktop keeps the larger spectrum surface.
    setMinimumHeight(K4Styles::isCompactLayout() ? 0 : 200);
    setMouseTracking(true);
    m_wheelAccumulator.setFilterMomentum(false);

    // Force Metal API on macOS
#ifdef Q_OS_MACOS
    setApi(QRhiWidget::Api::Metal);
#endif

    // Initialize color LUTs
    initColorLUT();    // Waterfall LUT
    initSpectrumLUT(); // Spectrum LUT (for BlueAmplitude style)

    // Note: Waterfall data buffer is allocated in initialize() after devicePixelRatio is known

    // Waterfall marker timer
    m_waterfallMarkerTimer = new QTimer(this);
    m_waterfallMarkerTimer->setSingleShot(true);
    connect(m_waterfallMarkerTimer, &QTimer::timeout, this, [this]() {
        m_showWaterfallMarker = false;
        update();
    });

    // Create dBm scale overlay (child widget)
    m_dbmScaleOverlay = new DbmScaleOverlay(this);
    m_dbmScaleOverlay->setDbRange(m_minDb, m_maxDb);
    m_dbmScaleOverlay->show();

    // Create frequency scale overlay (child widget at spectrum/waterfall boundary)
    m_freqScaleOverlay = new FrequencyScaleOverlay(this);
    m_freqScaleOverlay->setFrequencyRange(m_centerFreq, m_spanHz, m_cwPitch, m_mode);
    m_freqScaleOverlay->show();
}

PanadapterRhiWidget::~PanadapterRhiWidget() {
    // QRhi resources are automatically cleaned up
}

void PanadapterRhiWidget::resizeEvent(QResizeEvent *event) {
    QRhiWidget::resizeEvent(event);
    updateDbmScaleOverlay();
    updateFreqScaleOverlay();
}

void PanadapterRhiWidget::updateDbmScaleOverlay() {
    if (!m_dbmScaleOverlay)
        return;

    // Position overlay to cover spectrum area only (top portion)
    const int h = height();
    const int spectrumHeight = static_cast<int>(h * m_spectrumRatio);

    // Overlay covers left side of spectrum area
    m_dbmScaleOverlay->setGeometry(0, 0, 70, spectrumHeight);
    m_dbmScaleOverlay->setDbRange(m_minDb, m_maxDb);
    m_dbmScaleOverlay->raise(); // Ensure it's on top
}

void PanadapterRhiWidget::updateFreqScaleOverlay() {
    if (!m_freqScaleOverlay)
        return;

    const int h = height();
    const int w = width();
    const int spectrumHeight = static_cast<int>(h * m_spectrumRatio);

    // Position overlay centered on the boundary between spectrum and waterfall
    // Overlay height: 16px, centered on spectrumHeight boundary line
    const int overlayHeight = 16;
    const int overlayY = spectrumHeight - overlayHeight / 2;

    m_freqScaleOverlay->setGeometry(0, overlayY, w, overlayHeight);
    m_freqScaleOverlay->setFrequencyRange(m_centerFreq, m_spanHz, m_cwPitch, m_mode);
    m_freqScaleOverlay->raise(); // Ensure it renders on top
}

void PanadapterRhiWidget::initColorLUT() {
    // Create 256-entry RGBA color LUT for WATERFALL (unchanged)
    // 8-stage color progression: Black -> Dark Blue -> Royal Blue -> Cyan -> Green -> Yellow -> Red
    m_colorLUT.resize(256 * 4);

    for (int i = 0; i < 256; ++i) {
        const float inputValue = i / 255.0f;
        // A larger WTR CLRS value brightens a given incoming intensity.  The
        // range is applied to the LUT rather than PAN data, so both existing
        // waterfall history and new rows update immediately and identically.
        const float rangeMultiplier = static_cast<float>(m_waterfallColorRange) / 10.0f;
        float value = std::pow(inputValue, 1.0f / rangeMultiplier);
        int r, g, b;

        if (value < 0.10f) {
            r = 0;
            g = 0;
            b = 0;
        } else if (value < 0.25f) {
            float t = (value - 0.10f) / 0.15f;
            r = 0;
            g = 0;
            b = static_cast<int>(t * 51);
        } else if (value < 0.40f) {
            float t = (value - 0.25f) / 0.15f;
            r = 0;
            g = 0;
            b = static_cast<int>(51 + t * 102);
        } else if (value < 0.55f) {
            float t = (value - 0.40f) / 0.15f;
            r = 0;
            g = static_cast<int>(t * 128);
            b = static_cast<int>(153 + t * 102);
        } else if (value < 0.70f) {
            float t = (value - 0.55f) / 0.15f;
            r = 0;
            g = static_cast<int>(128 + t * 127);
            b = static_cast<int>(255 * (1.0f - t));
        } else if (value < 0.85f) {
            float t = (value - 0.70f) / 0.15f;
            r = static_cast<int>(t * 255);
            g = 255;
            b = 0;
        } else {
            float t = (value - 0.85f) / 0.15f;
            r = 255;
            g = static_cast<int>(255 * (1.0f - t));
            b = 0;
        }

        m_colorLUT[i * 4 + 0] = static_cast<quint8>(qBound(0, r, 255));
        m_colorLUT[i * 4 + 1] = static_cast<quint8>(qBound(0, g, 255));
        m_colorLUT[i * 4 + 2] = static_cast<quint8>(qBound(0, b, 255));
        m_colorLUT[i * 4 + 3] = 255;
    }

    // Apply the selected K4 waterfall palette while preserving signal intensity.
    if (m_waterfallColor != 1) {
        for (int i = 0; i < 256; ++i) {
            const int r = m_colorLUT[i * 4 + 0];
            const int g = m_colorLUT[i * 4 + 1];
            const int b = m_colorLUT[i * 4 + 2];
            const int intensity = qBound(0, (r * 30 + g * 59 + b * 11) / 100, 255);
            int outR = r;
            int outG = g;
            int outB = b;
            switch (m_waterfallColor) {
            case 0: // gray
                outR = outG = outB = intensity;
                break;
            case 2: // teal
                outR = intensity / 10;
                outG = qBound(0, intensity * 3 / 4, 255);
                outB = intensity;
                break;
            case 3: // blue
                outR = 0;
                outG = intensity / 3;
                outB = intensity;
                break;
            case 4: // sepia
                outR = intensity;
                outG = qBound(0, intensity * 4 / 5, 255);
                outB = qBound(0, intensity * 2 / 5, 255);
                break;
            default:
                break;
            }
            m_colorLUT[i * 4 + 0] = static_cast<quint8>(outR);
            m_colorLUT[i * 4 + 1] = static_cast<quint8>(outG);
            m_colorLUT[i * 4 + 2] = static_cast<quint8>(outB);
        }
    }
}

void PanadapterRhiWidget::initSpectrumLUT() {
    // Create 256-entry RGBA color LUT for SPECTRUM (BlueAmplitude style)
    // 8-stage: Royal Blue -> Cyan -> Green -> Yellow -> Orange -> Red -> White
    // Noise floor starts at royal blue (more visible color earlier)
    m_spectrumLUT.resize(256 * 4);

    for (int i = 0; i < 256; ++i) {
        float value = i / 255.0f;
        int r, g, b;

        if (value < 0.08f) {
            // Royal Blue (visible noise floor) - start brighter
            float t = value / 0.08f;
            r = 0;
            g = 0;
            b = static_cast<int>(80 + t * 100); // 80-180: royal blue range
        } else if (value < 0.20f) {
            // Royal Blue -> Cyan
            float t = (value - 0.08f) / 0.12f;
            r = 0;
            g = static_cast<int>(t * 200);
            b = static_cast<int>(180 + t * 75); // 180-255
        } else if (value < 0.35f) {
            // Cyan -> Green
            float t = (value - 0.20f) / 0.15f;
            r = 0;
            g = static_cast<int>(200 + t * 55); // 200-255
            b = static_cast<int>(255 * (1.0f - t));
        } else if (value < 0.52f) {
            // Green -> Yellow
            float t = (value - 0.35f) / 0.17f;
            r = static_cast<int>(t * 255);
            g = 255;
            b = 0;
        } else if (value < 0.70f) {
            // Yellow -> Orange -> Red
            float t = (value - 0.52f) / 0.18f;
            r = 255;
            g = static_cast<int>(255 * (1.0f - t));
            b = 0;
        } else {
            // Red -> White (strongest signals)
            float t = (value - 0.70f) / 0.30f;
            r = 255;
            g = static_cast<int>(t * 255);
            b = static_cast<int>(t * 255);
        }

        m_spectrumLUT[i * 4 + 0] = static_cast<quint8>(qBound(0, r, 255));
        m_spectrumLUT[i * 4 + 1] = static_cast<quint8>(qBound(0, g, 255));
        m_spectrumLUT[i * 4 + 2] = static_cast<quint8>(qBound(0, b, 255));
        m_spectrumLUT[i * 4 + 3] = 255;
    }
}

void PanadapterRhiWidget::releaseResources() {
    // QRhiWidget can recreate its graphics context when the window/surface
    // changes. No resource may be reused with a different QRhi instance.
    m_waterfallPipeline.reset(); m_overlayLinePipeline.reset(); m_peakLinePipeline.reset();
    m_overlayTrianglePipeline.reset(); m_spectrumBlueAmpPipeline.reset();
    m_waterfallSrb.reset(); m_overlaySrb.reset(); m_passbandSrb.reset();
    m_markerSrb.reset(); m_notchSrb.reset(); m_spectrumBlueAmpSrb.reset();
    m_secondaryPassbandSrb.reset(); m_secondaryMarkerSrb.reset();
    m_waterfallVbo.reset(); m_waterfallUniformBuffer.reset();
    m_overlayVbo.reset(); m_overlayUniformBuffer.reset();
    m_passbandVbo.reset(); m_passbandUniformBuffer.reset();
    m_markerVbo.reset(); m_markerUniformBuffer.reset();
    m_notchVbo.reset(); m_notchUniformBuffer.reset();
    m_fullscreenQuadVbo.reset(); m_spectrumBlueAmpUniformBuffer.reset();
    m_secondaryPassbandVbo.reset(); m_secondaryPassbandUniformBuffer.reset();
    m_secondaryMarkerVbo.reset(); m_secondaryMarkerUniformBuffer.reset();
    m_waterfallTexture.reset(); m_colorLutTexture.reset(); m_spectrumDataTexture.reset();
    m_spectrumColorLutTexture.reset(); m_sampler.reset();
    m_rhiInitialized = m_pipelinesCreated = m_firstFrameRendered = false;
    m_rhi = nullptr; m_rpDesc = nullptr;
}

void PanadapterRhiWidget::initialize(QRhiCommandBuffer *cb) {
    if (m_rhiInitialized && m_rhi == rhi())
        return;
    if (m_rhiInitialized) releaseResources();

    m_rhi = rhi();
    if (!m_rhi) {
        qWarning() << "QRhi is NULL - GPU backend failed to initialize";
        return;
    }

    // Use fixed texture sizes - GPU bilinear filtering handles scaling to display size
    m_textureWidth = BASE_TEXTURE_WIDTH;
    m_waterfallHistory = m_audioView ? 256 : BASE_WATERFALL_HISTORY;

    // Allocate waterfall data buffer
    if (!m_audioView || m_waterfallData.size() != m_textureWidth * m_waterfallHistory) {
        m_waterfallData.resize(m_textureWidth * m_waterfallHistory);
        m_waterfallData.fill(0);
    }

    // Load shaders from compiled .qsb resources
    m_spectrumBlueVert = RhiUtils::loadShader(":/shaders/src/dsp/shaders/spectrum_blue.vert.qsb");
    m_spectrumBlueAmpFrag = RhiUtils::loadShader(":/shaders/src/dsp/shaders/spectrum_blue_amp.frag.qsb");
    m_waterfallVert = RhiUtils::loadShader(":/shaders/src/dsp/shaders/waterfall.vert.qsb");
    m_waterfallFrag = RhiUtils::loadShader(":/shaders/src/dsp/shaders/waterfall.frag.qsb");
    m_overlayVert = RhiUtils::loadShader(":/shaders/src/dsp/shaders/overlay.vert.qsb");
    m_overlayFrag = RhiUtils::loadShader(":/shaders/src/dsp/shaders/overlay.frag.qsb");

    // Create waterfall texture (single channel for dB values)
    m_waterfallTexture.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_textureWidth, m_waterfallHistory), 1,
                                               QRhiTexture::UsedAsTransferSource));
    m_waterfallTexture->create();

    // Create color LUT texture (256x1 RGBA)
    m_colorLutTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(256, 1)));
    m_colorLutTexture->create();

    // Android GPU drivers do not consistently support linearly filtered R32F
    // textures. RGBA8 is universally supported by QRhi and preserves the
    // normalized 0..1 value sampled from the red component by the shader.
#ifdef Q_OS_ANDROID
    m_spectrumDataTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(m_textureWidth, 1)));
#else
    m_spectrumDataTexture.reset(m_rhi->newTexture(QRhiTexture::R32F, QSize(m_textureWidth, 1)));
#endif
    m_spectrumDataTexture->create();

    // Create spectrum color LUT texture (256x1 RGBA) - for BlueAmplitude style
    m_spectrumColorLutTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(256, 1)));
    m_spectrumColorLutTexture->create();

    // Upload color LUT data (separate LUTs for waterfall and spectrum)
    QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();
    // Upload waterfall color LUT
    QRhiTextureSubresourceUploadDescription waterfallLutUpload(m_colorLUT.constData(), m_colorLUT.size());
    rub->uploadTexture(m_colorLutTexture.get(), QRhiTextureUploadEntry(0, 0, waterfallLutUpload));
    // Upload spectrum color LUT (for BlueAmplitude style)
    QRhiTextureSubresourceUploadDescription spectrumLutUpload(m_spectrumLUT.constData(), m_spectrumLUT.size());
    rub->uploadTexture(m_spectrumColorLutTexture.get(), QRhiTextureUploadEntry(0, 0, spectrumLutUpload));

    // Upload initial zeroed waterfall data (prevents uninitialized texture garbage)
    QRhiTextureSubresourceUploadDescription waterfallUpload(m_waterfallData.constData(), m_waterfallData.size());
    rub->uploadTexture(m_waterfallTexture.get(), QRhiTextureUploadEntry(0, 0, waterfallUpload));

    // Create sampler
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::Repeat));
    m_sampler->create();

    // Waterfall quad (static)
    float tMax = static_cast<float>(m_waterfallHistory - 1) / m_waterfallHistory;
    float waterfallQuad[] = {
        // position (x, y), texcoord (s, t)
        -1.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f,  -1.0f, 1.0f, 0.0f, // bottom-right
        1.0f,  1.0f,  1.0f, tMax, // top-right
        -1.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f,  1.0f,  1.0f, tMax, // top-right
        -1.0f, 1.0f,  0.0f, tMax  // top-left
    };
    m_waterfallVbo.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(waterfallQuad)));
    m_waterfallVbo->create();
    rub->uploadStaticBuffer(m_waterfallVbo.get(), waterfallQuad);

    m_overlayVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 4096 * 2 * sizeof(float)));
    m_overlayVbo->create();

    // Create uniform buffers
    m_waterfallUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(RhiUtils::WaterfallUniforms)));
    m_waterfallUniformBuffer->create();

    m_overlayUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_overlayUniformBuffer->create();

    // Peak hold needs its own buffers so grid and peak updates can both be
    // recorded into the single pre-pass batch without colliding on one buffer.
    m_peakVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 4096 * 2 * sizeof(float)));
    m_peakVbo->create();
    m_peakUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_peakUniformBuffer->create();

    // Fullscreen quad (shared by all fragment-shader spectrum styles)
    // Position (x, y) + texCoord (s, t) - covers normalized -1 to 1 range
    float fullscreenQuad[] = {
        // position (x, y), texcoord (s, t)
        -1.0f, -1.0f, 0.0f, 1.0f, // bottom-left (texCoord y=1 = bottom)
        1.0f,  -1.0f, 1.0f, 1.0f, // bottom-right
        1.0f,  1.0f,  1.0f, 0.0f, // top-right (texCoord y=0 = top)
        -1.0f, -1.0f, 0.0f, 1.0f, // bottom-left
        1.0f,  1.0f,  1.0f, 0.0f, // top-right
        -1.0f, 1.0f,  0.0f, 0.0f  // top-left
    };
    m_fullscreenQuadVbo.reset(
        m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(fullscreenQuad)));
    m_fullscreenQuadVbo->create();
    rub->uploadStaticBuffer(m_fullscreenQuadVbo.get(), fullscreenQuad);

    // Spectrum amplitude style uniform buffer: 80 bytes (std140 layout)
    // fillBaseColor(16) + fillPeakColor(16) + glowColor(16) + params(16) + viewport(16)
    m_spectrumBlueAmpUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 80));
    m_spectrumBlueAmpUniformBuffer->create();

    // Separate buffers for passband to avoid GPU buffer conflicts
    m_passbandVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 256 * sizeof(float)));
    m_passbandVbo->create();

    m_passbandUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_passbandUniformBuffer->create();

    // Separate buffers for marker to avoid conflicts with passband
    m_markerVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 64 * sizeof(float)));
    m_markerVbo->create();

    m_markerUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_markerUniformBuffer->create();

    // Separate buffers for notch to avoid conflicts with grid (which shares overlay buffers)
    m_notchVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 1200 * sizeof(float)));
    m_notchVbo->create();

    m_notchUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_notchUniformBuffer->create();

    // Secondary VFO passband buffers (for showing other receiver's passband)
    m_secondaryPassbandVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 256 * sizeof(float)));
    m_secondaryPassbandVbo->create();

    m_secondaryPassbandUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_secondaryPassbandUniformBuffer->create();

    m_secondaryMarkerVbo.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 64 * sizeof(float)));
    m_secondaryMarkerVbo->create();

    m_secondaryMarkerUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32));
    m_secondaryMarkerUniformBuffer->create();

    cb->resourceUpdate(rub);

    m_rhiInitialized = true;
}

void PanadapterRhiWidget::createPipelines() {
    if (m_pipelinesCreated)
        return;

    if (!m_spectrumBlueVert.isValid() || !m_spectrumBlueAmpFrag.isValid())
        return;

    m_rpDesc = renderTarget()->renderPassDescriptor();

    // Spectrum amplitude pipeline (LUT-based colors with amplitude brightness)
    {
        m_spectrumBlueAmpSrb.reset(m_rhi->newShaderResourceBindings());
        m_spectrumBlueAmpSrb->setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage,
                                                      m_spectrumBlueAmpUniformBuffer.get()),
             QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                       m_spectrumDataTexture.get(), m_sampler.get()),
             QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                       m_spectrumColorLutTexture.get(), m_sampler.get())});
        m_spectrumBlueAmpSrb->create();

        m_spectrumBlueAmpPipeline.reset(m_rhi->newGraphicsPipeline());
        m_spectrumBlueAmpPipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, m_spectrumBlueVert}, {QRhiShaderStage::Fragment, m_spectrumBlueAmpFrag}});

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{4 * sizeof(float)}});                         // position(2) + texcoord(2)
        inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}, // position
                                   {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}}); // texcoord
        m_spectrumBlueAmpPipeline->setVertexInputLayout(inputLayout);
        m_spectrumBlueAmpPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_spectrumBlueAmpPipeline->setShaderResourceBindings(m_spectrumBlueAmpSrb.get());
        m_spectrumBlueAmpPipeline->setRenderPassDescriptor(m_rpDesc);

        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        m_spectrumBlueAmpPipeline->setTargetBlends({blend});

        m_spectrumBlueAmpPipeline->create();
    }

    // Waterfall pipeline
    {
        m_waterfallSrb.reset(m_rhi->newShaderResourceBindings());
        m_waterfallSrb->setBindings(
            {QRhiShaderResourceBinding::uniformBuffer(
                 0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                 m_waterfallUniformBuffer.get()),
             QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                       m_waterfallTexture.get(), m_sampler.get()),
             QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                       m_colorLutTexture.get(), m_sampler.get())});
        m_waterfallSrb->create();

        m_waterfallPipeline.reset(m_rhi->newGraphicsPipeline());
        m_waterfallPipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, m_waterfallVert}, {QRhiShaderStage::Fragment, m_waterfallFrag}});

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{4 * sizeof(float)}});                         // position(2) + texcoord(2)
        inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}, // position
                                   {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}}); // texcoord
        m_waterfallPipeline->setVertexInputLayout(inputLayout);
        m_waterfallPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_waterfallPipeline->setShaderResourceBindings(m_waterfallSrb.get());
        m_waterfallPipeline->setRenderPassDescriptor(m_rpDesc);
        m_waterfallPipeline->create();
    }

    // Overlay pipeline (lines)
    {
        m_overlaySrb.reset(m_rhi->newShaderResourceBindings());
        m_overlaySrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_overlayUniformBuffer.get())});
        m_overlaySrb->create();

        m_peakSrb.reset(m_rhi->newShaderResourceBindings());
        m_peakSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_peakUniformBuffer.get())});
        m_peakSrb->create();

        // Separate SRB for passband to avoid buffer conflicts
        m_passbandSrb.reset(m_rhi->newShaderResourceBindings());
        m_passbandSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_passbandUniformBuffer.get())});
        m_passbandSrb->create();

        // Separate SRB for marker to avoid buffer conflicts with passband
        m_markerSrb.reset(m_rhi->newShaderResourceBindings());
        m_markerSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_markerUniformBuffer.get())});
        m_markerSrb->create();

        m_notchSrb.reset(m_rhi->newShaderResourceBindings());
        m_notchSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_notchUniformBuffer.get())});
        m_notchSrb->create();

        // Secondary VFO passband SRBs
        m_secondaryPassbandSrb.reset(m_rhi->newShaderResourceBindings());
        m_secondaryPassbandSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_secondaryPassbandUniformBuffer.get())});
        m_secondaryPassbandSrb->create();

        m_secondaryMarkerSrb.reset(m_rhi->newShaderResourceBindings());
        m_secondaryMarkerSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_secondaryMarkerUniformBuffer.get())});
        m_secondaryMarkerSrb->create();

        m_overlayLinePipeline.reset(m_rhi->newGraphicsPipeline());
        m_overlayLinePipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, m_overlayVert}, {QRhiShaderStage::Fragment, m_overlayFrag}});

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{2 * sizeof(float)}});
        inputLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});
        m_overlayLinePipeline->setVertexInputLayout(inputLayout);
        m_overlayLinePipeline->setTopology(QRhiGraphicsPipeline::Lines);
        m_overlayLinePipeline->setShaderResourceBindings(m_overlaySrb.get());
        m_overlayLinePipeline->setRenderPassDescriptor(m_rpDesc);

        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        m_overlayLinePipeline->setTargetBlends({blend});

        m_overlayLinePipeline->create();

        // Peak Hold must be one connected spectrum trace.  Using the generic
        // Lines topology here only drew alternating, isolated bin segments.
        m_peakLinePipeline.reset(m_rhi->newGraphicsPipeline());
        m_peakLinePipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, m_overlayVert}, {QRhiShaderStage::Fragment, m_overlayFrag}});
        m_peakLinePipeline->setVertexInputLayout(inputLayout);
        m_peakLinePipeline->setTopology(QRhiGraphicsPipeline::LineStrip);
        m_peakLinePipeline->setShaderResourceBindings(m_peakSrb.get());
        m_peakLinePipeline->setRenderPassDescriptor(m_rpDesc);
        m_peakLinePipeline->setTargetBlends({blend});
        m_peakLinePipeline->setLineWidth(2.0f);
        m_peakLinePipeline->create();

        // Triangle version for filled shapes
        m_overlayTrianglePipeline.reset(m_rhi->newGraphicsPipeline());
        m_overlayTrianglePipeline->setShaderStages(
            {{QRhiShaderStage::Vertex, m_overlayVert}, {QRhiShaderStage::Fragment, m_overlayFrag}});
        m_overlayTrianglePipeline->setVertexInputLayout(inputLayout);
        m_overlayTrianglePipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_overlayTrianglePipeline->setShaderResourceBindings(m_overlaySrb.get());
        m_overlayTrianglePipeline->setRenderPassDescriptor(m_rpDesc);
        m_overlayTrianglePipeline->setTargetBlends({blend});
        m_overlayTrianglePipeline->create();
    }

    m_pipelinesCreated = true;
}

void PanadapterRhiWidget::render(QRhiCommandBuffer *cb) {
    // Always clear to black even if not initialized (prevents red/garbage showing)
    if (!m_rhiInitialized) {
        cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, nullptr);
        cb->endPass();
        return;
    }

    // Create pipelines on first render (need render pass descriptor)
    if (!m_pipelinesCreated) {
        createPipelines();
        if (!m_pipelinesCreated) {
            cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, nullptr);
            cb->endPass();
            return;
        }
    }

    const QSize outputSize = renderTarget()->pixelSize();
    const float w = outputSize.width();
    const float h = outputSize.height();
    const float spectrumHeight = h * m_spectrumRatio;
    const float waterfallHeight = h - spectrumHeight;

    QRhiResourceUpdateBatch *rub = m_rhi->nextResourceUpdateBatch();

    if (m_waterfallColorNeedsUpdate) {
        QRhiTextureSubresourceUploadDescription lutUpload(m_colorLUT.constData(), m_colorLUT.size());
        rub->uploadTexture(m_colorLutTexture.get(), QRhiTextureUploadEntry(0, 0, lutUpload));
        m_waterfallColorNeedsUpdate = false;
    }

    // Full waterfall clear (on disconnect)
    if (m_waterfallNeedsFullClear) {
        QRhiTextureSubresourceUploadDescription fullUpload(m_waterfallData.constData(), m_waterfallData.size());
        rub->uploadTexture(m_waterfallTexture.get(), QRhiTextureUploadEntry(0, 0, fullUpload));
        m_waterfallNeedsFullClear = false;
    }

    // Update waterfall texture if needed
    if (m_waterfallNeedsUpdate && !m_currentSpectrum.isEmpty()) {
        updateWaterfallData();
        QRhiTextureSubresourceUploadDescription rowUpload(
            m_waterfallData.constData() + m_waterfallWriteRow * m_textureWidth, m_textureWidth);
        rowUpload.setDestinationTopLeft(QPoint(0, m_waterfallWriteRow));
        rowUpload.setSourceSize(QSize(m_textureWidth, 1));
        rub->uploadTexture(m_waterfallTexture.get(), QRhiTextureUploadEntry(0, 0, rowUpload));
        m_waterfallWriteRow = (m_waterfallWriteRow + 1) % m_waterfallHistory;
        m_waterfallNeedsUpdate = false;
    }

    // Update waterfall uniform buffer with bin parameters
    float scrollOffset = static_cast<float>(m_waterfallWriteRow) / m_waterfallHistory;
    float binCount = static_cast<float>(m_currentSpectrum.isEmpty() ? m_textureWidth : m_currentSpectrum.size());
    if (m_audioView) binCount = float(qMax(1, int(m_audioSpectrum.size())));
    RhiUtils::WaterfallUniforms waterfallUniforms = {scrollOffset, binCount, static_cast<float>(m_textureWidth), 0.0f,
        m_audioView ? float((m_audioLowHz - m_audioFirstHz) / (binCount * m_audioBinHz)) : 0.0f,
        m_audioView ? float(m_audioSpanHz / (binCount * m_audioBinHz)) : 1.0f, 0.0f, 0.0f};
    rub->updateDynamicBuffer(m_waterfallUniformBuffer.get(), 0, sizeof(waterfallUniforms), &waterfallUniforms);

    // Calculate smoothed baseline for spectrum normalization
    if (!m_currentSpectrum.isEmpty()) {
        float frameMinNormalized = 1.0f;
        for (int i = 0; i < m_currentSpectrum.size(); ++i) {
            float normalized = normalizeDb(m_currentSpectrum[i]);
            if (normalized < frameMinNormalized)
                frameMinNormalized = normalized;
        }
        const float baselineAlpha = 0.05f;
        if (m_smoothedBaseline < 0.001f)
            m_smoothedBaseline = frameMinNormalized;
        else
            m_smoothedBaseline = baselineAlpha * frameMinNormalized + (1.0f - baselineAlpha) * m_smoothedBaseline;

        // Upload raw spectrum bins to 1D texture - GPU shader does bilinear interpolation
        int specSize = m_currentSpectrum.size();
        int offset = (m_textureWidth - specSize) / 2;

        // The Android path uses a universally supported normalized RGBA8
        // texture. Desktop keeps QK4's original R32F upload unchanged.
#ifdef Q_OS_ANDROID
        QByteArray normalizedSpectrum(m_textureWidth * 4, '\0');
#else
        QVector<float> normalizedSpectrum(m_textureWidth, 0.0f);
#endif
        for (int i = 0; i < specSize; ++i) {
            float normalized = normalizeDb(m_currentSpectrum[i]);
            float adjusted = qMax(0.0f, normalized - m_smoothedBaseline);
#ifdef Q_OS_ANDROID
            const int pixelOffset = (offset + i) * 4;
            normalizedSpectrum[pixelOffset] = static_cast<char>(qRound(qBound(0.0f, adjusted * 0.95f, 1.0f) * 255.0f));
            normalizedSpectrum[pixelOffset + 3] = static_cast<char>(255);
#else
            normalizedSpectrum[offset + i] = adjusted * 0.95f;
#endif
        }

#ifdef Q_OS_ANDROID
        QRhiTextureSubresourceUploadDescription specDataUpload(normalizedSpectrum.constData(), normalizedSpectrum.size());
#else
        QRhiTextureSubresourceUploadDescription specDataUpload(normalizedSpectrum.constData(),
                                                               normalizedSpectrum.size() * sizeof(float));
#endif
        specDataUpload.setSourceSize(QSize(m_textureWidth, 1));
        rub->uploadTexture(m_spectrumDataTexture.get(), QRhiTextureUploadEntry(0, 0, specDataUpload));

        // Update blue spectrum uniform buffer (80 bytes, std140 layout)
        float specBinCount =
            static_cast<float>(m_currentSpectrum.isEmpty() ? m_textureWidth : m_currentSpectrum.size());
        struct {
            float fillBaseColor[4]; // offset 0: dark navy
            float fillPeakColor[4]; // offset 16: electric blue
            float glowColor[4];     // offset 32: cyan glow
            float glowIntensity;    // offset 48
            float glowWidth;        // offset 52
            float spectrumHeightPx; // offset 56
            float binCount;         // offset 60: actual bin count
            float viewportSize[2];  // offset 64
            float textureWidth;     // offset 72: for bin centering
            float padding;          // offset 76
        } specBlueUniforms = {
            {0.0f, 0.08f, 0.16f, 0.85f},        // fillBaseColor: dark navy
            {0.0f, 0.63f, 1.0f, 0.85f},         // fillPeakColor: electric blue
            {0.0f, 0.83f, 1.0f, 1.0f},          // glowColor: cyan
            0.8f,                               // glowIntensity
            0.04f,                              // glowWidth
            spectrumHeight,                     // spectrumHeight in pixels
            specBinCount,                       // binCount for shader
            {w, h},                             // viewportSize
            static_cast<float>(m_textureWidth), // textureWidth for bin centering
            0.0f                                // padding
        };
        rub->updateDynamicBuffer(m_spectrumBlueAmpUniformBuffer.get(), 0, sizeof(specBlueUniforms), &specBlueUniforms);
    }

    cb->resourceUpdate(rub);

    // Qt RHI forbids cb->resourceUpdate() inside a render pass. Record every
    // remaining dynamic-buffer update into one batch handed to beginPass, and
    // defer the draw calls into a list replayed inside the pass.
    QRhiResourceUpdateBatch *overlayRub = m_rhi->nextResourceUpdateBatch();
    std::vector<std::function<void()>> draws;

    // Draw waterfall (bottom portion)
    if (m_waterfallPipeline) {
        draws.push_back([=]() {
            cb->setViewport({0, 0, w, waterfallHeight});
            cb->setGraphicsPipeline(m_waterfallPipeline.get());
            cb->setShaderResources(m_waterfallSrb.get());
            const QRhiCommandBuffer::VertexInput waterfallVbufBinding(m_waterfallVbo.get(), 0);
            cb->setVertexInput(0, 1, &waterfallVbufBinding);
            cb->draw(6);
        });
    }

    // Draw grid BEHIND spectrum (in spectrum area)
    if (m_gridEnabled && m_overlayLinePipeline) {
        QVector<float> gridVerts;

        // Horizontal lines (dB scale) - 8 divisions in spectrum area
        for (int i = 1; i < 8; ++i) {
            float y = spectrumHeight * i / 8.0f;
            gridVerts << 0.0f << y << w << y;
        }

        // Vertical lines (frequency) - 10 divisions in spectrum area
        for (int i = 1; i < 10; ++i) {
            float x = w * i / 10.0f;
            gridVerts << x << 0.0f << x << spectrumHeight;
        }

        overlayRub->updateDynamicBuffer(m_overlayVbo.get(), 0, gridVerts.size() * sizeof(float), gridVerts.constData());

        struct {
            float viewportWidth;
            float viewportHeight;
            float pad0, pad1;
            float r, g, b, a;
        } gridUniforms = {w,
                          spectrumHeight,
                          0,
                          0,
                          static_cast<float>(m_gridColor.redF()),
                          static_cast<float>(m_gridColor.greenF()),
                          static_cast<float>(m_gridColor.blueF()),
                          static_cast<float>(m_gridColor.alphaF())};
        overlayRub->updateDynamicBuffer(m_overlayUniformBuffer.get(), 0, sizeof(gridUniforms), &gridUniforms);

        const int gridCount = gridVerts.size() / 2;
        draws.push_back([=]() {
            cb->setViewport({0, waterfallHeight, w, spectrumHeight});
            cb->setGraphicsPipeline(m_overlayLinePipeline.get());
            cb->setShaderResources(m_overlaySrb.get());
            const QRhiCommandBuffer::VertexInput gridVbufBinding(m_overlayVbo.get(), 0);
            cb->setVertexInput(0, 1, &gridVbufBinding);
            cb->draw(gridCount);
        });
    }

    // Draw spectrum fill ON TOP of grid (shader-based fullscreen quad)
    if (!m_currentSpectrum.isEmpty() && m_spectrumBlueAmpPipeline) {
        draws.push_back([=]() {
            cb->setViewport({0, waterfallHeight, w, spectrumHeight});
            cb->setGraphicsPipeline(m_spectrumBlueAmpPipeline.get());
            cb->setShaderResources(m_spectrumBlueAmpSrb.get());
            const QRhiCommandBuffer::VertexInput quadVbufBinding(m_fullscreenQuadVbo.get(), 0);
            cb->setVertexInput(0, 1, &quadVbufBinding);
            cb->draw(6); // Fullscreen quad (2 triangles)
        });
    }

    // Peak hold is a separate trace, not part of the spectrum-fill shader.
    // The previous code correctly accumulated m_peakHold but never submitted
    // those samples to the renderer, so #PKM only affected the radio.
    if (m_peakHoldEnabled && m_peakHold.size() > 1 && m_peakLinePipeline) {
        QVector<float> peakVerts;
        peakVerts.reserve(m_peakHold.size() * 2);
        const float denom = static_cast<float>(m_peakHold.size() - 1);
        for (int i = 0; i < m_peakHold.size(); ++i) {
            const float normalized = normalizeDb(m_peakHold.at(i));
            const float adjusted = qMax(0.0f, normalized - m_smoothedBaseline) * 0.95f;
            peakVerts << (w * static_cast<float>(i) / denom) << (spectrumHeight * (1.0f - adjusted));
        }
        overlayRub->updateDynamicBuffer(m_peakVbo.get(), 0, peakVerts.size() * sizeof(float), peakVerts.constData());
        struct {
            float viewportWidth;
            float viewportHeight;
            float pad0, pad1;
            float r, g, b, a;
        } peakUniforms = {w, spectrumHeight, 0, 0, static_cast<float>(m_peakHoldColor.redF()),
                          static_cast<float>(m_peakHoldColor.greenF()), static_cast<float>(m_peakHoldColor.blueF()),
                          static_cast<float>(m_peakHoldColor.alphaF())};
        overlayRub->updateDynamicBuffer(m_peakUniformBuffer.get(), 0, sizeof(peakUniforms), &peakUniforms);
        const int peakCount = peakVerts.size() / 2;
        draws.push_back([=]() {
            cb->setViewport({0, waterfallHeight, w, spectrumHeight});
            cb->setGraphicsPipeline(m_peakLinePipeline.get());
            cb->setShaderResources(m_peakSrb.get());
            const QRhiCommandBuffer::VertexInput peakVbufBinding(m_peakVbo.get(), 0);
            cb->setVertexInput(0, 1, &peakVbufBinding);
            cb->draw(peakCount);
        });
    }

    // Draw overlays (markers, passband) at full viewport. Each records its
    // buffer update into overlayRub and defers its draw into `draws`.
    if (m_overlayLinePipeline && m_overlayTrianglePipeline) {
        // Draw secondary VFO passband first (so it renders behind primary when overlapping)
        if (m_secondaryVisible && m_secondaryFilterBw > 0 && m_secondaryTunedFreq > 0) {
            qint64 secLowFreq, secHighFreq;
            int secShiftOffsetHz = m_secondaryIfShift * 10;

            if (m_secondaryMode == "LSB" || m_secondaryMode == "DATA-R") {
                qint64 center = m_secondaryTunedFreq - secShiftOffsetHz;
                secLowFreq = center - m_secondaryFilterBw / 2;
                secHighFreq = center + m_secondaryFilterBw / 2;
            } else if (m_secondaryMode == "USB" || m_secondaryMode == "DATA") {
                qint64 center = m_secondaryTunedFreq + secShiftOffsetHz;
                secLowFreq = center - m_secondaryFilterBw / 2;
                secHighFreq = center + m_secondaryFilterBw / 2;
            } else if (m_secondaryMode == "CW" || m_secondaryMode == "CW-R") {
                int pitchOffset = (m_secondaryMode == "CW") ? m_secondaryCwPitch : -m_secondaryCwPitch;
                qint64 center = m_secondaryTunedFreq + pitchOffset;
                secLowFreq = center - m_secondaryFilterBw / 2;
                secHighFreq = center + m_secondaryFilterBw / 2;
            } else {
                // AM/FM - symmetric around carrier (both sidebands, no IF shift)
                secLowFreq = m_secondaryTunedFreq - m_secondaryFilterBw / 2;
                secHighFreq = m_secondaryTunedFreq + m_secondaryFilterBw / 2;
            }

            float secX1 = freqToNormalized(secLowFreq) * w;
            float secX2 = freqToNormalized(secHighFreq) * w;
            secX1 = qBound(0.0f, secX1, w);
            secX2 = qBound(0.0f, secX2, w);

            if (secX2 > secX1) {
                QVector<float> secQuadVerts = {
                    secX1, 0, secX2, 0, secX2, spectrumHeight, secX1, 0, secX2, spectrumHeight, secX1, spectrumHeight};

                overlayRub->updateDynamicBuffer(m_secondaryPassbandVbo.get(), 0, secQuadVerts.size() * sizeof(float),
                                              secQuadVerts.constData());

                struct {
                    float viewportWidth;
                    float viewportHeight;
                    float pad0, pad1;
                    float r, g, b, a;
                } secPbUniforms = {w,
                                   h,
                                   0,
                                   0,
                                   static_cast<float>(m_secondaryPassbandColor.redF()),
                                   static_cast<float>(m_secondaryPassbandColor.greenF()),
                                   static_cast<float>(m_secondaryPassbandColor.blueF()),
                                   static_cast<float>(m_secondaryPassbandColor.alphaF())};
                overlayRub->updateDynamicBuffer(m_secondaryPassbandUniformBuffer.get(), 0, sizeof(secPbUniforms),
                                              &secPbUniforms);

                draws.push_back([=]() {
                    cb->setViewport({0, 0, w, h});
                    cb->setGraphicsPipeline(m_overlayTrianglePipeline.get());
                    cb->setShaderResources(m_secondaryPassbandSrb.get());
                    const QRhiCommandBuffer::VertexInput secPbVbufBinding(m_secondaryPassbandVbo.get(), 0);
                    cb->setVertexInput(0, 1, &secPbVbufBinding);
                    cb->draw(6);
                });
            }

            // Secondary VFO marker
            qint64 secMarkerFreq = m_secondaryTunedFreq;
            if (m_secondaryMode == "CW") {
                secMarkerFreq = m_secondaryTunedFreq + m_secondaryCwPitch;
            } else if (m_secondaryMode == "CW-R") {
                secMarkerFreq = m_secondaryTunedFreq - m_secondaryCwPitch;
            }
            float secMarkerX = freqToNormalized(secMarkerFreq) * w;
            if (secMarkerX >= 0 && secMarkerX <= w) {
                float markerWidth = 2.0f;
                QVector<float> secMarkerVerts = {secMarkerX,
                                                 0.0f,
                                                 secMarkerX + markerWidth,
                                                 0.0f,
                                                 secMarkerX + markerWidth,
                                                 spectrumHeight,
                                                 secMarkerX,
                                                 0.0f,
                                                 secMarkerX + markerWidth,
                                                 spectrumHeight,
                                                 secMarkerX,
                                                 spectrumHeight};

                overlayRub->updateDynamicBuffer(m_secondaryMarkerVbo.get(), 0, secMarkerVerts.size() * sizeof(float),
                                              secMarkerVerts.constData());

                struct {
                    float viewportWidth;
                    float viewportHeight;
                    float pad0, pad1;
                    float r, g, b, a;
                } secMkUniforms = {w,
                                   h,
                                   0,
                                   0,
                                   static_cast<float>(m_secondaryMarkerColor.redF()),
                                   static_cast<float>(m_secondaryMarkerColor.greenF()),
                                   static_cast<float>(m_secondaryMarkerColor.blueF()),
                                   static_cast<float>(m_secondaryMarkerColor.alphaF())};
                overlayRub->updateDynamicBuffer(m_secondaryMarkerUniformBuffer.get(), 0, sizeof(secMkUniforms),
                                              &secMkUniforms);

                draws.push_back([=]() {
                    cb->setViewport({0, 0, w, h});
                    cb->setGraphicsPipeline(m_overlayTrianglePipeline.get());
                    cb->setShaderResources(m_secondaryMarkerSrb.get());
                    const QRhiCommandBuffer::VertexInput secMkVbufBinding(m_secondaryMarkerVbo.get(), 0);
                    cb->setVertexInput(0, 1, &secMkVbufBinding);
                    cb->draw(6);
                });
            }
        }

        // Draw passband overlay (uses separate buffers to avoid GPU conflicts)
        if (m_cursorVisible && m_filterBw > 0 && m_tunedFreq > 0) {
            // Calculate passband edges based on mode
            qint64 lowFreq, highFreq;

            // K4 IF shift is reported in decahertz (10 Hz units)
            // This is the passband center offset from the dial frequency
            // USB with shift=150 means passband centered 1500 Hz above dial
            // CW with shift=50 means passband centered at 500 Hz pitch
            int shiftOffsetHz = m_ifShift * 10;

            if (m_mode == "LSB" || m_mode == "DATA-R") {
                // LSB/DATA-R: passband is below dial, shift indicates center offset (negative)
                qint64 center = m_tunedFreq - shiftOffsetHz;
                lowFreq = center - m_filterBw / 2;
                highFreq = center + m_filterBw / 2;
            } else if (m_mode == "USB" || m_mode == "DATA") {
                // USB/DATA: passband is above dial, shift indicates center offset
                qint64 center = m_tunedFreq + shiftOffsetHz;
                lowFreq = center - m_filterBw / 2;
                highFreq = center + m_filterBw / 2;
            } else if (m_mode == "CW" || m_mode == "CW-R") {
                // CW: shift already includes pitch offset from K4
                int pitchOffset = (m_mode == "CW") ? m_cwPitch : -m_cwPitch;
                qint64 center = m_tunedFreq + pitchOffset;
                lowFreq = center - m_filterBw / 2;
                highFreq = center + m_filterBw / 2;
            } else {
                // AM/FM - symmetric around carrier (both sidebands, no IF shift)
                lowFreq = m_tunedFreq - m_filterBw / 2;
                highFreq = m_tunedFreq + m_filterBw / 2;
            }

            // Convert to pixel coordinates
            float x1 = freqToNormalized(lowFreq) * w;
            float x2 = freqToNormalized(highFreq) * w;

            // Clamp to visible area
            x1 = qBound(0.0f, x1, w);
            x2 = qBound(0.0f, x2, w);

            if (x2 > x1) {
                // Draw passband quad - use dedicated VBO, uniform buffer, and SRB
                // Use spectrumHeight not h - passband should only appear in spectrum area, not waterfall
                QVector<float> quadVerts = {
                    x1, 0, x2, 0, x2, spectrumHeight, x1, 0, x2, spectrumHeight, x1, spectrumHeight};

                overlayRub->updateDynamicBuffer(m_passbandVbo.get(), 0, quadVerts.size() * sizeof(float),
                                           quadVerts.constData());

                struct {
                    float viewportWidth;
                    float viewportHeight;
                    float pad0, pad1;
                    float r, g, b, a;
                } pbUniforms = {w,
                                h,
                                0,
                                0,
                                static_cast<float>(m_passbandColor.redF()),
                                static_cast<float>(m_passbandColor.greenF()),
                                static_cast<float>(m_passbandColor.blueF()),
                                static_cast<float>(m_passbandColor.alphaF())};
                overlayRub->updateDynamicBuffer(m_passbandUniformBuffer.get(), 0, sizeof(pbUniforms), &pbUniforms);

                draws.push_back([=]() {
                    cb->setViewport({0, 0, w, h});
                    cb->setGraphicsPipeline(m_overlayTrianglePipeline.get());
                    cb->setShaderResources(m_passbandSrb.get());
                    const QRhiCommandBuffer::VertexInput pbVbufBinding(m_passbandVbo.get(), 0);
                    cb->setVertexInput(0, 1, &pbVbufBinding);
                    cb->draw(6);
                });
            }

            // Draw frequency marker - use dedicated VBO, uniform buffer, and SRB
            // Use spectrumHeight not h - marker should only appear in spectrum area, not waterfall
            // For CW modes: marker at passband center (dial + pitch offset)
            // For SSB/other: marker at dial frequency (passband shifts around it)
            qint64 markerFreq = m_tunedFreq;
            if (m_mode == "CW") {
                // CW marker at passband center (pitch offset from dial)
                markerFreq = m_tunedFreq + m_cwPitch;
            } else if (m_mode == "CW-R") {
                // CW-R marker at passband center (pitch offset below dial)
                markerFreq = m_tunedFreq - m_cwPitch;
            }
            // For USB/LSB/AM/FM: marker stays at dial frequency
            float markerX = freqToNormalized(markerFreq) * w;
            if (markerX >= 0 && markerX <= w) {
                // Draw as filled rectangle (2px wide) instead of line for robust Metal rendering
                float markerWidth = 2.0f;
                QVector<float> markerVerts = {markerX,
                                              0.0f,
                                              markerX + markerWidth,
                                              0.0f,
                                              markerX + markerWidth,
                                              spectrumHeight,
                                              markerX,
                                              0.0f,
                                              markerX + markerWidth,
                                              spectrumHeight,
                                              markerX,
                                              spectrumHeight};

                overlayRub->updateDynamicBuffer(m_markerVbo.get(), 0, markerVerts.size() * sizeof(float),
                                           markerVerts.constData());

                struct {
                    float viewportWidth;
                    float viewportHeight;
                    float pad0, pad1;
                    float r, g, b, a;
                } mkUniforms = {w,
                                h,
                                0,
                                0,
                                static_cast<float>(m_frequencyMarkerColor.redF()),
                                static_cast<float>(m_frequencyMarkerColor.greenF()),
                                static_cast<float>(m_frequencyMarkerColor.blueF()),
                                static_cast<float>(m_frequencyMarkerColor.alphaF())};
                overlayRub->updateDynamicBuffer(m_markerUniformBuffer.get(), 0, sizeof(mkUniforms), &mkUniforms);

                draws.push_back([=]() {
                    cb->setViewport({0, 0, w, h});
                    cb->setGraphicsPipeline(m_overlayTrianglePipeline.get());
                    cb->setShaderResources(m_markerSrb.get());
                    const QRhiCommandBuffer::VertexInput mkVbufBinding(m_markerVbo.get(), 0);
                    cb->setVertexInput(0, 1, &mkVbufBinding);
                    cb->draw(6);
                });
            }

            // Draw notch filter marker (dotted line) - uses dedicated notch buffers
            // Calculate notch offset from tunedFreq (consistent with mini-pan)
            if (m_notchEnabled && m_notchPitchHz > 0 && m_spanHz > 0) {
                // NM value is audio frequency offset from dial frequency (tunedFreq).
                // CW/CW-R and DATA/DATA-R use USB/LSB mapping respectively:
                // CW:   notchRF = tunedFreq + NM  (USB-like sideband)
                // CW-R: notchRF = tunedFreq - NM  (LSB-like sideband)
                int offsetHz;
                if (m_mode == "LSB" || m_mode == "CW-R" || m_mode == "DATA-R") {
                    offsetHz = -m_notchPitchHz;
                } else {
                    // USB, CW, DATA, AM, FM
                    offsetHz = m_notchPitchHz;
                }

                float tunedX = freqToNormalized(m_tunedFreq) * w;
                float notchX = tunedX + (static_cast<float>(offsetHz) * w) / m_spanHz;
                bool inBounds = (notchX >= 0 && notchX <= w);

                if (inBounds) {
                    // Draw as dotted line (2px wide segments with gaps)
                    float notchWidth = 2.0f;
                    float dashLen = 6.0f;
                    float gapLen = 4.0f;
                    float stride = dashLen + gapLen;
                    QVector<float> notchVerts;
                    for (float y = 0.0f; y < spectrumHeight; y += stride) {
                        float yEnd = qMin(y + dashLen, spectrumHeight);
                        // Two triangles per dash segment
                        notchVerts << notchX << y << notchX + notchWidth << y << notchX + notchWidth << yEnd << notchX
                                   << y << notchX + notchWidth << yEnd << notchX << yEnd;
                    }

                    overlayRub->updateDynamicBuffer(m_notchVbo.get(), 0, notchVerts.size() * sizeof(float),
                                                  notchVerts.constData());

                    struct {
                        float viewportWidth;
                        float viewportHeight;
                        float pad0, pad1;
                        float r, g, b, a;
                    } notchUniforms = {w,
                                       h,
                                       0,
                                       0,
                                       static_cast<float>(m_notchColor.redF()),
                                       static_cast<float>(m_notchColor.greenF()),
                                       static_cast<float>(m_notchColor.blueF()),
                                       static_cast<float>(m_notchColor.alphaF())};
                    overlayRub->updateDynamicBuffer(m_notchUniformBuffer.get(), 0, sizeof(notchUniforms), &notchUniforms);

                    draws.push_back([=]() {
                        cb->setViewport({0, 0, w, h});
                        cb->setGraphicsPipeline(m_overlayTrianglePipeline.get());
                        cb->setShaderResources(m_notchSrb.get());
                        const QRhiCommandBuffer::VertexInput notchVbufBinding(m_notchVbo.get(), 0);
                        cb->setVertexInput(0, 1, &notchVbufBinding);
                        cb->draw(notchVerts.size() / 2); // 2 floats per vertex
                    });
                }
            }
        }
    }

    cb->beginPass(renderTarget(), QColor::fromRgbF(0.08f, 0.08f, 0.08f, 1.0f), {1.0f, 0}, overlayRub);
    for (auto &d : draws)
        d();
    cb->endPass();
}

void PanadapterRhiWidget::updateSpectrum(const QByteArray &bins, qint64 centerFreq, qint32 sampleRate,
                                         float noiseFloor) {
    m_centerFreq = centerFreq;
    m_sampleRate = sampleRate;
    m_noiseFloor = noiseFloor;

    // K4 tier span = sampleRate * 1000 Hz
    qint32 tierSpanHz = sampleRate * 1000;
    int totalBins = bins.size();

    // Extract center bins if tier span > commanded span
    QByteArray binsToUse;
    if (tierSpanHz > m_spanHz && totalBins > 100 && m_spanHz > 0) {
        int requestedBins = (static_cast<qint64>(m_spanHz) * totalBins) / tierSpanHz;
        requestedBins = qBound(50, requestedBins, totalBins);
        int centerStart = (totalBins - requestedBins) / 2; // Center extraction
        binsToUse = bins.mid(centerStart, requestedBins);
    } else {
        binsToUse = bins;
    }

    // A held bin only has meaning while the spectrum geometry is unchanged.
    // Reset before consuming this frame when a retune/recenter, span/tier, or
    // bin-count change would otherwise map old peaks to different frequencies.
    const bool peakGeometryChanged = !m_peakGeometryValid || m_peakGeometryCenterFreq != centerFreq ||
                                     m_peakGeometrySampleRate != sampleRate || m_peakGeometrySpanHz != m_spanHz ||
                                     m_peakGeometryBinCount != binsToUse.size();
    m_peakGeometryCenterFreq = centerFreq;
    m_peakGeometrySampleRate = sampleRate;
    m_peakGeometrySpanHz = m_spanHz;
    m_peakGeometryBinCount = binsToUse.size();
    m_peakGeometryValid = true;
    if (peakGeometryChanged)
        m_peakHold.clear();

    // Decompress bins to dB values
    decompressBins(binsToUse, m_rawSpectrum);

    // Apply exponential smoothing for gradual decay (attack fast, decay slow)
    constexpr float attackAlpha = 0.85f; // Fast attack (new peaks appear quickly)
    constexpr float decayAlpha = 0.45f;  // Moderate decay for crisp waterfall

    if (m_currentSpectrum.size() != m_rawSpectrum.size()) {
        m_currentSpectrum = m_rawSpectrum;
    } else {
        for (int i = 0; i < m_rawSpectrum.size(); ++i) {
            float alpha = (m_rawSpectrum[i] > m_currentSpectrum[i]) ? attackAlpha : decayAlpha;
            m_currentSpectrum[i] = alpha * m_rawSpectrum[i] + (1.0f - alpha) * m_currentSpectrum[i];
        }
    }

    // Update peak hold
    if (m_peakHoldEnabled) {
        if (m_peakHold.size() != m_currentSpectrum.size()) {
            m_peakHold = m_currentSpectrum;
        } else {
            for (int i = 0; i < m_currentSpectrum.size(); ++i) {
                if (m_currentSpectrum[i] > m_peakHold[i]) {
                    m_peakHold[i] = m_currentSpectrum[i];
                }
            }
        }
    }

    m_waterfallNeedsUpdate = true;
    updateFreqScaleOverlay(); // Update frequency labels when center freq changes
    update();
}

void PanadapterRhiWidget::setAudioView(double lowHz, double spanHz) {
    if (!std::isfinite(lowHz) || !std::isfinite(spanHz) || spanHz <= 0) return;
    if (!m_audioView) {
        // Set before showing the widget; retain audio history independently of
        // its viewport so zooming never rewrites historical frequencies.
        Q_ASSERT(!m_rhiInitialized);
        m_audioView = true;
        m_waterfallHistory = 256;
        m_waterfallData.fill(0, m_textureWidth * m_waterfallHistory);
        m_dbmScaleOverlay->hide();
        m_freqScaleOverlay->setAudioUnits(true);
        m_cursorVisible = false;
        m_filterBw = 0;
    }
    m_audioLowHz = lowHz;
    m_audioSpanHz = spanHz;
    m_centerFreq = qRound64(lowHz + spanHz / 2);
    m_spanHz = qRound(spanHz);
    updateAudioTrace();
    updateFreqScaleOverlay();
    update();
}

void PanadapterRhiWidget::updateAudioTrace() {
    if (m_audioSpectrum.isEmpty()) { m_currentSpectrum.clear(); return; }
    const int count = qBound(2, qCeil(m_audioSpanHz / m_audioBinHz), m_textureWidth);
    m_currentSpectrum.resize(count);
    for (int i = 0; i < count; ++i) {
        const double bin = (m_audioLowHz + (i + 0.5) * m_audioSpanHz / count - m_audioFirstHz) / m_audioBinHz;
        const int left = int(std::floor(bin));
        if (left < 0 || left >= m_audioSpectrum.size()) { m_currentSpectrum[i] = m_minDb; continue; }
        const int right = qMin(left + 1, int(m_audioSpectrum.size()) - 1);
        const float fraction = float(bin - left);
        m_currentSpectrum[i] = m_audioSpectrum[left] * (1 - fraction) + m_audioSpectrum[right] * fraction;
    }
}

void PanadapterRhiWidget::updateAudioSpectrum(const QVector<float> &db, double firstBinHz, double binHz) {
    if (!m_audioView || db.isEmpty() || db.size() > m_textureWidth ||
        !std::isfinite(firstBinHz) || !std::isfinite(binHz) || binHz <= 0) return;
    const bool geometryChanged = !m_audioSpectrum.isEmpty() &&
        (db.size() != m_audioSpectrum.size() || firstBinHz != m_audioFirstHz || binHz != m_audioBinHz);
    if (geometryChanged) {
        m_waterfallData.fill(0); m_waterfallWriteRow = 0; m_audioFloorValid = false;
    }
    m_audioFirstHz = firstBinHz; m_audioBinHz = binHz;
    m_audioSpectrum = db;
    for (auto &value : m_audioSpectrum) if (!std::isfinite(value)) value = -120;
    auto sorted = m_audioSpectrum;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 3, sorted.end());
    const float floor = sorted[sorted.size() / 3];
    m_audioFloor = m_audioFloorValid ? 0.92f * m_audioFloor + 0.08f * floor : floor;
    m_audioFloorValid = true;
    m_minDb = m_audioFloor - 24; m_maxDb = m_audioFloor + 48;
    updateAudioTrace();
    const int offset = (m_textureWidth - int(db.size())) / 2;
    quint8 *row = m_waterfallData.data() + m_waterfallWriteRow * m_textureWidth;
    std::memset(row, 0, m_textureWidth);
    for (int i = 0; i < m_audioSpectrum.size(); ++i)
        row[offset + i] = quint8(qBound(0, int(normalizeDb(m_audioSpectrum[i]) * 255), 255));
    m_waterfallWriteRow = (m_waterfallWriteRow + 1) % m_waterfallHistory;
    // Audio arrives much more slowly than PAN frames. A bounded 1 MB history
    // upload also preserves every row when several arrive before a UI frame.
    m_waterfallNeedsFullClear = true;
    m_waterfallNeedsUpdate = false;
    update();
}

void PanadapterRhiWidget::updateMiniSpectrum(const QByteArray &bins) {
    m_rawSpectrum.resize(bins.size());
    for (int i = 0; i < bins.size(); ++i) {
        m_rawSpectrum[i] = static_cast<quint8>(bins[i]) * 10.0f - 160.0f;
    }

    // Apply exponential smoothing for gradual decay (attack fast, decay slow)
    constexpr float attackAlpha = 0.85f; // Fast attack
    constexpr float decayAlpha = 0.38f;  // Slower decay (visible glow effect)

    if (m_currentSpectrum.size() != m_rawSpectrum.size()) {
        m_currentSpectrum = m_rawSpectrum;
    } else {
        for (int i = 0; i < m_rawSpectrum.size(); ++i) {
            float alpha = (m_rawSpectrum[i] > m_currentSpectrum[i]) ? attackAlpha : decayAlpha;
            m_currentSpectrum[i] = alpha * m_rawSpectrum[i] + (1.0f - alpha) * m_currentSpectrum[i];
        }
    }

    m_waterfallNeedsUpdate = true;
    update();
}

void PanadapterRhiWidget::decompressBins(const QByteArray &bins, QVector<float> &out) {
    // K4 spectrum bins: dBm = raw_byte - K4_DBM_OFFSET
    out.resize(bins.size());
    for (int i = 0; i < bins.size(); ++i) {
        out[i] = static_cast<quint8>(bins[i]) - K4_DBM_OFFSET;
    }
}

void PanadapterRhiWidget::updateWaterfallData() {
    if (m_currentSpectrum.isEmpty())
        return;

    // Upload raw bins centered in texture for shader sampling
    int row = m_waterfallWriteRow;
    int specSize = m_currentSpectrum.size();
    int offset = (m_textureWidth - specSize) / 2;

    // Clear row (zeros outside bin region = no signal)
    std::memset(&m_waterfallData[row * m_textureWidth], 0, m_textureWidth);

    // Copy raw bins (no interpolation - GPU handles it)
    for (int i = 0; i < specSize; ++i) {
        float normalized = normalizeDb(m_currentSpectrum[i]);
        m_waterfallData[row * m_textureWidth + offset + i] =
            static_cast<quint8>(qBound(0, static_cast<int>(normalized * 255), 255));
    }
}

float PanadapterRhiWidget::normalizeDb(float db) {
    return qBound(0.0f, (db - m_minDb) / (m_maxDb - m_minDb), 1.0f);
}

float PanadapterRhiWidget::freqToNormalized(qint64 freq) {
    // Map frequency to normalized range [0.0, 1.0] where:
    // - 0.0 = left edge (startFreq)
    // - 1.0 = right edge (startFreq + spanHz)
    //
    // IMPORTANT: In CW mode, the K4 centers the spectrum on (dial + cwPitch), not the dial frequency.
    // This is because the IF center is offset by the CW sidetone pitch.
    qint64 effectiveCenter = m_centerFreq;
    if (m_mode == "CW") {
        effectiveCenter = m_centerFreq + m_cwPitch;
    } else if (m_mode == "CW-R") {
        effectiveCenter = m_centerFreq - m_cwPitch;
    }
    qint64 startFreq = effectiveCenter - m_spanHz / 2;
    return static_cast<float>(freq - startFreq) / static_cast<float>(m_spanHz);
}

qint64 PanadapterRhiWidget::xToFreq(int x, int w) {
    // Map pixel position to frequency for click-to-tune
    // Use floating point for precision
    //
    // NOTE: Do NOT apply CW pitch offset here. The user clicks on a signal at a certain
    // visual position. That signal's frequency is what we want to tune to.
    // The spectrum display already shows frequencies correctly; we just need to map
    // the click position back to frequency using the centerFreq from the K4.
    if (w <= 0)
        return m_centerFreq;
    qint64 startFreq = m_centerFreq - m_spanHz / 2;
    // Clamp to [0, 1] to prevent runaway acceleration when dragging past edges
    double normalized = qBound(0.0, static_cast<double>(x) / static_cast<double>(w), 1.0);
    return startFreq + static_cast<qint64>(normalized * m_spanHz);
}

QColor PanadapterRhiWidget::interpolateColor(const QColor &a, const QColor &b, float t) {
    t = qBound(0.0f, t, 1.0f);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QColor PanadapterRhiWidget::spectrumGradientColor(float t) {
    // 5-stop gradient: visible dark lime → lime green → bright lime → light lime → white
    // Creates a lime green spectrum fill with visible base color
    struct GradientStop {
        float pos;
        int r, g, b, a;
    };
    static const GradientStop stops[] = {
        {0.00f, 20, 60, 20, 128},    // Visible dark lime (50% alpha)
        {0.15f, 40, 120, 30, 180},   // Translucent lime green
        {0.50f, 80, 200, 60, 220},   // Bright lime green
        {0.75f, 160, 255, 120, 245}, // Light lime with yellow hint
        {1.00f, 255, 255, 255, 255}  // Pure white peak
    };

    t = qBound(0.0f, t, 1.0f);

    // Find surrounding stops and interpolate
    for (int i = 0; i < 4; ++i) {
        if (t <= stops[i + 1].pos) {
            float localT = (t - stops[i].pos) / (stops[i + 1].pos - stops[i].pos);
            QColor c1(stops[i].r, stops[i].g, stops[i].b, stops[i].a);
            QColor c2(stops[i + 1].r, stops[i + 1].g, stops[i + 1].b, stops[i + 1].a);
            return interpolateColor(c1, c2, localT);
        }
    }
    return QColor(255, 255, 255, 255); // Clamp to white
}

// Configuration setters
void PanadapterRhiWidget::setDbRange(float minDb, float maxDb) {
    m_minDb = minDb;
    m_maxDb = maxDb;
    updateDbmScaleOverlay(); // Update overlay labels when range changes
    update();
}

void PanadapterRhiWidget::setSpectrumRatio(float ratio) {
    m_spectrumRatio = qBound(0.1f, ratio, 0.9f);
    updateDbmScaleOverlay();  // Resize dBm scale to match new spectrum area
    updateFreqScaleOverlay(); // Reposition frequency labels at boundary
    update();
}

void PanadapterRhiWidget::setWaterfallHeight(int percent) {
    // Waterfall height percentage: 50% means 50% waterfall, 50% spectrum
    // Spectrum ratio = (100 - waterfallHeight) / 100
    float ratio = (100.0f - qBound(10, percent, 90)) / 100.0f;
    m_spectrumRatio = qBound(0.1f, ratio, 0.9f);
    updateDbmScaleOverlay();  // Resize dBm scale to match new spectrum area
    updateFreqScaleOverlay(); // Reposition frequency labels at boundary
    update();
}

void PanadapterRhiWidget::setTunedFrequency(qint64 freq) {
    if (m_tunedFreq != freq) {
        m_tunedFreq = freq;
        m_showWaterfallMarker = true;
        m_waterfallMarkerTimer->start(500);
        update();
    }
}

void PanadapterRhiWidget::setFilterBandwidth(int bwHz) {
    m_filterBw = bwHz;
    update();
}

void PanadapterRhiWidget::setMode(const QString &mode) {
    m_mode = mode;
    updateFreqScaleOverlay();
    update();
}

void PanadapterRhiWidget::setIfShift(int shift) {
    if (m_ifShift != shift) {
        m_ifShift = shift;
        update();
    }
}

void PanadapterRhiWidget::setCwPitch(int pitchHz) {
    if (m_cwPitch != pitchHz) {
        m_cwPitch = pitchHz;
        updateFreqScaleOverlay();
        update();
    }
}

void PanadapterRhiWidget::clear() {
    m_audioSpectrum.clear();
    m_audioFloorValid = false;
    m_currentSpectrum.clear();
    m_rawSpectrum.clear();
    m_peakHold.clear();
    m_peakGeometryValid = false;
    m_waterfallWriteRow = 0;
    m_waterfallData.fill(0);
    m_waterfallNeedsFullClear = true;

    // Reset frequency/mode/overlay state so reconnect starts clean
    m_centerFreq = 0;
    m_tunedFreq = 0;
    m_spanHz = 10000;
    m_mode = "USB";
    m_cwPitch = 500;
    m_ifShift = 50;
    m_filterBw = 2400;
    m_notchEnabled = false;
    m_notchPitchHz = 0;
    m_cursorVisible = false;

    // Secondary VFO (visibility is a UI preference set at construction;
    // rendering is gated by freq/bw > 0 which are reset here)
    m_secondaryTunedFreq = 0;
    m_secondaryFilterBw = 0;

    // Hide frequency labels (paintEvent returns early when spanHz <= 0)
    if (m_freqScaleOverlay)
        m_freqScaleOverlay->setFrequencyRange(0, 0, 0, "");

    update();
}

void PanadapterRhiWidget::setGridEnabled(bool enabled) {
    m_gridEnabled = enabled;
    update();
}

void PanadapterRhiWidget::setWaterfallColor(int color) {
    color = qBound(0, color, 4);
    if (m_waterfallColor == color)
        return;
    m_waterfallColor = color;
    initColorLUT();
    m_waterfallColorNeedsUpdate = m_rhiInitialized;
    update();
    emit waterfallAppearanceChanged(m_waterfallColor, m_waterfallColorRange);
}

void PanadapterRhiWidget::setWaterfallColorRange(int range) {
    range = qBound(5, range, 30);
    if (m_waterfallColorRange == range)
        return;

    m_waterfallColorRange = range;
    initColorLUT();
    m_waterfallColorNeedsUpdate = m_rhiInitialized;
    update();
    emit waterfallAppearanceChanged(m_waterfallColor, m_waterfallColorRange);
}

void PanadapterRhiWidget::setPeakHoldEnabled(bool enabled) {
    if (m_peakHoldEnabled == enabled)
        return;

    m_peakHoldEnabled = enabled;
    // Peak starts fresh each time it is enabled; it is intentionally sticky
    // thereafter and advances only when a stronger value arrives for a bin.
    m_peakHold.clear();
    if (enabled && !m_currentSpectrum.isEmpty()) {
        m_peakHold = m_currentSpectrum;
    }
    update();
}

void PanadapterRhiWidget::setRefLevel(int level) {
    if (m_refLevel != level) {
        m_refLevel = level;
        updateDbRangeFromRefAndScale();
        update();
    }
}

void PanadapterRhiWidget::setScale(int scale) {
    // Scale range: 10-150 (per K4 documentation)
    // Higher values = more compressed display (signals appear weaker, wider dB range)
    // Lower values = more expanded display (signals appear stronger, narrower dB range)
    if (m_scale != scale && scale >= 10 && scale <= 150) {
        m_scale = scale;
        updateDbRangeFromRefAndScale();
        update();
    }
}

void PanadapterRhiWidget::updateDbRangeFromRefAndScale() {
    // RefLevel is the bottom reference, scale is the dB range upward
    // Display shows from refLevel to (refLevel + scale)
    m_minDb = static_cast<float>(m_refLevel);
    m_maxDb = static_cast<float>(m_refLevel) + static_cast<float>(m_scale);

    updateDbmScaleOverlay();
}

void PanadapterRhiWidget::setSpan(int spanHz) {
    if (m_spanHz != spanHz && spanHz > 0) {
        m_spanHz = spanHz;
        // The same bin index now represents a different frequency range.
        m_peakHold.clear();
        m_peakGeometryValid = false;
        updateFreqScaleOverlay();
        update();
    }
}

void PanadapterRhiWidget::setNotchFilter(bool enabled, int pitchHz) {
    if (m_notchEnabled != enabled || m_notchPitchHz != pitchHz) {
        m_notchEnabled = enabled;
        m_notchPitchHz = pitchHz;
        update();
    }
}

void PanadapterRhiWidget::setCursorVisible(bool visible) {
    if (m_cursorVisible != visible) {
        m_cursorVisible = visible;
        update();
    }
}

void PanadapterRhiWidget::setAmplitudeUnits(bool useSUnits) {
    if (m_dbmScaleOverlay) {
        m_dbmScaleOverlay->setUseSUnits(useSUnits);
    }
}

// Secondary VFO setters
void PanadapterRhiWidget::setSecondaryVfo(qint64 freq, int bwHz, const QString &mode, int ifShift, int cwPitch) {
    m_secondaryTunedFreq = freq;
    m_secondaryFilterBw = bwHz;
    m_secondaryMode = mode;
    m_secondaryIfShift = ifShift;
    m_secondaryCwPitch = cwPitch;
    update();
}

void PanadapterRhiWidget::setSecondaryVisible(bool visible) {
    if (m_secondaryVisible != visible) {
        m_secondaryVisible = visible;
        update();
    }
}

void PanadapterRhiWidget::setSecondaryPassbandColor(const QColor &color) {
    m_secondaryPassbandColor = color;
    update();
}

void PanadapterRhiWidget::setSecondaryMarkerColor(const QColor &color) {
    m_secondaryMarkerColor = color;
    update();
}

// Color setters
void PanadapterRhiWidget::setSpectrumBaseColor(const QColor &color) {
    m_spectrumBaseColor = color;
    update();
}

void PanadapterRhiWidget::setSpectrumPeakColor(const QColor &color) {
    m_spectrumPeakColor = color;
    update();
}

void PanadapterRhiWidget::setSpectrumLineColor(const QColor &color) {
    m_spectrumLineColor = color;
    update();
}

void PanadapterRhiWidget::setGridColor(const QColor &color) {
    m_gridColor = color;
    update();
}

void PanadapterRhiWidget::setPeakHoldColor(const QColor &color) {
    m_peakHoldColor = color;
    update();
}

void PanadapterRhiWidget::setPassbandColor(const QColor &color) {
    m_passbandColor = color;
    update();
}

void PanadapterRhiWidget::setFrequencyMarkerColor(const QColor &color) {
    m_frequencyMarkerColor = color;
    update();
}

void PanadapterRhiWidget::setNotchColor(const QColor &color) {
    m_notchColor = color;
    update();
}

void PanadapterRhiWidget::setBackgroundGradient(const QColor &center, const QColor &edge) {
    m_bgCenterColor = center;
    m_bgEdgeColor = edge;
    update();
}

// Mouse events
void PanadapterRhiWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_isRightDragging = false;
        qint64 freq = xToFreq(event->pos().x(), width());
        emit frequencyClicked(freq);
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        m_isRightDragging = true;
        m_isDragging = false;
        qint64 freq = xToFreq(event->pos().x(), width());
        emit frequencyRightClicked(freq);
        event->accept();
    }
}

void PanadapterRhiWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        int x = event->pos().x();
        int w = width();

        if (x < 0) {
            // Dragging past left edge → emit scroll signal (like wheel down)
            // Rate limit to prevent flooding: at most one scroll per 100ms
            if (!m_edgeScrollTimer.isValid() || m_edgeScrollTimer.elapsed() >= 100) {
                m_edgeScrollTimer.restart();
                emit frequencyScrolled(-1);
            }
        } else if (x > w) {
            // Dragging past right edge → emit scroll signal (like wheel up)
            if (!m_edgeScrollTimer.isValid() || m_edgeScrollTimer.elapsed() >= 100) {
                m_edgeScrollTimer.restart();
                emit frequencyScrolled(1);
            }
        } else {
            // Normal drag within display bounds
            qint64 freq = xToFreq(x, w);
            emit frequencyDragged(freq);
        }
        event->accept();
    } else if (m_isRightDragging && (event->buttons() & Qt::RightButton)) {
        int x = event->pos().x();
        int w = width();

        if (x < 0) {
            // Dragging past left edge → emit scroll signal for opposite VFO
            if (!m_edgeScrollTimer.isValid() || m_edgeScrollTimer.elapsed() >= 100) {
                m_edgeScrollTimer.restart();
                emit frequencyScrolled(-1);
            }
        } else if (x > w) {
            // Dragging past right edge → emit scroll signal for opposite VFO
            if (!m_edgeScrollTimer.isValid() || m_edgeScrollTimer.elapsed() >= 100) {
                m_edgeScrollTimer.restart();
                emit frequencyScrolled(1);
            }
        } else {
            // Normal drag within display bounds
            qint64 freq = xToFreq(x, w);
            emit frequencyRightDragged(freq);
        }
        event->accept();
    }
}

void PanadapterRhiWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        m_isRightDragging = false;
        event->accept();
    }
}

void PanadapterRhiWidget::wheelEvent(QWheelEvent *event) {
    int key = 0; // frequency (no modifier)
    if (event->modifiers() & Qt::ShiftModifier)
        key = 1; // scale
    else if (event->modifiers() & Qt::ControlModifier)
        key = 2; // ref level

    int steps = m_wheelAccumulator.accumulate(event, key);
    if (steps != 0) {
        switch (key) {
        case 1:
            emit scaleScrolled(steps);
            break;
        case 2:
            emit refLevelScrolled(steps);
            break;
        default:
            emit frequencyScrolled(steps);
            break;
        }
    }
    event->accept();
}
