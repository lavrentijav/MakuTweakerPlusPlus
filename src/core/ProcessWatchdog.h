#pragma once

namespace maku::watchdog {

void Start();
void Stop();
void SetEnabled(bool enabled);
bool IsEnabled();

} // namespace maku::watchdog
