# K4Styles Reference

Complete reference for all `K4Styles::Colors` and `K4Styles::Dimensions` constants.

**Source:** `src/ui/k4styles.h`

---

## Colors (K4Styles::Colors::*)

### Backgrounds

| Constant | Value | Usage |
|----------|-------|-------|
| `Background` | `#1a1a1a` | Main app background |
| `DarkBackground` | `#0d0d0d` | Darker areas, panel backgrounds |
| `PopupBackground` | `#1e1e1e` | Popup menu fill |
| `K4Styles::Colors::DisabledBackground` | `#444444` | Disabled indicator backgrounds (SUB/DIV off state) |

### App Accent Color

| Constant | Value | Usage |
|----------|-------|-------|
| `AccentAmber` | `#FFB000` | TX indicators, labels, highlights |

### VFO Theme Colors

| Constant | Value | Usage |
|----------|-------|-------|
| `VfoACyan` | `#00BFFF` | VFO A: square, passband, markers, overlays |
| `VfoBGreen` | `#00FF00` | VFO B: square, passband, markers, overlays |

### Status Indicator Colors

| Constant | Value | Usage |
|----------|-------|-------|
| `TxRed` | `#FF0000` | TX indicator, transmit state |
| `StatusGreen` | `#00FF00` | AGC indicator |

### Text Colors

| Constant | Value | Usage |
|----------|-------|-------|
| `TextWhite` | `#FFFFFF` | Primary text, button labels |
| `TextDark` | `#333333` | Text on light/selected buttons |
| `TextGray` | `#999999` | Secondary text, labels |
| `TextFaded` | `#808080` | Faded text (e.g., auto mode values) |
| `InactiveGray` | `#666666` | Inactive/disabled elements |

### Button Gradient - Normal State

| Constant | Value | Position |
|----------|-------|----------|
| `K4Styles::Colors::GradientTop` | `#4a4a4a` | 0% (top) |
| `K4Styles::Colors::GradientMid1` | `#3a3a3a` | 33% |
| `K4Styles::Colors::GradientMid2` | `#353535` | 66% |
| `K4Styles::Colors::GradientBottom` | `#2a2a2a` | 100% (bottom) |

### Button Gradient - Hover State

| Constant | Value | Position |
|----------|-------|----------|
| `K4Styles::Colors::HoverTop` | `#5a5a5a` | 0% (top) |
| `K4Styles::Colors::HoverMid1` | `#4a4a4a` | 33% |
| `K4Styles::Colors::HoverMid2` | `#454545` | 66% |
| `K4Styles::Colors::HoverBottom` | `#3a3a3a` | 100% (bottom) |

### Button Gradient - Light (TX function buttons)

| Constant | Value | Position |
|----------|-------|----------|
| `K4Styles::Colors::LightGradientTop` | `#888888` | 0% (top) |
| `K4Styles::Colors::LightGradientMid1` | `#777777` | 33% |
| `K4Styles::Colors::LightGradientMid2` | `#6a6a6a` | 66% |
| `K4Styles::Colors::LightGradientBottom` | `#606060` | 100% (bottom) |

### Button Gradient - Selected State

| Constant | Value | Position |
|----------|-------|----------|
| `K4Styles::Colors::SelectedTop` | `#E0E0E0` | 0% (top) |
| `K4Styles::Colors::SelectedMid1` | `#D0D0D0` | 33% |
| `K4Styles::Colors::SelectedMid2` | `#C8C8C8` | 66% |
| `K4Styles::Colors::SelectedBottom` | `#B8B8B8` | 100% (bottom) |

### Border Colors

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Colors::BorderNormal` | `#606060` | Default button border |
| `K4Styles::Colors::BorderHover` | `#808080` | Button border on hover |
| `K4Styles::Colors::BorderPressed` | `#909090` | Button border when pressed |
| `K4Styles::Colors::BorderSelected` | `#AAAAAA` | Selected/active button border |
| `K4Styles::Colors::BorderLight` | `#909090` | Light button border |

### Meter Gradient Colors (S/Po Meter)

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Colors::MeterGreenDark` | `#00CC00` | Low signal (green start) |
| `K4Styles::Colors::MeterGreen` | `#00FF00` | Normal signal range |
| `K4Styles::Colors::MeterYellowGreen` | `#CCFF00` | Transition to yellow |
| `K4Styles::Colors::MeterYellow` | `#FFFF00` | Moderate signal |
| `K4Styles::Colors::MeterOrange` | `#FF6600` | High signal |
| `K4Styles::Colors::MeterOrangeRed` | `#FF3300` | Approaching max |
| `K4Styles::Colors::MeterRed` | `#FF0000` | Peak/overload signal |

### PA Drain Current Meter Colors

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Colors::MeterIdDark` | `#00AAFF` | Id meter low range |
| `K4Styles::Colors::MeterIdLight` | `#66CCFF` | Id meter high range |

