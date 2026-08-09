#pragma once

namespace maku::cli {

/// True when argv asks for the command-line interface rather than the GUI.
/// Anything that does not start with '-' or '/' is a subcommand; the legacy
/// GUI switches (`-p`, `/mgr`, …) keep opening the window as before.
bool WantsCli(int argc, wchar_t** argv);

/// Runs the requested command and returns the process exit code.
/// 0 success, 1 failure, 2 usage error.
int Run(int argc, wchar_t** argv);

} // namespace maku::cli
