#pragma once

#include "../globals/globals.h"

constexpr int NETWORK_TASK_STACK_SIZE = 4935;
constexpr int WIFI_CONNECTION_TASK_STACK_SIZE = 2505;

bool startNetworkTask(TaskParams_t* params);
void stopNetworkTask();
