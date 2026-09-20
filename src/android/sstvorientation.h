#ifndef SSTVORIENTATION_H
#define SSTVORIENTATION_H

#include <QtGlobal>

// This helper is deliberately independent of the SSTV codec/controller so
// host tests and non-Android builds remain free of JNI dependencies.
void setSstvOrientationEnabled(bool enabled);
void setFt8PortraitEnabled(bool enabled);
void setRadioLogbookOrientationEnabled(bool enabled);

#endif // SSTVORIENTATION_H
