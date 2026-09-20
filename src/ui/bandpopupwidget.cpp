#include "bandpopupwidget.h"
#include "k4styles.h"
#include "settings/radiosettings.h"
#include <QGridLayout>
#include <QVBoxLayout>

namespace {
// Layout constants
const int ButtonWidth = 70;
const int ButtonHeight = 44;
const int ButtonSpacing = 8;
const int RowSpacing = 2;

struct ShortwaveBand {
    const char *label;
    quint64 centerHz;
};

// International broadcast bands and their center frequencies. The labels and
// frequency ranges are from https://en.wikipedia.org/wiki/Shortwave_bands;
// international broadcast service is ordinarily AM, selected in MainWindow.
const QList<ShortwaveBand> ShortwaveBands = {
    {"120m",  2397500}, {"90m",  3300000}, {"75m",  3950000},
    {"60m",   4872500}, {"49m",  6050000}, {"41m",  7325000},
    {"31m",   9650000}, {"25m", 11850000}, {"22m", 13720000},
    {"19m",  15450000}, {"16m", 17690000}, {"15m", 18960000},
    {"13m",  21650000}, {"11m", 25885000},
};

// K4 Band Number mapping (BN command)
// Band number -> button label
const QMap<int, QString> BandNumToName = {
    {0, "1.8"}, // 160m
    {1, "3.5"}, // 80m
    {2, "5"},   // 60m
    {3, "7"},   // 40m
    {4, "10"},  // 30m
    {5, "14"},  // 20m
    {6, "18"},  // 17m
    {7, "21"},  // 15m
    {8, "24"},  // 12m
    {9, "28"},  // 10m
    {10, "50"}, // 6m
    // 16-25 are transverter bands, all map to "XVTR"
};

// Button label -> band number
const QMap<QString, int> BandNameToNum = {
    {"1.8", 0}, {"3.5", 1}, {"5", 2},  {"7", 3},  {"10", 4},  {"14", 5},
    {"18", 6},  {"21", 7},  {"24", 8}, {"28", 9}, {"50", 10}, {"XVTR", 16}, // First transverter band (16-25 range)
};
} // namespace

BandPopupWidget::BandPopupWidget(QWidget *parent)
    : K4PopupBase(parent), m_selectedBand("14") // Default to 20m band
{
    setupUi();
}

QSize BandPopupWidget::contentSize() const {
    int cm = K4Styles::Dimensions::PopupContentMargin;

    const int columns = m_bank == ShortwaveBank ? 5 : (m_bank == TransverterBank ? 4 : 7);
    const int rows = m_bank == ShortwaveBank ? 3 : (m_bank == TransverterBank ? 4 : 2);
    int width = columns * ButtonWidth + (columns - 1) * ButtonSpacing + 2 * cm;
    int height = rows * ButtonHeight + (rows - 1) * RowSpacing + 2 * cm;
    return QSize(width, height);
}

void BandPopupWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(contentMargins());
    mainLayout->setSpacing(0);
    m_bandGrid = new QGridLayout();
    m_bandGrid->setSpacing(ButtonSpacing);
    mainLayout->addLayout(m_bandGrid);
    rebuildBandGrid();

    // Initialize popup size from base class
    initPopup();
}

