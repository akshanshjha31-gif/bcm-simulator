/**
 * @file    scenarios.h
 * @brief   Registry of SIL scenarios.
 */
#ifndef BCM_SIL_SCENARIOS_H
#define BCM_SIL_SCENARIOS_H

#include "sil_runner.h"

#include <string>
#include <vector>

namespace bcm {
namespace sil {

typedef void (*ScenarioFn)(SilRunner&);

struct Scenario {
    const char* name;
    ScenarioFn  run;
};

/// Every scenario, in execution order.
std::vector<Scenario> all_scenarios();

}  // namespace sil
}  // namespace bcm

#endif /* BCM_SIL_SCENARIOS_H */
