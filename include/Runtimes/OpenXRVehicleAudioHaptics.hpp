#pragma once

namespace cvr::haptics {

// Captures the Windows default render endpoint in loopback mode and converts its low-frequency
// vehicle audio/transients into a small per-hand OpenXR amplitude envelope. The worker is started
// only for non-PSVR2 systems; PSVR2 keeps using the Toolkit bridge's richer PCM path.
void StartVehicleAudioCapture();
void StopVehicleAudioCapture();

} // namespace cvr::haptics
