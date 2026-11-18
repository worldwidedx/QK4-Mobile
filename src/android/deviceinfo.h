#ifndef ANDROIDDEVICEINFO_H
#define ANDROIDDEVICEINFO_H

namespace AndroidDeviceInfo {
// Capability, not current posture: remains true on either foldable display.
// False also covers devices that do not expose the capability to applications.
bool hasFoldingHardware();
}

#endif
