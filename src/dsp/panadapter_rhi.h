#ifndef PANADAPTER_RHI_H
#define PANADAPTER_RHI_H

#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <QColor>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <memory>
#include "../ui/wheelaccumulator.h"

// Forward declarations for overlay widgets
class DbmScaleOverlay;
class FrequencyScaleOverlay;

// Modern GPU-accelerated panadapter using Qt RHI
// Supports Metal (macOS), DirectX (Windows), Vulkan (Linux)
class PanadapterRhiWidget : public QRhiWidget {
    Q_OBJECT

public:
    explicit PanadapterRhiWidget(QWidget *parent = nullptr);
    ~PanadapterRhiWidget();

    // Update spectrum data from K4 PAN packet
    void updateSpectrum(const QByteArray &bins, qint64 centerFreq, qint32 sampleRate, float noiseFloor);

    // Update from MiniPAN packet (simpler format)
    void updateMiniSpectrum(const QByteArray &bins);

    // Audio tools use the same GPU spectrum, waterfall, palette and history.
    // Frequencies are audio Hz; amplitudes are relative, never radio dBm.
    void setAudioView(double lowHz, double spanHz);
    void updateAudioSpectrum(const QVector<float> &db, double firstBinHz, double binHz);
    bool isAudioView() const { return m_audioView; }
    int waterfallColor() const { return m_waterfallColor; }
    int waterfallColorRange() const { return m_waterfallColorRange; }

    // Configuration
    void setDbRange(float minDb, float maxDb);
    void setSpectrumRatio(float ratio);
    void setTunedFrequency(qint64 freq);
    void setFilterBandwidth(int bwHz);
    void setMode(const QString &mode);
    void setIfShift(int shift);
    void setCwPitch(int pitchHz);
    void clear();

    // Display settings
    void setGridEnabled(bool enabled);
    void setWaterfallColor(int color);
    // Local WTR CLRS mapping using Elecraft's native 5-30 setting.
    void setWaterfallColorRange(int range);
    void setPeakHoldEnabled(bool enabled);
    void setRefLevel(int level);
    void setScale(int scale); // 25-150, affects display gain/range
    void setSpan(int spanHz);
    void setWaterfallHeight(int percent); // 0-100: percentage of display for waterfall
    int span() const { return m_spanHz; }
    void setNotchFilter(bool enabled, int pitchHz);
    void setCursorVisible(bool visible);
    void setAmplitudeUnits(bool useSUnits); // false=dBm, true=S-units

    // Secondary VFO (other receiver's passband)
    void setSecondaryVfo(qint64 freq, int bwHz, const QString &mode, int ifShift, int cwPitch);
    void setSecondaryVisible(bool visible);
    void setSecondaryPassbandColor(const QColor &color);
    void setSecondaryMarkerColor(const QColor &color);

    // Color configuration (NEW: all colors configurable)
    void setSpectrumBaseColor(const QColor &color);
    void setSpectrumPeakColor(const QColor &color);
    void setSpectrumLineColor(const QColor &color);
    void setGridColor(const QColor &color);
    void setPeakHoldColor(const QColor &color);
    void setPassbandColor(const QColor &color);
    void setFrequencyMarkerColor(const QColor &color);
    void setNotchColor(const QColor &color);
    void setBackgroundGradient(const QColor &center, const QColor &edge);

signals:
    void waterfallAppearanceChanged(int palette, int range);
    void frequencyClicked(qint64 freq);
    void frequencyDragged(qint64 freq);
    void frequencyScrolled(int steps);
    // Right-click signals (for tuning opposite VFO)
    void frequencyRightClicked(qint64 freq);
    void frequencyRightDragged(qint64 freq);
    // Scale/RefLevel scroll signals (Shift+Wheel, Ctrl+Wheel)
    void scaleScrolled(int steps);
    void refLevelScrolled(int steps);

protected:
    // QRhiWidget overrides
    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;
    void releaseResources() override;
    void resizeEvent(QResizeEvent *event) override;

    // Input events
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Update dBm scale overlay position and values
    void updateDbmScaleOverlay();
    // Update frequency scale overlay position and values
    void updateFreqScaleOverlay();
    // Update dB range based on current ref level and scale
    void updateDbRangeFromRefAndScale();
    // Initialization
    void initColorLUT();
    void initSpectrumLUT();
    void createPipelines();

    // Data processing
    void decompressBins(const QByteArray &bins, QVector<float> &out);
    void updateWaterfallData();
    void updateAudioTrace();

