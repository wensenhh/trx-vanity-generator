#include "utils/first_run.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>

int main() {
    // Reset any previous acknowledgment
    trx::FirstRunWizard::reset_acknowledgment();

    // First call should show notice and write marker
    bool ok1 = trx::FirstRunWizard::check_and_show();
    assert(ok1 && "check_and_show should return true in non-interactive mode");

    // Marker file should now exist
    bool ok2 = trx::FirstRunWizard::already_acknowledged();
    assert(ok2 && "already_acknowledged should be true after first call");

    // Second call should immediately return true without interaction
    bool ok3 = trx::FirstRunWizard::check_and_show();
    assert(ok3 && "check_and_show should return true after acknowledgment");

    // Reset for clean state
    trx::FirstRunWizard::reset_acknowledgment();

    std::cout << "PASS: first_run wizard smoke test\n";
    return 0;
}
