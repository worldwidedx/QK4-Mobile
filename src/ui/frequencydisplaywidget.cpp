#include "frequencydisplaywidget.h"
#include "k4styles.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QTimer>
#include <QResizeEvent>

FrequencyDisplayWidget::FrequencyDisplayWidget(QWidget *parent)
    : QWidget(parent), m_digits(QString(kDigits, '0')), m_normalColor(K4Styles::Colors::TextWhite),
      m_editColor(K4Styles::Colors::VfoACyan) {

    // Set up font with tabular figures for consistent digit widths
    m_font = K4Styles::Fonts::dataFont(K4Styles::Dimensions::FontSizeFrequency);

    // Calculate character widths for click detection
    QFontMetrics fm(m_font);
    m_charWidth = fm.horizontalAdvance('0');
    m_dotWidth = fm.horizontalAdvance('.');

    // Widget configuration
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);

    // Size hint based on max display width (X.XXX.XXX.XXX = 10 digits + 3 dots).
    int width = m_charWidth * kDigits + m_dotWidth * 3 + 4; // +4 for padding
    setMinimumWidth(width);
    setFixedHeight(K4Styles::Dimensions::MenuItemHeight);
    m_holdTimer = new QTimer(this);
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(550);
    connect(m_holdTimer, &QTimer::timeout, this, [this] {
        if (m_touchTuningEnabled && isVisible() && isEnabled())
            emit directEntryRequested();
    });
}

void FrequencyDisplayWidget::setFrequency(const QString &frequency) {
    parseFrequency(frequency);
    if (m_autoFit)
        updateFontMetrics();
    update();
}

QString FrequencyDisplayWidget::frequency() const {
    return m_digits;
}

QString FrequencyDisplayWidget::displayText() const {
    return formatWithDots();
}

void FrequencyDisplayWidget::setEditModeColor(const QColor &color) {
    m_editColor = color;
    if (m_cursorPosition >= 0) {
        update();
    }
}

void FrequencyDisplayWidget::setNormalColor(const QColor &color) {
    m_normalColor = color;
    if (m_cursorPosition < 0) {
        update();
    }
}

void FrequencyDisplayWidget::setTuningRateDigit(int digitFromRight) {
    if (m_tuningRateDigit != digitFromRight) {
        m_tuningRateDigit = digitFromRight;
        update();
    }
}

bool FrequencyDisplayWidget::isEditing() const {
    return m_cursorPosition >= 0;
}

void FrequencyDisplayWidget::setAutoFit(bool enabled) {
    m_autoFit = enabled;
    if (enabled) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    updateFontMetrics();
    update();
}

void FrequencyDisplayWidget::setTouchTuningEnabled(bool enabled) {
    if (isEditing())
        exitEditMode(false);
    m_touchTuningEnabled = enabled;
}

void FrequencyDisplayWidget::setSelectedTuningDigit(int digitFromRight) {
    m_touchStepPosition = digitFromRight < 0 ? -1 : kMaxDigitIndex - qBound(0, digitFromRight, kMaxDigitIndex);
    update();
}

void FrequencyDisplayWidget::updateFontMetrics() {
    m_font = K4Styles::Fonts::dataFont(K4Styles::Dimensions::FontSizeFrequency);
    if (m_autoFit) {
        const QString text = formatWithDots();
        const int dots = text.count('.');
        const QFontMetrics metrics(m_font);
        const int textWidth = (text.size() - dots) * metrics.horizontalAdvance('0') + dots * metrics.horizontalAdvance('.');
        if (textWidth > width() - 4)
            m_font.setPixelSize(qMax(10, m_font.pixelSize() * qMax(1, width() - 4) / qMax(1, textWidth)));
    }
    const QFontMetrics metrics(m_font);
    m_charWidth = metrics.horizontalAdvance('0');
    m_dotWidth = metrics.horizontalAdvance('.');
}

void FrequencyDisplayWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_autoFit)
        updateFontMetrics();
}

bool FrequencyDisplayWidget::event(QEvent *event) {
    if (m_holdTimer && (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate ||
                        event->type() == QEvent::TouchCancel))
        m_holdTimer->stop();
    return QWidget::event(event);
}

void FrequencyDisplayWidget::mouseMoveEvent(QMouseEvent *event) {
    if ((event->pos() - m_touchPress).manhattanLength() > 10)
        m_holdTimer->stop();
    QWidget::mouseMoveEvent(event);
}

void FrequencyDisplayWidget::mouseReleaseEvent(QMouseEvent *event) {
    m_holdTimer->stop();
    QWidget::mouseReleaseEvent(event);
}

