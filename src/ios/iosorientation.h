#ifndef IOSORIENTATION_H
#define IOSORIENTATION_H

namespace IosOrientation {
void requestLandscape();
void requestPortrait();
void allowPortraitAndLandscape();
}

namespace IosWindow {
// Height in points to keep clear at the top of the window for the iPadOS
// window controls. Zero when the window fills the screen.
int controlsTopInset();
}

#endif // IOSORIENTATION_H