### Overlay Panel Colors (Menu/Macro Dialogs)

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Colors::OverlayBackground` | `#1A1A1F` | Full-screen overlay background |
| `K4Styles::Colors::OverlayContentBg` | `#18181C` | Content area background |
| `K4Styles::Colors::OverlayHeaderBg` | `#222228` | Header row background |
| `K4Styles::Colors::OverlayColumnHeaderBg` | `#1E1E24` | Column header background |
| `K4Styles::Colors::OverlayItemBg` | `#19191E` | List item background |
| `K4Styles::Colors::OverlayNavButton` | `#3A3A45` | Navigation button normal |
| `K4Styles::Colors::OverlayNavButtonPressed` | `#505060` | Navigation button pressed |
| `K4Styles::Colors::OverlayDivider` | `#28282D` | Section dividers |
| `K4Styles::Colors::OverlayDividerLight` | `#3C3C41` | Lighter dividers |

### Selection Colors (K4-style Dual-Panel)

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Colors::SelectionLight` | `#DCDCDC` | Selected item text/highlight (light) |
| `K4Styles::Colors::SelectionDark` | `#505055` | Selected item background (dark) |

---

## Dimensions (K4Styles::Dimensions::*)

### Border & Radius

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::BorderWidth` | `2` | Standard button border width (px) |
| `K4Styles::Dimensions::BorderRadius` | `6` | Standard button corner radius (px) |
| `K4Styles::Dimensions::BorderRadiusLarge` | `8` | Large button/popup corner radius (px) |

### Shadow (Popup Widgets)

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::ShadowRadius` | `16` | Shadow blur radius (px) |
| `K4Styles::Dimensions::ShadowOffsetX` | `2` | Shadow horizontal offset (px) |
| `K4Styles::Dimensions::ShadowOffsetY` | `4` | Shadow vertical offset (px) |
| `K4Styles::Dimensions::ShadowMargin` | `20` | Space around popup for shadow (`ShadowRadius + 4`) |
| `K4Styles::Dimensions::ShadowLayers` | `8` | Number of blur layers for soft shadow |

### Button Heights

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::ButtonHeightLarge` | `44` | Menu overlay nav buttons (px) |
| `K4Styles::Dimensions::ButtonHeightMedium` | `36` | Bottom menu bar, popup buttons (px) |
| `K4Styles::Dimensions::ButtonHeightSmall` | `28` | Function buttons in side panels (px) |
| `K4Styles::Dimensions::ButtonHeightMini` | `24` | Compact toggle buttons (MON, NORM, BAL, etc.) (px) |

### Popup Layout

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::PopupButtonWidth` | `70` | Standard popup button width (px) |
| `K4Styles::Dimensions::PopupButtonHeight` | `44` | Standard popup button height (px) |
| `K4Styles::Dimensions::PopupButtonSpacing` | `8` | Spacing between popup buttons (px) |
| `K4Styles::Dimensions::PopupContentMargin` | `12` | Padding inside popup content area (px) |

### Common UI Heights

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::SeparatorHeight` | `1` | Horizontal/vertical separator lines (px) |
| `K4Styles::Dimensions::MenuItemHeight` | `40` | Menu overlay items, frequency labels (px) |

### Common UI Widths

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::FormLabelWidth` | `80` | Form field labels in dialogs (px) |
| `K4Styles::Dimensions::VfoSquareSize` | `45` | VFO A/B indicator squares and mode labels (px) |
| `K4Styles::Dimensions::NavButtonWidth` | `54` | Navigation buttons in overlays (px) |

### Font Sizes

All font sizes are in **pixels** — use with `QFont::setPixelSize()` or `K4Styles::Fonts::paintFont()`.
This ensures consistent rendering across macOS (72 PPI) and Windows (96 PPI).

| Constant | Value | Usage |
|----------|-------|-------|
| `K4Styles::Dimensions::FontSizeTiny` | `7` | Sub-labels (BANK, AF REC, MESSAGE) (px) |
| `K4Styles::Dimensions::FontSizeSmall` | `8` | TX function sub-labels (px) |
| `K4Styles::Dimensions::FontSizeNormal` | `9` | Memory buttons (px) |
| `K4Styles::Dimensions::FontSizeMedium` | `10` | Volume labels (px) |
| `K4Styles::Dimensions::FontSizeLarge` | `11` | Feature labels, time/power display (px) |
| `K4Styles::Dimensions::FontSizeButton` | `12` | Menu bar buttons (px) |
| `K4Styles::Dimensions::FontSizePopup` | `14` | Popup buttons (px) |
| `K4Styles::Dimensions::FontSizeTitle` | `16` | Large control buttons (+/-) (px) |
| `K4Styles::Dimensions::FontSizeFrequency` | `32` | VFO frequency displays (px) |

---

## Helper Functions

### Stylesheet Functions

