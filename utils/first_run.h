#pragma once

#include <string>

namespace trx {

// ============================================================================
// First-run wizard / security notice
// ============================================================================
// Shows an interactive security notice on the first CLI run.
// Stores acknowledgment in ~/.trx_vanity_first_run so it only appears once.
//
// Returns true if the user acknowledged (or has already acknowledged).
// Returns false if the user declined (program should exit).
// ============================================================================

class FirstRunWizard {
public:
    // Check and show first-run notice. Non-interactive / CI safe.
    // If stdin is not a TTY, skips interaction and prints a one-line reminder.
    static bool check_and_show();

    // Force reset (for testing)
    static bool reset_acknowledgment();

    // Exposed for unit tests
    static bool already_acknowledged();

private:
    static std::string marker_path();
    static bool write_acknowledgment();
};

} // namespace trx