void FrequencyDisplayWidget::parseFrequency(const QString &freq) {
    // Remove dots and any non-digit characters
    QString digits;
    for (const QChar &c : freq) {
        if (c.isDigit()) {
            digits.append(c);
        }
    }

    // Pad left with zeros to kDigits, or retain the rightmost kDigits if the
    // radio ever reports a value beyond the supported display capacity.
    while (digits.length() < kDigits) {
        digits.prepend('0');
    }
    if (digits.length() > kDigits) {
        digits = digits.right(kDigits);
    }

    m_digits = digits;
}

int FrequencyDisplayWidget::displayStartIndex() const {
    // Preserve the established compact HF presentation by normally showing
    // the rightmost eight digits. Reveal additional non-zero leading digits
    // for VHF/UHF/XVTR frequencies. Editing exposes all positions.
    if (m_cursorPosition >= 0)
        return 0;

    constexpr int baseVisibleDigits = 8;
    constexpr int baseStart = kDigits - baseVisibleDigits;
    int start = 0;
    while (start < baseStart && m_digits[start] == '0')
        ++start;

    // Match the prior <10 MHz behavior: 7 MHz is "7.024.980", not
    // "07.024.980".
    if (start == baseStart && m_digits[start] == '0')
        ++start;
    return start;
}

QString FrequencyDisplayWidget::formatWithDots() const {
    // Group digits in threes from the right:
    // 7.024.980, 144.200.000, 1.296.000.000.

    QString result;
    const int startIdx = displayStartIndex();

    for (int i = startIdx; i < kDigits; ++i) {
        result.append(m_digits[i]);

        const int posFromRight = kMaxDigitIndex - i;
        if (posFromRight > 0 && posFromRight % 3 == 0)
            result.append('.');
    }

    return result;
}

int FrequencyDisplayWidget::digitIndexFromCharIndex(int charIndex) const {
    // Map character index in display string to the stored digit index.
    QString display = formatWithDots();
    if (charIndex < 0 || charIndex >= display.length()) {
        return -1;
    }

    // Count digits before this position
    int digitCount = 0;
    for (int i = 0; i < charIndex; ++i) {
        if (display[i] != '.') {
            digitCount++;
        }
    }

    // If this character is a dot, return -1
    if (display[charIndex] == '.') {
        return -1;
    }

    const int offset = displayStartIndex();
    return offset + digitCount;
}

QRect FrequencyDisplayWidget::charRectAt(int charIndex) const {
    QString display = formatWithDots();
    if (charIndex < 0 || charIndex >= display.length()) {
        return QRect();
    }

    // Calculate X position by summing widths of preceding characters
    int x = 0;
    for (int i = 0; i < charIndex; ++i) {
        x += (display[i] == '.') ? m_dotWidth : m_charWidth;
    }

    int charW = (display[charIndex] == '.') ? m_dotWidth : m_charWidth;
    return QRect(x, 0, charW, height());
}

int FrequencyDisplayWidget::digitPositionFromX(int x) const {
    QString display = formatWithDots();

    int currentX = 0;
    for (int i = 0; i < display.length(); ++i) {
        int charW = (display[i] == '.') ? m_dotWidth : m_charWidth;

        if (x >= currentX && x < currentX + charW) {
            int digitIdx = digitIndexFromCharIndex(i);
            if (digitIdx >= 0) {
                return digitIdx;
            }
            // Clicked on a dot - find nearest digit
            // Check left neighbor first
            if (i > 0) {
                int leftDigit = digitIndexFromCharIndex(i - 1);
                if (leftDigit >= 0) {
                    return leftDigit;
                }
            }
            // Check right neighbor
            if (i + 1 < display.length()) {
                int rightDigit = digitIndexFromCharIndex(i + 1);
                if (rightDigit >= 0) {
                    return rightDigit;
                }
            }
        }
        currentX += charW;
    }

    // Click beyond display - select last digit
    return kMaxDigitIndex;
}

void FrequencyDisplayWidget::enterEditMode(int digitPosition) {
    if (digitPosition < 0 || digitPosition > kMaxDigitIndex) {
        return;
    }

    m_originalDigits = m_digits;
    m_cursorPosition = digitPosition;
    setFocus();
    grabMouse(); // Capture all mouse events to detect clicks outside
    update();
}

