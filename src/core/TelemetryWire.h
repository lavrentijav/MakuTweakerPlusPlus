#pragma once
#include <string>
#include <utility>
#include <vector>

namespace maku::analytics::wire {

enum class Ev : unsigned char { Launch, Launch30, Screen, Bench };
enum class Fd : unsigned char { Lang, Screen, Cpu, ScoreType, Score };

bool ChannelOpen();
void Transmit(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields);

/// Sends regardless of the analytics setting. Reserved for data the user asked
/// to share explicitly. Returns false only when the request could not be queued.
bool TransmitOnce(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields);
std::wstring ClientStoreLeaf();

} // namespace maku::analytics::wire
