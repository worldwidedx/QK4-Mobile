# UI Components Reference

Complete inventory of all Qt widgets, layouts, and UI elements in QK4.

> **Android v0.7.0 status:** This historical component inventory includes
> desktop dimensions and legacy widgets. The supported product is the compact
> landscape phone layout in `MainWindow`, with the touch Controls dialog used
> for side-panel functions. Treat `src/ui/k4styles.cpp`, `src/mainwindow.cpp`,
> and `docs/PROJECT_STATUS.md` as authoritative for the current Android UI.

---

## Table of Contents

1. [Windows and Dialogs](#windows-and-dialogs)
2. [Custom Widgets](#custom-widgets)
3. [Labels](#labels)
4. [Buttons](#buttons)
5. [Input Fields](#input-fields)
6. [Layouts](#layouts)
7. [Menus](#menus)
8. [Signal/Slot Connections](#signalslot-connections)
9. [Stylesheets](#stylesheets)

---

## Windows and Dialogs

### MainWindow

| Property | Value |
|----------|-------|
| Type | `QMainWindow` |
| File | `src/mainwindow.cpp/.h` |
| Min Size | 900 x 600 px |
| Central Widget | `QWidget` with `QVBoxLayout` |

### RadioManagerDialog

| Property | Value |
|----------|-------|
| Type | `QDialog` |
| File | `src/ui/radiomanagerdialog.cpp/.h` |
| Fixed Size | 520 x 340 px |
| Purpose | Server/radio connection management |

### OptionsDialog

| Property | Value |
|----------|-------|
| Type | `QDialog` |
| File | `src/ui/optionsdialog.cpp/.h` |
| Min Size | 700 x 550 px |
| Default Size | 800 x 650 px |
| Purpose | Application options and radio info |

**Structure:**
- Left: `QListWidget` (150px) - Vertical tab list
- Right: `QStackedWidget` - Page content

**Tabs:**
| Tab | Contents |
|-----|----------|
| About | Radio ID, Model, Installed Options, Software Versions |

**About Page Details:**
- Two-column layout: Radio Info | Installed Options
- Option Modules decoded from OM string (position-based)
- Firmware versions with human-readable component names

---

## Custom Widgets

### PanadapterRhiWidget

| Property | Value |
|----------|-------|
| Type | `QRhiWidget` subclass |
| File | `src/dsp/panadapter_rhi.cpp/.h` |
| Min Height | 200 px |
| Rendering | GPU via Qt RHI (Metal/DirectX/Vulkan) |

**Key Resources:**
```cpp
// RHI resources
QRhi *m_rhi;
std::unique_ptr<QRhiTexture> m_waterfallTexture;     // 256×2048
std::unique_ptr<QRhiTexture> m_colorLutTexture;      // 256×1 RGBA
std::unique_ptr<QRhiTexture> m_spectrumDataTexture;  // 1D spectrum

// Pipelines
std::unique_ptr<QRhiGraphicsPipeline> m_waterfallPipeline;
std::unique_ptr<QRhiGraphicsPipeline> m_spectrumBluePipeline;
std::unique_ptr<QRhiGraphicsPipeline> m_overlayLinePipeline;
```

**Signals:**
```cpp
void frequencyClicked(qint64 frequency);
void frequencyDragged(qint64 frequency);
void frequencyScrolled(int direction);
```

**Spectrum Styles:**
- `SpectrumStyle::Blue` - Y-position based blue gradient
- `SpectrumStyle::BlueAmplitude` - LUT-based colors (default)

**Constants:**
```cpp
static constexpr int BASE_WATERFALL_HISTORY = 256;
static constexpr int BASE_TEXTURE_WIDTH = 2048;
```

---

### MiniPanRhiWidget

| Property | Value |
|----------|-------|
| Type | `QRhiWidget` subclass |
| File | `src/dsp/minipan_rhi.cpp/.h` |
| Fixed Height | 90 px |
| Min Width | 180 px |
| Max Width | 200 px |
| Bandwidth | Mode-dependent: CW=3kHz (±1.5kHz), Voice/Data=10kHz (±5kHz) |
| Rendering | GPU via Qt RHI (Metal/DirectX/Vulkan) |

**Key Resources:**
```cpp
// RHI resources
std::unique_ptr<QRhiTexture> m_waterfallTexture;  // 100×512
std::unique_ptr<QRhiTexture> m_colorLutTexture;   // 256×1 RGBA

// Display settings
int m_bandwidthHz = 10000;  // Mode-dependent
float m_spectrumRatio = 0.40f;  // 40% spectrum, 60% waterfall
```

**Signals:**
```cpp
void clicked();  // Toggle back to normal view
```

**Mode-Dependent Bandwidth:**
| Mode | Bandwidth |
|------|-----------|
| CW, CW-R | 3 kHz (±1.5 kHz) |
| USB, LSB, DATA, AM, FM | 10 kHz (±5 kHz) |

**Constants:**
```cpp
static constexpr int WATERFALL_HISTORY = 100;
static constexpr int TEXTURE_WIDTH = 512;
```

**Layout Split:** 40% spectrum / 60% waterfall

---

### SideControlPanel

| Property | Value |
|----------|-------|
| Type | `QWidget` subclass |
| File | `src/ui/sidecontrolpanel.cpp/.h` |
| Fixed Width | 72 px |
| Purpose | Side control panel with dual-value buttons |

**Buttons:**
| Variable | Primary | Alternate |
|----------|---------|-----------|
| `m_wpmBtn` | WPM | PTCH |
| `m_pwrBtn` | PWR | DLY |
| `m_bwBtn` | BW | SHFT |
| `m_rfBtn` | M.RF | M.SQL |
| `m_subRfBtn` | S.RF | S.SQL |

**Icon Buttons:**
| Variable | Icon | Purpose |
|----------|------|---------|
| `m_connectBtn` | Globe | Server connection |
| `m_helpBtn` | ? | Help |

**Setter Methods:**
```cpp
void setWpm(int wpm);
void setPitch(double pitch);      // kHz format (0.50)
void setPower(int power);
void setDelay(int delay);         // ms
void setBandwidth(double bw);     // kHz format (0.40)
void setShift(double shift);      // decimal (0.50)
void setMainRfGain(int gain);
void setMainSquelch(int sql);
```

---

### DualControlButton

| Property | Value |
|----------|-------|
| Type | `QPushButton` subclass |
| File | `src/ui/dualcontrolbutton.cpp/.h` |
| Fixed Size | 64 x 48 px |
| Purpose | Button with two swappable values |

---

### BottomMenuBar

| Property | Value |
|----------|-------|
| Type | `QWidget` subclass |
| File | `src/ui/bottommenubar.cpp/.h` |
| Fixed Height | 52 px |
| Purpose | Bottom menu bar |

**Buttons:**
| Variable | Label |
|----------|-------|
| `m_menuBtn` | MENU |
| `m_fnBtn` | Fn |
| `m_displayBtn` | DISPLAY |
| `m_bandBtn` | BAND |
| `m_mainRxBtn` | MAIN RX |
| `m_subRxBtn` | SUB RX |
| `m_txBtn` | TX |

**Signals:**
```cpp
void menuClicked();
void fnClicked();
void displayClicked();
void bandClicked();
void mainRxClicked();
void subRxClicked();
void txClicked();
```

---

### FeatureMenuBar

| Property | Value |
|----------|-------|
| Type | `QWidget` subclass |
| File | `src/ui/featuremenubar.cpp/.h` |
| Purpose | Popup control bar for ATTN/NB/NR/NOTCH |

**Control Groups:**
- ATTENUATOR: Toggle, +/- in 3dB steps
- NB LEVEL: Toggle, level 0-15, filter cycling
- NR ADJUST: Toggle, level 0-10
- MANUAL NOTCH: Toggle, frequency 150-5000Hz

Supports B SET mode (commands use $ suffix for Sub RX).

---

### DisplayPopupWidget

| Property | Value |
|----------|-------|
| Type | `K4PopupBase` subclass |
| File | `src/ui/displaypopupwidget.cpp/.h` |
| Purpose | DISPLAY button popup with panadapter controls |

**Control Groups:**
- REF LEVEL, SPAN, TUNE mode, VFO CURSOR
- AVERAGE, DDC NB, PEAK, FREEZE, WATERFALL color

---

### ButtonRowPopup

| Property | Value |
|----------|-------|
| Type | `K4PopupBase` subclass |
| File | `src/ui/buttonrowpopup.cpp/.h` |
| Purpose | Single-row popup for Fn/MAIN RX/SUB RX/TX |

7 buttons for MAIN RX/SUB RX/TX menu functions.

---

### Volume Sliders (in SideControlPanel)

| Property | Value |
|----------|-------|
| MAIN Volume | Cyan (#00BFFF), left audio channel |
| SUB Volume | Green (#00FF00), right audio channel |
| Range | 0-100 |
| Default | 45% |

Both sliders persist values via RadioSettings.

---

## Labels

### Top Status Bar

| Variable | Text | Color | Size |
|----------|------|-------|------|
| `m_titleLabel` | "Elecraft K4 - {callsign}" | White | 14px bold |
| `m_dateTimeLabel` | "MM-DD / HH:MM:SS Z" | Gray | 12px |
| `m_powerLabel` | "{watts} W" | Amber | 12px |
| `m_swrLabel` | "{swr}:1" | Amber | 12px |
| `m_voltageLabel` | "{volts} V" | Amber | 12px |
| `m_currentLabel` | "{amps} A" | Amber | 12px |
| `m_connectionStatusLabel` | "Disconnected/Connecting/Connected" | Gray→Amber→Green | 12px |

### VFO A Section

| Variable | Purpose | Color | Size |
|----------|---------|-------|------|
| `m_frequencyALabel` | Frequency display | White | 32px bold mono |
| `m_modeALabel` | Mode (USB/LSB/CW/etc) | White | 11px bold |
| `m_agcALabel` | AGC status | Gray/White | 11px |
| `m_preampALabel` | Preamp status | Gray/White | 11px |
| `m_attALabel` | Attenuator status | Gray/White | 11px |
| `m_nbALabel` | Noise blanker | Gray/White | 11px |
| `m_nrALabel` | Noise reduction | Gray/White | 11px |

### VFO B Section

| Variable | Purpose | Color | Size |
|----------|---------|-------|------|
| `m_frequencyBLabel` | Frequency display | White | 32px bold mono |
| `m_modeBLabel` | Mode | White | 11px bold |
| `m_agcBLabel` | AGC status | Gray/White | 11px |
| `m_preampBLabel` | Preamp status | Gray/White | 11px |
| `m_attBLabel` | Attenuator status | Gray/White | 11px |
| `m_nbBLabel` | Noise blanker | Gray/White | 11px |
| `m_nrBLabel` | Noise reduction | Gray/White | 11px |

### Center Section

| Variable | Purpose | Color | Size |
|----------|---------|-------|------|
| `m_vfoASquare` | "A" indicator | Cyan bg | 16px, 45x45 |
| `m_vfoBSquare` | "B" indicator | Green bg | 16px, 45x45 |
| `m_txTriangle` | "◀" (points to A) | Amber | 18px |
| `m_txTriangleB` | "▶" (points to B, split) | Amber | 18px |
| `m_txIndicator` | "TX" | Amber | 18px bold |
| `m_splitLabel` | "SPLIT OFF/ON" | Amber/Green | 11px |
| `m_msgBankLabel` | "MSG: I/II" | Gray | 11px |
| `m_ritLabel` | "RIT" | Gray/White | 10px |
| `m_xitLabel` | "XIT" | Gray/White | 10px |
| `m_ritXitValueLabel` | "+0.00" | White | 14px bold |

### Antenna Row

| Variable | Purpose | Alignment | Color |
|----------|---------|-----------|-------|
| `m_rxAntALabel` | "1:ANT1" (RX A) | Left | White |
| `m_txAntennaLabel` | "1:ANT1" (TX) | Center | Amber |
| `m_rxAntBLabel` | "1:ANT1" (RX B) | Right | White |

---

## Buttons

### MainWindow

| Variable | Text | Size | Purpose |
|----------|------|------|---------|
| `m_spanDownBtn` | "-" | 28x24 | Decrease span |
| `m_spanUpBtn` | "+" | 28x24 | Increase span |

### RadioManagerDialog

| Variable | Text | Purpose |
|----------|------|---------|
| `m_connectButton` | "Connect" | Connect to selected radio |
| `m_newButton` | "New" | Create new server entry |
| `m_saveButton` | "Save" | Save server settings |
| `m_deleteButton` | "Delete" | Delete selected server |
| `m_backButton` | "←" | Close dialog |
| `m_tlsCheckbox` | "Use TLS (Encrypted)" | Toggle TLS mode |
| `m_pskEdit` | PSK field | Pre-Shared Key (visible when TLS enabled) |

---

## Input Fields

### RadioManagerDialog

| Variable | Placeholder | Purpose |
|----------|-------------|---------|
| `m_nameEdit` | "Server Name" | Server display name |
| `m_hostEdit` | "192.168.1.100" | Host IP address |
| `m_portEdit` | "9204" or "9205" | TCP port (auto-switches with TLS) |
| `m_passwordEdit` | "Password" | Auth password (echo: Password) |
| `m_pskEdit` | "Pre-Shared Key" | TLS PSK (visible only when TLS enabled) |

### List Widgets

| Variable | Type | Purpose |
|----------|------|---------|
| `m_radioList` | `QListWidget` | Available servers (180-200px wide) |

---

## Layouts

### Main Window Layout Hierarchy

```
MainWindow (QMainWindow)
└── centralWidget (QWidget)
    └── mainLayout (QVBoxLayout)
        │
        ├── Top Status Bar (QWidget, 28px fixed height)
        │   └── QHBoxLayout
        │       ├── m_titleLabel
        │       ├── m_dateTimeLabel
        │       ├── [stretch]
        │       ├── m_powerLabel
        │       ├── m_swrLabel
        │       ├── m_voltageLabel
        │       ├── m_currentLabel
        │       └── m_connectionStatusLabel
        │
        ├── VFO Section (QWidget)
        │   └── QVBoxLayout
        │       │
        │       ├── VFO Row (QWidget)
        │       │   └── QHBoxLayout
        │       │       │
        │       │       ├── VFO A Widget (stretch: 1)
        │       │       │   └── QVBoxLayout
        │       │       │       ├── Frequency Row (QHBoxLayout)
        │       │       │       │   └── m_frequencyALabel
        │       │       │       └── m_vfoAStackedWidget (90px fixed height)
        │       │       │           ├── [0] Normal Content
        │       │       │           │   └── QVBoxLayout
        │       │       │           │       ├── m_sMeterA
        │       │       │           │       └── Features Row (QHBoxLayout)
        │       │       │           │           └── AGC, PRE, ATT, NB, NR
        │       │       │           └── [1] m_miniPanA
        │       │       │
        │       │       ├── Center Widget (fixed width ~200px)
        │       │       │   └── QVBoxLayout
        │       │       │       ├── TX Row (QHBoxLayout)
        │       │       │       │   ├── VFO A Column (m_vfoASquare, m_modeALabel)
        │       │       │       │   ├── m_txTriangle
        │       │       │       │   ├── m_txIndicator
        │       │       │       │   ├── m_txTriangleB
        │       │       │       │   └── VFO B Column (m_vfoBSquare, m_modeBLabel)
        │       │       │       ├── m_splitLabel
        │       │       │       ├── m_msgBankLabel
        │       │       │       └── RIT/XIT Box
        │       │       │           └── QVBoxLayout
        │       │       │               ├── Labels Row (RIT, XIT)
        │       │       │               └── m_ritXitValueLabel
        │       │       │
        │       │       └── VFO B Widget (stretch: 1)
        │       │           └── QVBoxLayout
        │       │               ├── Frequency Row
        │       │               │   └── m_frequencyBLabel
        │       │               ├── m_sMeterB
        │       │               └── Features Row (AGC, PRE, ATT, NB, NR)
        │       │
        │       └── Antenna Row (QHBoxLayout)
        │           ├── m_rxAntALabel
        │           ├── [stretch]
        │           ├── m_txAntennaLabel
        │           ├── [stretch]
        │           └── m_rxAntBLabel
        │
        └── Spectrum Container (QWidget, min height 300px)
            └── QVBoxLayout
                └── m_panadapterA
                    ├── m_spanDownBtn (overlay, bottom-right)
                    └── m_spanUpBtn (overlay, bottom-right)
```

### RadioManagerDialog Layout

```
QDialog
└── mainLayout (QVBoxLayout)
    │
    ├── topLayout (QHBoxLayout)
    │   ├── Left Section (QVBoxLayout)
    │   │   ├── "Available Servers" label
    │   │   └── m_radioList
    │   │
    │   ├── Right Section (QVBoxLayout)
    │   │   ├── "Edit Connect" label
    │   │   └── Form Grid (QGridLayout)
    │   │       ├── Row 0: Name label + m_nameEdit
    │   │       ├── Row 1: Host label + m_hostEdit
    │   │       ├── Row 2: Port label + m_portEdit
    │   │       ├── Row 3: Password label + m_passwordEdit
    │   │       ├── Row 4: m_tlsCheckbox ("Use TLS (Encrypted)")
    │   │       └── Row 5: PSK label + m_pskEdit (visible when TLS enabled)
    │   │
    │   └── [stretch]
    │
    └── buttonLayout (QHBoxLayout)
        ├── m_connectButton
        ├── m_newButton
        ├── m_saveButton
        ├── m_deleteButton
        └── m_backButton
```

---

## Menus

### MenuBar Structure

```
File (placeholder)
├── (empty)

Connect
├── Radios...  → showRadioManager()

Tools (placeholder)
├── (empty)

View (placeholder)
├── (empty)
```

### Menu Styling

```cpp
menuBar()->setStyleSheet(
    "QMenuBar { background-color: #0d0d0d; color: #FFFFFF; }"
    "QMenuBar::item:selected { background-color: #333333; }"
    "QMenu { background-color: #0d0d0d; color: #FFFFFF; }"
    "QMenu::item:selected { background-color: #333333; }"
);
```

---

## Signal/Slot Connections

### Network → UI

```cpp
// TcpClient signals
TcpClient::stateChanged      → MainWindow::onStateChanged
TcpClient::errorOccurred     → MainWindow::onError
TcpClient::authenticated     → MainWindow::onAuthenticated
TcpClient::authenticationFailed → MainWindow::onAuthenticationFailed

// Protocol signals
Protocol::catResponseReceived    → MainWindow::onCatResponse
Protocol::spectrumDataReady      → MainWindow::onSpectrumData
Protocol::miniSpectrumDataReady  → MainWindow::onMiniSpectrumData
Protocol::audioDataReady         → MainWindow::onAudioData
```

### RadioState → UI

```cpp
// VFO A
RadioState::frequencyChanged      → MainWindow::onFrequencyChanged
RadioState::modeChanged           → MainWindow::onModeChanged
RadioState::sMeterChanged         → MainWindow::onSMeterChanged
RadioState::filterBandwidthChanged → MainWindow::onBandwidthChanged
RadioState::processingChanged     → MainWindow::onProcessingChanged

// VFO B
RadioState::frequencyBChanged     → MainWindow::onFrequencyBChanged
RadioState::modeBChanged          → MainWindow::onModeBChanged
RadioState::sMeterBChanged        → MainWindow::onSMeterBChanged
RadioState::filterBandwidthBChanged → MainWindow::onBandwidthBChanged
RadioState::processingChangedB    → MainWindow::onProcessingChangedB

// Status
RadioState::rfPowerChanged        → MainWindow::onRfPowerChanged
RadioState::supplyVoltageChanged  → MainWindow::onSupplyVoltageChanged
RadioState::supplyCurrentChanged  → MainWindow::onSupplyCurrentChanged
RadioState::swrChanged            → MainWindow::onSwrChanged
RadioState::splitChanged          → MainWindow::onSplitChanged
RadioState::antennaChanged        → MainWindow::onAntennaChanged
RadioState::ritXitChanged         → MainWindow::onRitXitChanged
RadioState::messageBankChanged    → MainWindow::onMessageBankChanged

// Panadapter
RadioState::refLevelChanged       → [lambda] m_panadapterA->setRefLevel
RadioState::spanChanged           → [lambda] m_panadapterA->setSpan
```

### Widget Interactions

```cpp
// Span buttons
m_spanDownBtn::clicked → [lambda] Decrease span, send SPAN CAT
m_spanUpBtn::clicked   → [lambda] Increase span, send SPAN CAT

// Panadapter
PanadapterWidget::frequencyClicked  → [lambda] Send FA CAT command
PanadapterWidget::frequencyScrolled → [lambda] Send UP/DN CAT commands

// Mini-Pan toggle
MiniPanWidget::clicked              → [lambda] m_vfoAStackedWidget->setCurrentIndex(0)
m_normalVfoAContent mousePress      → [lambda] m_vfoAStackedWidget->setCurrentIndex(1)

// RadioManagerDialog
m_connectButton::clicked → RadioManagerDialog::onConnectClicked
m_newButton::clicked     → RadioManagerDialog::onNewClicked
m_saveButton::clicked    → RadioManagerDialog::onSaveClicked
m_deleteButton::clicked  → RadioManagerDialog::onDeleteClicked
RadioManagerDialog::connectRequested → MainWindow::connectToRadio
```

---

## Stylesheets

> **DEPRECATED:** Use `K4Styles` functions from `src/ui/k4styles.h` instead of inline CSS.
> See `PATTERNS.md` → "Adding a Popup Menu" for current patterns.

### Common Button Style (Legacy Reference)

```css
QPushButton {
    background-color: #5a5a5a;
    color: #ffffff;
    border: none;
    border-radius: 4px;
    padding: 8px 16px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #707070;
}
```

### Input Field Style

```css
QLineEdit {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #555555;
    border-radius: 4px;
    padding: 6px;
}
```

### List Widget Style

```css
QListWidget {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #555555;
    border-radius: 4px;
    padding: 4px;
}
QListWidget::item {
    padding: 6px;
}
QListWidget::item:selected {
    background-color: #0078d4;
    color: #ffffff;
}
```

### Dialog Style

```css
QDialog {
    background-color: #2b2b2b;
}
```

---

## Event Handling

### MainWindow::eventFilter

Handles:
- `QEvent::Resize` on `m_panadapterA`: Repositions span buttons
- `QEvent::MouseButtonPress` on `m_normalVfoAContent`: Toggles to Mini-Pan

### Mouse Events in Widgets

| Widget | Event | Action |
|--------|-------|--------|
| PanadapterWidget | mousePressEvent | Click-to-tune frequency |
| PanadapterWidget | mouseMoveEvent | Drag frequency |
| PanadapterWidget | wheelEvent | Scroll frequency adjust |
| MiniPanWidget | mousePressEvent | Toggle view |

---

## Font Specifications

| Usage | Family | Size | Weight |
|-------|--------|------|--------|
| Frequency display | Inter (tabular figures) | 32px | Bold |
| Title | Inter | 14px | Bold |
| Mode labels | Inter | 11px | Bold |
| Status labels | Inter | 11-12px | Normal |
| Secondary text | Inter | 10-11px | Normal |

---

*Last updated: February 10, 2026*