void BandPopupWidget::rebuildBandGrid() {
    while (QLayoutItem *item = m_bandGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_buttonMap.clear();

    QStringList bands;
    int columns = 7;
    if (m_bank == ShortwaveBank) {
        // Keep GEN at the lower-left as the selected return toggle. The other
        // fourteen cells are the complete broadcast-band list in 3 x 5 form.
        bands = {"120m", "90m", "75m", "60m", "49m",
                 "41m", "31m", "25m", "22m", "19m",
                 "GEN", "16m", "15m", "13m", "11m"};
        columns = 5;
    } else if (m_bank == TransverterBank) {
        bands = {"XVTR1", "XVTR2", "XVTR3", "XVTR4",
                 "XVTR5", "XVTR6", "XVTR7", "XVTR8",
                 "HF", "XVTR9", "XVTR10", "XVTR11", "XVTR12"};
        columns = 4;
    } else {
        bands = {"1.8", "3.5", "7", "14", "21", "28", "MEM",
                 "GEN", "5", "10", "18", "24", "50", "XVTR"};
    }

    for (int index = 0; index < bands.size(); ++index) {
        const QString &band = bands.at(index);
        QPushButton *btn = createBandButton(band);
        m_buttonMap[band] = btn;
        m_bandGrid->addWidget(btn, index / columns, index % columns);
    }
    updateButtonStyles();

    const int sm = K4Styles::Dimensions::ShadowMargin;
    setFixedSize(contentSize() + QSize(2 * sm, 2 * sm));
}

QPushButton *BandPopupWidget::createBandButton(const QString &text) {
    QPushButton *btn = new QPushButton(text, this);
    btn->setFixedSize(ButtonWidth, ButtonHeight);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setProperty("bandName", text);

    connect(btn, &QPushButton::clicked, this, &BandPopupWidget::onBandButtonClicked);

    return btn;
}

void BandPopupWidget::updateButtonStyles() {
    for (auto it = m_buttonMap.begin(); it != m_buttonMap.end(); ++it) {
        if ((m_bank == ShortwaveBank && it.key() == "GEN") ||
            (m_bank == TransverterBank && it.key() == "HF") ||
            (m_bank == AmateurBank && it.key() == m_selectedBand) ||
            (m_bank == TransverterBank && it.key() == m_selectedBand)) {
            it.value()->setStyleSheet(K4Styles::popupButtonSelected());
        } else {
            it.value()->setStyleSheet(K4Styles::popupButtonNormal());
        }
    }
}

void BandPopupWidget::setSelectedBand(const QString &bandName) {
    if (m_buttonMap.contains(bandName)) {
        m_selectedBand = bandName;
        updateButtonStyles();
    }
}

void BandPopupWidget::onBandButtonClicked() {
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (btn) {
        QString bandName = btn->property("bandName").toString();
        if (bandName == "GEN") {
            m_bank = m_bank == ShortwaveBank ? AmateurBank : ShortwaveBank;
            m_shortwaveBandsActive = m_bank == ShortwaveBank;
            rebuildBandGrid();
            emit bandBankChanged();
            return;
        }
        if (bandName == "XVTR") {
            m_bank = TransverterBank;
            m_shortwaveBandsActive = false;
            rebuildBandGrid();
            emit bandBankChanged();
            return;
        }
        if (bandName == "HF") {
            m_bank = AmateurBank;
            m_shortwaveBandsActive = false;
            rebuildBandGrid();
            emit bandBankChanged();
            return;
        }

        m_selectedBand = bandName;
        updateButtonStyles();
        emit bandSelected(bandName);
        hidePopup();
    }
}

bool BandPopupWidget::isShortwaveBand(const QString &bandName) const {
    for (const auto &band : ShortwaveBands) {
        if (bandName == QLatin1String(band.label))
            return true;
    }
    return false;
}

quint64 BandPopupWidget::shortwaveBandTuneHz(const QString &bandName) const {
    for (const auto &band : ShortwaveBands) {
        if (bandName == QLatin1String(band.label)) {
            return RadioSettings::instance()->swlBandFrequency(bandName, band.centerHz);
        }
    }
    return 0;
}

void BandPopupWidget::setActiveShortwaveBand(bool vfoB, const QString &bandName) {
    if (!isShortwaveBand(bandName))
        return;
    if (vfoB)
        m_activeShortwaveBandB = bandName;
    else
        m_activeShortwaveBandA = bandName;
}

void BandPopupWidget::clearActiveShortwaveBand(bool vfoB) {
    if (vfoB)
        m_activeShortwaveBandB.clear();
    else
        m_activeShortwaveBandA.clear();
}

void BandPopupWidget::rememberVfoFrequency(bool vfoB, quint64 frequencyHz) {
    const QString &bandName = vfoB ? m_activeShortwaveBandB : m_activeShortwaveBandA;
    // A GEN selection is a local recall slot, not a K4 amateur-band stack.
    // Once selected, it must retain the actual tuned frequency (for example,
    // 27.185 MHz under the 11m entry), rather than reject it against the
    // narrower international-broadcast table used for the initial default.
    if (!bandName.isEmpty())
        RadioSettings::instance()->setSwlBandFrequency(bandName, frequencyHz);
}

int BandPopupWidget::getBandNumber(const QString &bandName) const {
    if (bandName.startsWith("XVTR")) {
        bool ok = false;
        const int slot = bandName.mid(4).toInt(&ok);
        if (ok && slot >= 1 && slot <= 12)
            return 10 + slot; // K4/QK4 transverter band numbers BN11..BN22
    }
    return BandNameToNum.value(bandName, -1); // -1 for GEN, MEM, or unknown
}

QString BandPopupWidget::getBandName(int bandNum) const {
    if (bandNum >= 11 && bandNum <= 22) {
        return QString("XVTR%1").arg(bandNum - 10);
    }
    return BandNumToName.value(bandNum, QString());
}

void BandPopupWidget::setSelectedBandByNumber(int bandNum) {
    QString bandName = getBandName(bandNum);
    if (!bandName.isEmpty()) {
        setSelectedBand(bandName);
    }
}
