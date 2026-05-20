#pragma once

// Apply dashboard setpoints when PARAM_opmode == OPMODE_DASHBOARD (1).

#define OPMODE_GPIO       0
#define OPMODE_DASHBOARD  1

void Setpoint_Apply(void);  // call from Ms10Task after GPIO read if dashboard mode