    // Coordinate helpers
    float normalizeDb(float db);
    float freqToNormalized(qint64 freq);
    qint64 xToFreq(int x, int w);
    QColor interpolateColor(const QColor &a, const QColor &b, float t);
    QColor spectrumGradientColor(float t); // 5-stop teal-to-white gradient

    // RHI resources
    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiBuffer> m_waterfallVbo;
    std::unique_ptr<QRhiBuffer> m_waterfallUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_overlayVbo;
    std::unique_ptr<QRhiBuffer> m_overlayUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_peakVbo;
    std::unique_ptr<QRhiBuffer> m_peakUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_passbandVbo;
    std::unique_ptr<QRhiBuffer> m_passbandUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_markerVbo;
    std::unique_ptr<QRhiBuffer> m_markerUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_notchVbo;
    std::unique_ptr<QRhiBuffer> m_notchUniformBuffer;
    std::unique_ptr<QRhiTexture> m_waterfallTexture;
    std::unique_ptr<QRhiTexture> m_colorLutTexture;
    std::unique_ptr<QRhiTexture> m_spectrumDataTexture; // 1D texture for spectrum values
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiGraphicsPipeline> m_waterfallPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_overlayLinePipeline;
    // A continuous red line for local sticky Peak Hold.  The generic overlay
    // pipeline uses independent line pairs and cannot render a spectrum trace.
    std::unique_ptr<QRhiGraphicsPipeline> m_peakLinePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_overlayTrianglePipeline;
    std::unique_ptr<QRhiBuffer> m_fullscreenQuadVbo; // Shared fullscreen quad for fragment-shader styles
    // Spectrum amplitude style resources (LUT-based colors)
    std::unique_ptr<QRhiGraphicsPipeline> m_spectrumBlueAmpPipeline;
    std::unique_ptr<QRhiShaderResourceBindings> m_spectrumBlueAmpSrb;
    std::unique_ptr<QRhiBuffer> m_spectrumBlueAmpUniformBuffer;
    std::unique_ptr<QRhiTexture> m_spectrumColorLutTexture; // 256-entry color LUT
    std::unique_ptr<QRhiShaderResourceBindings> m_waterfallSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_overlaySrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_peakSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_passbandSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_markerSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_notchSrb;
    // Secondary passband buffers (for other VFO's overlay)
    std::unique_ptr<QRhiBuffer> m_secondaryPassbandVbo;
    std::unique_ptr<QRhiBuffer> m_secondaryPassbandUniformBuffer;
    std::unique_ptr<QRhiShaderResourceBindings> m_secondaryPassbandSrb;
    std::unique_ptr<QRhiBuffer> m_secondaryMarkerVbo;
    std::unique_ptr<QRhiBuffer> m_secondaryMarkerUniformBuffer;
    std::unique_ptr<QRhiShaderResourceBindings> m_secondaryMarkerSrb;
    QRhiRenderPassDescriptor *m_rpDesc = nullptr;

    bool m_rhiInitialized = false;
    bool m_pipelinesCreated = false;
    bool m_firstFrameRendered = false;

    // Shader stages (loaded from .qsb files)
    QShader m_spectrumBlueVert;
    QShader m_spectrumBlueAmpFrag;
    QShader m_waterfallVert;
    QShader m_waterfallFrag;
    QShader m_overlayVert;
    QShader m_overlayFrag;

    // Spectrum data
    QVector<float> m_currentSpectrum;
    bool m_audioView = false;
    QVector<float> m_audioSpectrum;
    double m_audioFirstHz = 100, m_audioBinHz = 6.25;
    double m_audioLowHz = 0, m_audioSpanHz = 3000;
    float m_audioFloor = -90;
    bool m_audioFloorValid = false;
    QVector<float> m_rawSpectrum;
    QVector<float> m_peakHold;
    qint64 m_peakGeometryCenterFreq = 0;
    qint32 m_peakGeometrySampleRate = 0;
    int m_peakGeometrySpanHz = 0;
    int m_peakGeometryBinCount = 0;
    bool m_peakGeometryValid = false;

    // K4 spectrum calibration: dBm = raw_byte - K4_DBM_OFFSET
    // Calibrated by comparing peak signals with K4 display
    static constexpr float K4_DBM_OFFSET = 146.0f;

