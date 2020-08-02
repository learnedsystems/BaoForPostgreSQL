#ifndef BAO_CONFIGS_H
#define BAO_CONFIGS_H

#include "c.h"

#define BAO_MAX_ARMS 26

// Each Bao config variable is linked to a PostgreSQL session variable.
// See the string docs provided to the PG functions in main.c.
static bool enable_bao = false;
static bool enable_bao_rewards = false;
static bool enable_bao_selection = false;
static char* bao_host = NULL;
static int bao_port = 9381;
static int bao_num_arms = 5;
static bool bao_include_json_in_explain = false;
#endif