```cpp
K4Styles::popupButtonNormal()      // Dark gradient button stylesheet
K4Styles::popupButtonSelected()    // Light/white selected button stylesheet
K4Styles::menuBarButton()          // Bottom menu bar button stylesheet
K4Styles::menuBarButtonActive()    // Active/inverted menu button stylesheet
K4Styles::menuBarButtonSmall()     // Compact +/- button stylesheet
```

### QPainter Functions

```cpp
// Fonts (pixel-based for cross-platform consistency)
QFont K4Styles::Fonts::paintFont(int pixelSize, QFont::Weight weight = QFont::Bold);
QFont K4Styles::Fonts::dataFont(int pixelSize, QFont::Weight weight = QFont::Bold);

// Gradients
QLinearGradient K4Styles::buttonGradient(int top, int bottom, bool hovered = false);
QLinearGradient K4Styles::selectedGradient(int top, int bottom);
QLinearGradient K4Styles::meterGradient(float x1, float y1, float x2, float y2);

// Colors
QColor K4Styles::borderColor();         // Returns BorderNormal color
QColor K4Styles::borderColorSelected(); // Returns BorderSelected color

// Shadow
void K4Styles::drawDropShadow(QPainter &painter, const QRect &contentRect, int cornerRadius = 8);
```

---

## Usage Examples

### Stylesheet-based Button

```cpp
auto *btn = new QPushButton("BAND", this);
btn->setStyleSheet(K4Styles::popupButtonNormal());

// On selection:
btn->setStyleSheet(K4Styles::popupButtonSelected());
```

### Custom-painted Widget

```cpp
void MyWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    // Draw gradient background
    QLinearGradient grad = K4Styles::buttonGradient(0, height(), m_hovered);
    painter.fillRect(rect(), grad);

    // Draw border
    painter.setPen(QPen(K4Styles::borderColor(), K4Styles::Dimensions::BorderWidth));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                           K4Styles::Dimensions::BorderRadius,
                           K4Styles::Dimensions::BorderRadius);

    // Draw text
    painter.setPen(QColor(K4Styles::Colors::TextWhite));
    painter.drawText(rect(), Qt::AlignCenter, "Label");
}
```

### Popup with Shadow

```cpp
void MyPopup::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect content = contentRect();  // From K4PopupBase

    // Draw shadow (K4PopupBase does this automatically)
    K4Styles::drawDropShadow(painter, content, K4Styles::Dimensions::BorderRadiusLarge);

    // Draw popup background
    painter.setBrush(QColor(K4Styles::Colors::PopupBackground));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(content,
                           K4Styles::Dimensions::BorderRadiusLarge,
                           K4Styles::Dimensions::BorderRadiusLarge);
}
```

---

## Color Palette Visual Reference

```
Backgrounds:
  #1a1a1a ████ Background
  #0d0d0d ████ DarkBackground
  #1e1e1e ████ PopupBackground
  #444444 ████ DisabledBackground

Accent & VFO Colors:
  #FFB000 ████ AccentAmber (TX indicators, highlights)
  #00BFFF ████ VfoACyan (VFO A theme)
  #00FF00 ████ VfoBGreen (VFO B theme)

Status:
  #FF0000 ████ TxRed
  #00FF00 ████ StatusGreen

Text:
  #FFFFFF ████ TextWhite
  #333333 ████ TextDark
  #999999 ████ TextGray
  #808080 ████ TextFaded
  #666666 ████ InactiveGray

Normal Gradient (top→bottom):
  #4a4a4a ████ GradientTop
  #3a3a3a ████ GradientMid1
  #353535 ████ GradientMid2
  #2a2a2a ████ GradientBottom

Selected Gradient (top→bottom):
  #E0E0E0 ████ SelectedTop
  #D0D0D0 ████ SelectedMid1
  #C8C8C8 ████ SelectedMid2
  #B8B8B8 ████ SelectedBottom

Borders:
  #606060 ████ BorderNormal
  #808080 ████ BorderHover
  #909090 ████ BorderPressed/BorderLight
  #AAAAAA ████ BorderSelected

Meter Gradient (green→yellow→red):
  #00CC00 ████ MeterGreenDark
  #00FF00 ████ MeterGreen
  #CCFF00 ████ MeterYellowGreen
  #FFFF00 ████ MeterYellow
  #FF6600 ████ MeterOrange
  #FF3300 ████ MeterOrangeRed
  #FF0000 ████ MeterRed

PA Drain Meter:
  #00AAFF ████ MeterIdDark
  #66CCFF ████ MeterIdLight

Overlay Panels:
  #1A1A1F ████ OverlayBackground
  #18181C ████ OverlayContentBg
  #222228 ████ OverlayHeaderBg
  #1E1E24 ████ OverlayColumnHeaderBg
  #19191E ████ OverlayItemBg
  #3A3A45 ████ OverlayNavButton
  #505060 ████ OverlayNavButtonPressed
  #28282D ████ OverlayDivider
  #3C3C41 ████ OverlayDividerLight

Selection:
  #DCDCDC ████ SelectionLight
  #505055 ████ SelectionDark
```