void FrequencyDisplayWidget::exitEditMode(bool send) {
    if (m_cursorPosition < 0) {
        return; // Not in edit mode
    }

    releaseMouse(); // Release mouse grab

    if (send) {
        // Remove leading zeros for the signal (but keep at least one digit)
        QString digits = m_digits;
        while (digits.length() > 1 && digits[0] == '0') {
            digits.remove(0, 1);
        }
        emit frequencyEntered(digits);
    } else {
        // Restore original frequency
        m_digits = m_originalDigits;
        emit editingCancelled();
    }

    m_cursorPosition = -1;
    clearFocus();
    update();
}

void FrequencyDisplayWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(m_font);

    QString display = formatWithDots();

    // Draw each character
    int x = 0;
    int digitIdx = displayStartIndex();

    for (int i = 0; i < display.length(); ++i) {
        QChar c = display[i];
        int charW = (c == '.') ? m_dotWidth : m_charWidth;

        // Determine color for this character
        QColor charColor;
        if (m_cursorPosition >= 0) {
            // Edit mode: all characters in edit color
            charColor = m_editColor;
        } else if (c == '.') {
            // Dots always in normal color
            charColor = m_normalColor;
        } else {
            // Normal mode: check if this digit should be grayed (tuning rate indicator)
            const int posFromRight = kMaxDigitIndex - digitIdx;
            if (m_tuningRateDigit >= 0 && posFromRight <= m_tuningRateDigit) {
                // This digit is at or below tuning rate - show in gray
                charColor = QColor(K4Styles::Colors::TextGray);
            } else {
                charColor = m_normalColor;
            }
        }
        p.setPen(charColor);

        // Draw the character
        QRect charRect(x, 0, charW, height());
        p.drawText(charRect, Qt::AlignCenter, c);

        // Draw cursor underline if this is the selected digit (edit mode)
        if (c != '.' && (digitIdx == m_cursorPosition ||
                         (m_cursorPosition < 0 && digitIdx == m_touchStepPosition))) {
            int underlineY = height() - 4;
            p.fillRect(x + 2, underlineY, charW - 4, 2, m_editColor);
        }

        // Advance digit index (only for non-dot characters)
        if (c != '.') {
            digitIdx++;
        }

        x += charW;
    }
}

void FrequencyDisplayWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Check if click is inside widget bounds
        QRect widgetRect(0, 0, width(), height());
        bool insideWidget = widgetRect.contains(event->pos());

        if (m_cursorPosition >= 0 && !insideWidget) {
            // In edit mode but clicked outside - cancel
            exitEditMode(false);
            return;
        }

        if (insideWidget) {
            int digitPos = digitPositionFromX(event->pos().x());
            if (digitPos >= 0) {
                if (m_touchTuningEnabled) {
                    // Keep the digit selected for the touch controls or dial.
                    m_touchStepPosition = digitPos;
                    m_touchPress = event->pos();
                    m_holdTimer->start();
                    emit tuningDigitSelected(kMaxDigitIndex - digitPos);
                    update();
                } else {
                    if (m_cursorPosition < 0) {
                        enterEditMode(digitPos);
                    } else {
                        m_cursorPosition = digitPos;
                        update();
                    }
                }
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void FrequencyDisplayWidget::keyPressEvent(QKeyEvent *event) {
    if (m_cursorPosition < 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    int key = event->key();

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        // Replace digit at cursor position
        m_digits[m_cursorPosition] = QChar('0' + (key - Qt::Key_0));

        // Advance cursor (stop at end)
        if (m_cursorPosition < kMaxDigitIndex) {
            m_cursorPosition++;
        }
        update();
    } else if (key == Qt::Key_Left) {
        if (m_cursorPosition > 0) {
            m_cursorPosition--;
            update();
        }
    } else if (key == Qt::Key_Right) {
        if (m_cursorPosition < kMaxDigitIndex) {
            m_cursorPosition++;
            update();
        }
    } else if (key == Qt::Key_Home) {
        m_cursorPosition = 0;
        update();
    } else if (key == Qt::Key_End) {
        m_cursorPosition = kMaxDigitIndex;
        update();
    } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        exitEditMode(true); // Send frequency
    } else if (key == Qt::Key_Escape) {
        exitEditMode(false); // Cancel
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FrequencyDisplayWidget::wheelEvent(QWheelEvent *event) {
    if (m_cursorPosition >= 0) {
        event->ignore(); // In edit mode — don't tune
        return;
    }
    int steps = m_wheelAccumulator.accumulate(event);
    if (steps != 0)
        emit frequencyScrolled(steps);
    event->accept();
}

void FrequencyDisplayWidget::focusOutEvent(QFocusEvent *event) {
    // Cancel edit mode when focus is lost
    if (m_cursorPosition >= 0) {
        exitEditMode(false);
    }
    QWidget::focusOutEvent(event);
}