    // Waterfall data - sized for 4K/HiDPI displays
    // Memory: 4096 × 1024 × 1 byte = 4 MB (trivial for modern GPUs)
    static constexpr int BASE_WATERFALL_HISTORY = 1024;
    static constexpr int BASE_TEXTURE_WIDTH = 4096;
    int m_textureWidth = BASE_TEXTURE_WIDTH;         // Scaled by devicePixelRatio
    int m_waterfallHistory = BASE_WATERFALL_HISTORY; // Scaled by devicePixelRatio
    int m_waterfallWriteRow = 0;
    QVector<quint8> m_waterfallData;
    bool m_waterfallNeedsUpdate = false;
    bool m_waterfallNeedsFullClear = false;

    // Color LUT (256 RGBA entries) - for waterfall
    QVector<quint8> m_colorLUT;
    int m_waterfallColor = 1;
    int m_waterfallColorRange = 10; // K4-style local WTR CLRS: 5-30
    bool m_waterfallColorNeedsUpdate = false;
    // Spectrum color LUT (256 RGBA entries) - for BlueAmplitude style
    QVector<quint8> m_spectrumLUT;

    // Frequency info
    qint64 m_centerFreq = 0;
    qint32 m_sampleRate = 192000;
    float m_noiseFloor = -130.0f;
    qint64 m_tunedFreq = 0;
    int m_filterBw = 2400;
    QString m_mode = "USB";
    int m_ifShift = 50;
    int m_cwPitch = 500;

    // Display settings
    float m_minDb = -138.0f;
    float m_maxDb = -58.0f;
    // Landscape phone console: spectrum and waterfall are deliberately equal
    // height so the live trace stays readable while retaining useful history.
    float m_spectrumRatio = 0.50f;
    float m_smoothedBaseline = 0.0f;
    bool m_gridEnabled = true;
    // Follow the radio's #PKM state; connection sync enables this when Peak is on.
    bool m_peakHoldEnabled = false;
    int m_refLevel = -110;
    int m_scale = 75; // 10-150, default 75 (neutral)
    int m_spanHz = 10000;
    bool m_notchEnabled = false;
    int m_notchPitchHz = 0;
    bool m_cursorVisible = true;

    // Mouse drag state
    bool m_isDragging = false;
    bool m_isRightDragging = false;
    QElapsedTimer m_edgeScrollTimer; // Rate limiting for edge drag scrolling

    // Secondary VFO (other receiver's passband)
    qint64 m_secondaryTunedFreq = 0;
    int m_secondaryFilterBw = 0;
    QString m_secondaryMode = "";
    int m_secondaryIfShift = 50;
    int m_secondaryCwPitch = 500;
    bool m_secondaryVisible = false;
    QColor m_secondaryPassbandColor{0, 255, 0, 64}; // Green 25% alpha
    QColor m_secondaryMarkerColor{0, 255, 0, 255};  // Green 100% alpha

    // Colors (all configurable)
    // Spectrum gradient uses spectrumGradientColor() for 5-stop lime-to-white
    QColor m_spectrumBaseColor{20, 60, 20, 128};    // Visible dark lime (gradient stop 0.0)
    QColor m_spectrumPeakColor{255, 255, 255, 255}; // Pure white peak (gradient stop 1.0)
    QColor m_spectrumLineColor{50, 255, 50};        // Lime green line on top
    QColor m_gridColor{160, 160, 160, 77};          // Light gray with 30% alpha
    // Peak ON must remain visually distinct from the gray grid/noise trace.
    QColor m_peakHoldColor{255, 0, 0, 255};         // Match the K4's red peak-hold trace
    QColor m_passbandColor{0, 191, 255, 64};        // Cyan with 25% alpha (VFO A default)
    QColor m_frequencyMarkerColor{0, 140, 200};     // Darker cyan (VFO A default)
    QColor m_notchColor{255, 0, 0};                 // Red
    QColor m_bgCenterColor{56, 56, 56};             // Lighter gray at center
    QColor m_bgEdgeColor{20, 20, 20};               // Darker at edges

    // Waterfall marker
    QTimer *m_waterfallMarkerTimer = nullptr;
    bool m_showWaterfallMarker = false;

    WheelAccumulator m_wheelAccumulator;

    // dBm scale overlay (child widget for text rendering)
    DbmScaleOverlay *m_dbmScaleOverlay = nullptr;
    // Frequency scale overlay (child widget for frequency labels at boundary)
    FrequencyScaleOverlay *m_freqScaleOverlay = nullptr;
};

#endif // PANADAPTER_RHI_H
