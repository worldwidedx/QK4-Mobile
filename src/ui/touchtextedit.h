#pragma once

class QAbstractScrollArea;
class QLineEdit;

namespace TouchTextEdit {

// iOS only (no-op elsewhere). Text fields inside a QScroller-driven scroll
// area lose iOS's native edit menu and selection handles, because QScroller
// swallows the touch and replays a synthetic mouse press. Call this on the
// scroll area after grabbing its QScroller gesture: a touch that begins on a
// QLineEdit then bypasses the scroller; drags that begin elsewhere still scroll.
// Also makes a double-tap on those fields select the whole value.
void enableNativeEditMenu(QAbstractScrollArea *area);

// Double-tap selects the whole field (iOS only; no-op elsewhere).
void selectAllOnDoubleTap(QLineEdit *edit);

} // namespace TouchTextEdit
