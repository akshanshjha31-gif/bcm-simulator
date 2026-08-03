/**
 * @file    sil_main.cpp
 * @brief   SIL harness entry point and pass/fail report.
 *
 * Runs every scenario against services::BcmCore and prints a report. Exits
 * non-zero on any failure so CI can gate on it.
 *
 * Usage:
 *   bcm_sil               run everything
 *   bcm_sil --verbose     also list the checks that passed
 *   bcm_sil <name>...     run only the named scenarios
 *   bcm_sil --list        list scenario names
 */
#include "scenarios.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using bcm::services::Config;
using bcm::sil::CheckResult;
using bcm::sil::Scenario;
using bcm::sil::ScenarioResult;
using bcm::sil::SilRunner;

namespace {

void print_scenario(const ScenarioResult& r, bool verbose)
{
    const bool ok = r.passed();
    std::cout << (ok ? "  PASS  " : "  FAIL  ")
              << std::left << std::setw(48) << r.name
              << "  " << r.requirement << "\n";

    for (const CheckResult& c : r.checks) {
        if (c.passed && !verbose) { continue; }
        std::cout << (c.passed ? "         . " : "         X ")
                  << std::left << std::setw(52) << c.description
                  << "@" << c.at_ms << " ms\n";
        if (!c.passed && !c.detail.empty()) {
            std::cout << "             -> " << c.detail << "\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    bool                     verbose = false;
    bool                     list    = false;
    std::vector<std::string> selected;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") { verbose = true; }
        else if (arg == "--list")              { list = true; }
        else                                   { selected.push_back(arg); }
    }

    const std::vector<Scenario> scenarios = bcm::sil::all_scenarios();

    if (list) {
        for (const Scenario& s : scenarios) { std::cout << s.name << "\n"; }
        return 0;
    }

    std::cout << "\nBCM Software-in-the-Loop\n"
              << "Object under test: services::BcmCore "
                 "(the same code the firmware links)\n"
              << "------------------------------------------------------"
                 "-----------------------\n";

    Config   cfg;
    unsigned scenarios_run = 0U;
    unsigned scenarios_failed = 0U;
    unsigned checks_total = 0U;
    unsigned checks_failed = 0U;

    for (const Scenario& s : scenarios) {
        if (!selected.empty()) {
            bool wanted = false;
            for (const std::string& n : selected) {
                if (n == s.name) { wanted = true; break; }
            }
            if (!wanted) { continue; }
        }

        /* A fresh runner per scenario: no state may leak between them, or a
         * pass could depend on whatever ran before it. */
        SilRunner runner(cfg);
        s.run(runner);
        const ScenarioResult result = runner.end();

        print_scenario(result, verbose);

        ++scenarios_run;
        if (!result.passed()) { ++scenarios_failed; }
        checks_total  += static_cast<unsigned>(result.checks.size());
        checks_failed += result.failures();
    }

    std::cout << "------------------------------------------------------"
                 "-----------------------\n";

    if (scenarios_run == 0U) {
        std::cout << "No scenarios matched. Use --list to see the names.\n";
        return 2;
    }

    std::cout << "scenarios: " << scenarios_run
              << "  passed: " << (scenarios_run - scenarios_failed)
              << "  failed: " << scenarios_failed << "\n"
              << "checks:    " << checks_total
              << "  passed: " << (checks_total - checks_failed)
              << "  failed: " << checks_failed << "\n";

    if (scenarios_failed == 0U) {
        std::cout << "\nRESULT: PASS\n\n";
        return 0;
    }
    std::cout << "\nRESULT: FAIL\n\n";
    return 1;
}
