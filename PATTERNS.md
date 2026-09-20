# Patterns

Signal/slot patterns and feature addition guides for QK4.

## Android touch and rendering rules

- Treat upstream QK4 as authoritative for radio plumbing. Preserve CAT, audio,
  and streaming techniques; adapt only presentation and touch interaction.
- Add a touch secondary action through intentional long press, never through a
  button's upper/lower hit area. Scrolling must not invoke child buttons.
- Put local panadapter behavior in the renderer/state owned by the app. Peak
  Hold and WTR CLRS are local display behavior, not substitutes for CAT state.
- Use `K4Styles::configureForScreen()` and compact dimensions rather than
  hard-coding a Galaxy-specific layout. Keep critical controls reachable.
- For controls where the state is not visible on the console, use the standard
  application notification overlay above the active popup.

## Signal/Slot Connection Patterns

### RadioState → MainWindow

```cpp
connect(m_radioState, &RadioState::frequencyChanged,
        this, &MainWindow::onFrequencyChanged);
connect(m_radioState, &RadioState::modeChanged,
        this, &MainWindow::onModeChanged);
```

### Widget → Lambda (inline handling)

```cpp
connect(m_spanUpBtn, &QPushButton::clicked, this, [this]() {
    int newSpan = m_panadapterA->span() + 1000;
    m_tcpClient->sendCAT(QString("SPAN%1;").arg(newSpan));
});
```

### Custom Widget Signals

```cpp
// In PanadapterWidget
signals:
    void frequencyClicked(qint64 frequency);
    void frequencyScrolled(int direction);

// Connection in MainWindow
connect(m_panadapterA, &PanadapterWidget::frequencyClicked,
        this, [this](qint64 freq) {
    m_tcpClient->sendCAT(QString("FA%1;").arg(freq, 11, 10, QChar('0')));
});
```

---

## Adding New Features

### Adding a New Widget

1. Create `src/<category>/<widgetname>.cpp/.h`
2. Subclass `QWidget`, implement `paintEvent()` if custom drawing
3. Add to `CMakeLists.txt` SOURCES and HEADERS
4. Include in `mainwindow.h`, create instance in `setupUi()`
5. Connect signals/slots in MainWindow constructor

**GPU-accelerated widgets:** For high-performance rendering (spectrum, waterfall), subclass `QRhiWidget` instead of `QWidget`. See `src/dsp/panadapter_rhi.cpp` for the pattern. Requires shader compilation via `qt6_add_shaders()` in CMakeLists.txt.

### Adding a New Menu Item

```cpp
// In MainWindow::setupMenuBar()
QMenu *myMenu = menuBar()->addMenu("&MyMenu");
QAction *myAction = myMenu->addAction("My Action");
connect(myAction, &QAction::triggered, this, &MainWindow::onMyAction);
```

### Adding RadioState Property

1. Add member variable and getter in `radiostate.h`
2. Add signal: `void myPropertyChanged(Type value);`
3. Add setter that emits signal on change
4. Parse in `Protocol::processCAT()` and call setter
5. Connect signal to UI update slot in MainWindow

### Adding a CAT Command

1. **models/radiostate.cpp**: Add parser case in `parseCATCommand()`
2. **models/radiostate.h**: Add getter, signal, member variable
3. **mainwindow.cpp**: Connect `RadioState::*Changed()` to UI update slot

### Adding Protocol Packet Type

1. **network/protocol.h**: Add to `PayloadType` enum
2. **network/protocol.cpp**: Add case in `processPacket()`
3. **network/protocol.h**: Add signal for new packet type
4. **mainwindow.cpp**: Connect signal to handler

---

## Adding a Popup Menu

**All popup menus MUST inherit from `K4PopupBase`.**

### K4PopupBase Class Reference

Base class for all popup widgets. Defined in `src/ui/k4popupbase.h`.

**Inherits:** QWidget

**Inherited by:** BandPopupWidget, ButtonRowPopup, DisplayPopupWidget, FnPopupWidget, ModePopupWidget

#### Public Methods

| Method | Description |
|--------|-------------|
| `K4PopupBase(QWidget *parent)` | Constructor. Sets up window flags, translucent background, focus policy |
| `showAboveButton(QWidget *trigger)` | Position and show popup centered above trigger button's parent |
| `showAboveWidget(QWidget *ref)` | Position and show popup above reference widget |
| `hidePopup()` | Hide popup and emit `closed()` signal |

#### Signals

| Signal | Description |
|--------|-------------|
| `closed()` | Emitted when popup is hidden (via hidePopup(), Escape key, or click outside) |

#### Protected Methods (for subclasses)

| Method | Description |
|--------|-------------|
| `contentSize()` | **Pure virtual.** Return QSize of content area (excluding shadow margins) |
| `contentMargins()` | Returns QMargins for layout (shadow + content padding) |
| `contentRect()` | Returns QRect of content area for custom painting |
| `initPopup()` | Sets widget size from contentSize(). **Must call at end of constructor** |
| `paintContent(QPainter&, QRect&)` | Override for custom painting after background/shadow |

#### What K4PopupBase Handles Automatically

- Window flags (`Qt::Popup | Qt::FramelessWindowHint`)
- Translucent background for shadow rendering
- Drop shadow drawing (8-layer blur via `K4Styles::drawDropShadow`)
- Popup background fill (`K4Styles::Colors::PopupBackground`)
- Escape key closes popup
- Screen boundary detection (keeps popup on-screen)
- `closed()` signal emission on hide

