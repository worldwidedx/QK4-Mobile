#ifndef K4POPUPBASE_H
#define K4POPUPBASE_H

#include <QWidget>

/**
 * @brief Base class for QK4 popup widgets.
 *
 * Provides centralized handling of:
 * - Window flags and translucent background setup
 * - Popup positioning above trigger buttons
 * - Drop shadow rendering
 * - Hide/close behavior with closed() signal
 * - Escape key handling
 *
 * Subclasses must implement:
 * - contentSize(): Return the size of the content area (buttons, controls)
 *
 * Subclasses should:
 * - Call initPopup() at the end of their constructor
 * - Use contentMargins() when setting up their layout
 * - Override paintContent() if they need custom painting beyond child widgets
 *
 * Example usage in subclass constructor:
 * @code
 * MyPopup::MyPopup(QWidget* parent) : K4PopupBase(parent) {
 *     auto* layout = new QVBoxLayout(this);
 *     layout->setContentsMargins(contentMargins());
 *     // ... add widgets to layout ...
 *     initPopup();  // Must be called last
 * }
 * @endcode
 */
class K4PopupBase : public QWidget {
    Q_OBJECT

public:
    explicit K4PopupBase(QWidget *parent = nullptr);

    /**
     * @brief Position and show the popup above the trigger button.
     *
     * Centers the popup above the trigger button's parent widget (typically
     * the menu bar).
     *
     * @param triggerButton The button that triggered the popup
     */
    void showAboveButton(QWidget *triggerButton);

    /**
     * @brief Position and show the popup above a reference widget.
     *
     * @param referenceWidget Widget to position the popup above
     */
    void showAboveWidget(QWidget *referenceWidget);

    /**
     * @brief Hide the popup and emit closed() signal.
     */
    void hidePopup();

signals:
    /**
     * @brief Emitted when the popup is hidden.
     */
    void closed();

protected:
    /**
     * @brief Return the content size (width, height) excluding shadow margins.
     *
     * Subclasses must implement this to specify their content dimensions.
     * The content size should include:
     * - Button grid/row dimensions
     * - Content margins (typically PopupContentMargin on each side)
     *
     * @return Size of the content area
     */
    virtual QSize contentSize() const = 0;

    /**
     * @brief Get the margins to use for the main layout.
     *
     * Returns margins that account for shadow space and content padding.
     * Use this when setting up the layout in the subclass constructor.
     *
     * @return QMargins for the layout's setContentsMargins()
     */
    QMargins contentMargins() const;

    /**
     * @brief Initialize the popup after subclass setup.
     *
     * Must be called at the end of the subclass constructor after
     * setting up the layout and child widgets. This calculates and
     * sets the final widget size.
     */
    void initPopup();

    /**
     * @brief Get the content rectangle (inside shadow margins).
     *
     * Useful for custom painting in subclasses.
     *
     * @return Rectangle for the content area
     */
    QRect contentRect() const;

    /**
     * @brief Override to perform custom painting after background/shadow.
     *
     * Called by paintEvent() after drawing the shadow and background.
     * Default implementation does nothing.
     *
     * @param painter The painter to use
     * @param contentRect The content rectangle (inside shadow margins)
     */
    virtual void paintContent(QPainter &painter, const QRect &contentRect);

    // Standard Qt event handlers
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // K4POPUPBASE_H
