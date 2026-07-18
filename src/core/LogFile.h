#pragma once

#include <string>

namespace maku::logf {

/// %AppData%\MakuTweaker++\logs\MakuTweaker.log — called once at app start if not already init.
void InitDefault();
void Init(const std::wstring& path);
std::wstring Path();
bool Active();

void Write(const std::string& line);
void Info(const std::string& line);
void Warn(const std::string& line);
void Error(const std::string& line);
void InfoWide(const std::wstring& line);

} // namespace maku::logf