#### Implementation Details

```cpp
// Constructor sets up:
setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
setAttribute(Qt::WA_TranslucentBackground);
setFocusPolicy(Qt::StrongFocus);

// contentMargins() returns:
QMargins(ShadowMargin + PopupContentMargin, ...)  // All four sides

// initPopup() calculates:
totalSize = contentSize() + 2 * ShadowMargin
```

---

### Popup Creation Guide

K4PopupBase provides:
- Window flags and frameless behavior
- Drop shadow rendering (8-layer blur)
- Positioning above trigger buttons
- `closed()` signal on hide
- Escape key handling

### Template Files

| Popup Type | Template | Copy From |
|------------|----------|-----------|
| Grid with selection | `bandpopupwidget.cpp` | Multi-row buttons |
| Single row | `buttonrowpopup.cpp` | Simplest example |
| Settings panel | `displaypopupwidget.cpp` | +/- controls, toggles |
| Mode grid | `modepopupwidget.cpp` | Sub-mode logic |

### Step 1: Create Header File

```cpp
// src/ui/mypopup.h
#ifndef MYPOPUP_H
#define MYPOPUP_H

#include "k4popupbase.h"  // REQUIRED base class
#include <QPushButton>
#include <QList>

class MyPopup : public K4PopupBase {  // Inherit from K4PopupBase
    Q_OBJECT

public:
    explicit MyPopup(QWidget *parent = nullptr);

signals:
    void itemSelected(int index);
    // Note: closed() is inherited from K4PopupBase

protected:
    QSize contentSize() const override;  // REQUIRED - pure virtual

private:
    void setupUi();
    QList<QPushButton *> m_buttons;
};

#endif // MYPOPUP_H
```

### Step 2: Create Implementation File

```cpp
// src/ui/mypopup.cpp
#include "mypopup.h"
#include "k4styles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
const int ButtonWidth = 70;
const int ButtonHeight = 44;
const int ButtonSpacing = 8;
}

MyPopup::MyPopup(QWidget *parent) : K4PopupBase(parent) {
    setupUi();
}

void MyPopup::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(contentMargins());  // Use base class method
    mainLayout->setSpacing(0);

    auto *rowLayout = new QHBoxLayout();
    rowLayout->setSpacing(ButtonSpacing);

    // Create buttons using K4Styles
    for (int i = 0; i < 7; ++i) {
        auto *btn = new QPushButton(QString::number(i + 1), this);
        btn->setFixedSize(ButtonWidth, ButtonHeight);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(K4Styles::popupButtonNormal());
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            emit itemSelected(i);
            hidePopup();  // Inherited from K4PopupBase
        });
        m_buttons.append(btn);
        rowLayout->addWidget(btn);
    }
    mainLayout->addLayout(rowLayout);

    initPopup();  // REQUIRED - must be last in constructor/setupUi
}

QSize MyPopup::contentSize() const {
    // Calculate content dimensions (excluding shadow margins)
    int cm = K4Styles::Dimensions::PopupContentMargin;

    int width = 7 * ButtonWidth + 6 * ButtonSpacing + 2 * cm;
    int height = ButtonHeight + 2 * cm;
    return QSize(width, height);
}
```

### Step 3: Add to CMakeLists.txt

```cmake
# In SOURCES section:
src/ui/mypopup.cpp

# In HEADERS section:
src/ui/mypopup.h
```

### Step 4: Wire Up in MainWindow

```cpp
// mainwindow.h - Add member
MyPopup *m_myPopup;

// mainwindow.cpp - Create and connect
m_myPopup = new MyPopup(this);
connect(m_myPopup, &MyPopup::itemSelected, this, &MainWindow::onMyItemSelected);
connect(m_myPopup, &MyPopup::closed, this, [this]() {
    m_triggerButton->setStyleSheet(K4Styles::menuBarButton());  // Reset button
});

// Show popup when trigger button clicked
connect(m_triggerButton, &QPushButton::clicked, this, [this]() {
    m_myPopup->showAboveButton(m_triggerButton);
    m_triggerButton->setStyleSheet(K4Styles::menuBarButtonActive());
});
```

### Custom Painting (Optional)

If you need custom drawing beyond child widgets, override `paintContent()`:

```cpp
void MyPopup::paintContent(QPainter &painter, const QRect &contentRect) {
    // K4PopupBase already drew: shadow, background
    // Add your custom drawing here
    painter.setPen(K4Styles::borderColor());
    painter.drawLine(contentRect.left() + 10, contentRect.center().y(),
                     contentRect.right() - 10, contentRect.center().y());
}
```

### Button Selection Styling

```cpp
void MyPopup::updateButtonStyles() {
    for (auto *btn : m_buttons) {
        if (btn == m_selectedButton) {
            btn->setStyleSheet(K4Styles::popupButtonSelected());
        } else {
            btn->setStyleSheet(K4Styles::popupButtonNormal());
        }
    }
}
```

### Key Points

1. **Always inherit from `K4PopupBase`** - never `QWidget` directly
2. **Implement `contentSize()`** - returns size of content area (K4PopupBase adds shadow margins)
3. **Call `initPopup()`** - must be the last call in constructor
4. **Use `contentMargins()`** - for layout margins (handles shadow + content padding)
5. **Use `K4Styles::*`** - for all colors, gradients, and button styling
6. **Never implement shadow code** - K4PopupBase handles this
