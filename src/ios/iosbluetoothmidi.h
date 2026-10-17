#ifndef IOSBLUETOOTHMIDI_H
#define IOSBLUETOOTHMIDI_H

#include <QString>
#include <functional>

// Bluetooth LE MIDI on iOS is not discoverable from CoreMIDI directly. A
// peripheral becomes an ordinary CoreMIDI source only after the operator pairs
// it in Apple's standard Bluetooth MIDI browser, so the app presents that
// browser and then re-enumerates. Nothing else in the MIDI path changes: a
// paired CTR2 arrives through the same RtMidi enumeration as a USB one.
void iosShowBluetoothMidiPicker(std::function<void()> onDismiss);

// Returns "BLE", "USB" or "Network" for the CoreMIDI source with this port
// name, or an empty string when the transport cannot be positively identified.
// An unlabeled entry is correct; a wrong label is not.
QString iosMidiTransportForName(const QString &portName);

#endif // IOSBLUETOOTHMIDI_H
