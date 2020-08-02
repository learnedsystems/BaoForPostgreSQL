#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <math.h>

#include "bao_configs.h"
#include "bao_util.h"
#include "bao_bufferstate.h"
#include "bao_planner.h"
#include "postgres.h"
#include "fmgr.h"
#include "parser/parsetree.h"
#include "executor/executor.h"
#include "optimizer/planner.h"
#include "utils/guc.h"
#include "commands/explain.h"
#include "tcop/tcopprot.h"



PG_MODULE_MAGIC;
void _PG_init(void);
void _PG_fini(void);





// Bao works by integrating with PostgreSQL's hook functionality.
// 1) The bao_planner hook intercepts a query before the PG optimizer handles
//    it, and communicates with the Bao server.
// 2) The bao_ExecutorStart hook sets up time recording for the given query.
//    static PlannedStmt* bao_planner(Query* parse,
// 3) The bao_ExecutorEnd hook gets the query timing and sends the reward
//    for the query back to the Bao server.
// 4) The bao_ExplainOneQuery hook adds the Bao suggested hint and the reward
//    prediction to the EXPLAIN output of a query.
static PlannedStmt* bao_planner(Query *parse,
                                int cursorOptions, ParamListInfo boundParams);
static void bao_ExecutorStart(QueryDesc *queryDesc, int eflags);
static void bao_ExecutorEnd(QueryDesc *queryDesc);
static void bao_ExplainOneQuery(Query* query, int cursorOptions, IntoClause* into,
                                ExplainState* es, const char* queryString,
                                ParamListInfo params, QueryEnvironment *queryEnv);

static planner_hook_type prev_planner_hook = NULL;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;
static ExplainOneQuery_hook_type prev_ExplainOneQuery = NULL;

void _PG_init(void) {
  // install each Bao hook
  prev_ExecutorStart = ExecutorStart_hook;
  ExecutorStart_hook = bao_ExecutorStart;

  prev_ExecutorEnd = ExecutorEnd_hook;
  ExecutorEnd_hook = bao_ExecutorEnd;

  prev_planner_hook = planner_hook;
  planner_hook = bao_planner;

  prev_ExplainOneQuery = ExplainOneQuery_hook;
  ExplainOneQuery_hook = bao_ExplainOneQuery;

  // define Bao user-visible variables
  DefineCustomBoolVariable(
    "enable_bao",
    "Enable the Bao optimizer",
    "Enables the Bao optimizer. When enabled, the variables enable_bao_rewards"
    " and enable_bao_selection can be used to control whether or not Bao records"
    " query latency or selects query plans.",
    &enable_bao,
    false,
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomBoolVariable(
    "enable_bao_rewards",
    "Send reward info to Bao",
    "Enables reward collection. When enabled, and when enable_bao is true, query latencies"
    " are sent to the Bao server after execution.",
    &enable_bao_rewards,
    true,
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomBoolVariable(
    "enable_bao_selection",
    "Use Bao to select query plans",
    "Enables Bao query plan selection. When enabled, and when enable_bao is true, Bao"
    " will choose a query plan according to its learned model.",
    &enable_bao_selection,
    true,
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomStringVariable(
    "bao_host",
    "Bao server host", NULL,
    &bao_host,
    "localhost",
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomIntVariable(
    "bao_port",
    "Bao server port", NULL,
    &bao_port,
    9381, 1, 65536, 
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomIntVariable(
    "bao_num_arms",
    "Number of arms to consider",
    "The number of arms to consider for each query plan. Each arm represents "
    "a planner configuration. Higher values give better plans, but higher "
    "optimization times. The standard planner is always considered.",
    &bao_num_arms,
    5, 1, BAO_MAX_ARMS, 
    PGC_USERSET,
    0,
    NULL, NULL, NULL);

  DefineCustomBoolVariable(
    "bao_include_json_in_explain",
    "Includes Bao's JSON representation in EXPLAIN output.",
    "Includes Bao's JSON representation of a query plan in the "
    "output of EXPLAIN commands. Used by the Bao server.",
    &bao_include_json_in_explain,
    false,
    PGC_USERSET,
    0,
    NULL, NULL, NULL);
}


void _PG_fini(void) {
  elog(LOG, "finished extension");
}

static PlannedStmt* bao_planner(Query *parse,
                                int cursorOptions,
                                ParamListInfo boundParams) {
  // Bao planner. This is where we select a query plan.

  // The plan returned by the Bao planner, containing the PG plan,
  // the JSON query plan (for reward tracking), and the arm selected.
  BaoPlan* plan;

  // For timing Bao's overhead.
  clock_t t_start, t_final;
  double plan_time_ms;

  // Final PG plan to execute.
  PlannedStmt* to_return;

  if (prev_planner_hook) {
    elog(WARNING, "Skipping Bao hook, another planner hook is installed.");
    return prev_planner_hook(parse, cursorOptions,
                             boundParams);
  }

  // Skip optimizing this query if it is not a SELECT statement (checked by
  // `should_bao_optimize`), or if Bao is not enabled. We do not check
  // enable_bao_selection here, because if enable_bao is on, we still need
  // to attach a query plan to the query to record the reward later.
  if (!should_bao_optimize(parse) || !enable_bao) {
    return standard_planner(parse, cursorOptions,
                            boundParams);
  }


  t_start = clock();

  // Call Bao query planning routine (in `bao_planner.h`).
  plan = plan_query(parse, cursorOptions, boundParams);

  if (plan == NULL) {
    // something went wrong, default to the PG plan.
    return standard_planner(parse, cursorOptions, boundParams);
  }

  // We need some way to associate this query with the BaoQueryInfo data.
  // Hack: connect the Bao plan info to this plan via the queryId field.
  to_return = plan->plan;
  to_return->queryId = (uint64_t)(void*) plan->query_info;
  plan->query_info = NULL;
  
  t_final = clock();
  plan_time_ms = ((double)(t_final - t_start)
                  / (double)CLOCKS_PER_SEC) * (double)1000.0;
  
  elog(LOG, "Bao planning selected arm %d, in %f CPU ms.",
       plan->selection, plan_time_ms);

  // Free the BaoPlan* object now that we have gotten the BaoQueryInfo
  // and after we have gotten the PG plan out of it.
  free_bao_plan(plan);
  
  return to_return;
}


static void bao_ExecutorStart(QueryDesc *queryDesc, int eflags) {
  // Code from pg_stat_statements. If needed, setup query timing
  // to use as Bao's reward signal.

  if (prev_ExecutorStart)
    prev_ExecutorStart(queryDesc, eflags);
  else
    standard_ExecutorStart(queryDesc, eflags);

  if (enable_bao_rewards
      && queryDesc->plannedstmt->queryId != 0) {
    if (queryDesc->totaltime == NULL) {
      MemoryContext oldcxt;
      
      oldcxt = MemoryContextSwitchTo(queryDesc->estate->es_query_cxt);
      queryDesc->totaltime = InstrAlloc(1, INSTRUMENT_TIMER);
      MemoryContextSwitchTo(oldcxt);
    }
  }

}

static void bao_ExecutorEnd(QueryDesc *queryDesc) {
  // A query has finished. We need to check if it was a query Bao could optimize,
  // and if so, report the reward to the Bao server.
  
  BaoQueryInfo* bao_query_info;
  char* r_json;
  int conn_fd;

  if (enable_bao_rewards && should_report_reward(queryDesc)) {
    // We are tracking rewards for queries, and this query was
    // eligible for optimization by Bao.
    conn_fd = connect_to_bao(bao_host, bao_port);
    if (conn_fd < 0) {
      elog(WARNING, "Unable to connect to Bao server, reward for query will be dropped.");
      return;
    }

    if (!queryDesc->totaltime) {
      elog(WARNING, "Bao could not read instrumentation result, reward for query will be dropped.");
      return;
    }

    // Finalize the instrumentation so we can read the final time.
    InstrEndLoop(queryDesc->totaltime);

    // Generate a JSON blob with our reward.
    r_json = reward_json(queryDesc->totaltime->total * 1000.0);

    // Extract the BaoQueryInfo, which we hid inside the queryId of the
    // PlannedStmt. `should_report_reward` ensures it is set.
    bao_query_info = (BaoQueryInfo*)(void*)queryDesc->plannedstmt->queryId;
    queryDesc->plannedstmt->queryId = 0;

    // Write out the query plan, buffer information, and reward to the Bao
    // server.
    write_all_to_socket(conn_fd, START_FEEDBACK_MESSAGE);
    write_all_to_socket(conn_fd, bao_query_info->plan_json);
    write_all_to_socket(conn_fd, bao_query_info->buffer_json);
    write_all_to_socket(conn_fd, r_json);
    write_all_to_socket(conn_fd, TERMINAL_MESSAGE);
    shutdown(conn_fd, SHUT_RDWR);

    free_bao_query_info(bao_query_info);
  }
  
  if (prev_ExecutorEnd) {
    prev_ExecutorEnd(queryDesc);
  } else {
    standard_ExecutorEnd(queryDesc);
  }
}

static void bao_ExplainOneQuery(Query* query, int cursorOptions, IntoClause* into,
                                ExplainState* es, const char* queryString,
                                ParamListInfo params, QueryEnvironment* queryEnv) {
  
  PlannedStmt* plan;
  BaoPlan* bao_plan;
  instr_time plan_start, plan_duration;
  int conn_fd;
  char* buffer_json;
  char* plan_json;
  double prediction;
  char* hint_text;
  bool old_selection_val;
  bool connected = false;

  
  // If there are no other EXPLAIN hooks, add to the EXPLAIN output Bao's estimate
  // of this query plan's execution time, as well as what hints would be used
  // by Bao.
  
  // TODO: right now we add to the start of the EXPLAIN output, because I cannot
  //       figure out how to add to the end of it. 
  
  if (prev_ExplainOneQuery) {
    prev_ExplainOneQuery(query, cursorOptions, into, es,
                         queryString, params, queryEnv);
  }

  // There should really be a standard_ExplainOneQuery, but there
  // isn't, so we will do our best. We will replicate some PG code
  // here as a consequence.
  
  INSTR_TIME_SET_CURRENT(plan_start);
  plan = (planner_hook ? planner_hook(query, cursorOptions, params)
          : standard_planner(query, cursorOptions, params));
  INSTR_TIME_SET_CURRENT(plan_duration);
  INSTR_TIME_SUBTRACT(plan_duration, plan_start);
    
  if (!enable_bao) {
    // Bao is disabled, do the deault explain thing.
    ExplainOnePlan(plan, into, es, queryString,
                   params, queryEnv, &plan_duration);
    return;
  }

  buffer_json = buffer_state();
  plan_json = plan_to_json(plan);

  // Ask the Bao server for an estimate for this plan.
  conn_fd = connect_to_bao(bao_host, bao_port);
  if (conn_fd < 0) {
    elog(WARNING, "Unable to connect to Bao server, no prediction provided.");
    prediction = NAN;
  } else {
    write_all_to_socket(conn_fd, START_PREDICTION_MESSAGE);
    write_all_to_socket(conn_fd, plan_json);
    write_all_to_socket(conn_fd, buffer_json);
    write_all_to_socket(conn_fd, TERMINAL_MESSAGE);
    shutdown(conn_fd, SHUT_WR);
    
    // Read the response from the Bao server.
    if (read(conn_fd, &prediction, sizeof(double)) != sizeof(double)) {
      elog(WARNING, "Bao could not read the response from the server during EXPLAIN.");
      prediction = NAN;
    }

    connected = true;
    shutdown(conn_fd, SHUT_RDWR);
  }

  // Open a new explain group called "Bao" and add our prediction into it.
  ExplainOpenGroup("BaoProps", NULL, true, es);
  ExplainOpenGroup("Bao", "Bao", true, es);

  if (connected) {
    // The Bao server will (correctly) give a NaN if no model is available,
    // but PostgreSQL will dump that NaN into the raw JSON, causing parse bugs.
    if (isnan(prediction))
      ExplainPropertyText("Bao prediction", "NaN", es);
    else
      ExplainPropertyFloat("Bao prediction", "ms", prediction, 3, es);
  }
  
  if (bao_include_json_in_explain) {
    ExplainPropertyText("Bao plan JSON", plan_json, es);
    ExplainPropertyText("Bao buffer JSON", buffer_json, es);
  }

  free(plan_json);
  free(buffer_json);

  // Next, plan the query so that we can suggest a hint. If enable_bao_selection
  // was on, this repeats some work, as the query will be planned twice. That's OK
  // since EXPLAIN should still be fast.
  old_selection_val = enable_bao_selection;
  enable_bao_selection = true;
  bao_plan = plan_query(query, cursorOptions, params);
  enable_bao_selection = old_selection_val;
  
  if (!bao_plan) {
    elog(WARNING, "Could not plan query with Bao during explain, omitting hint.");
  } else {
    hint_text = arm_to_hint(bao_plan->selection);
    ExplainPropertyText("Bao recommended hint",
                        (hint_text ? hint_text : "(no hint)"),
                        es);
    free(hint_text);
    free_bao_plan(bao_plan);
  }
    
  ExplainCloseGroup("Bao", "Bao", true, es);
  ExplainCloseGroup("BaoProps", NULL, true, es);
  
  // Do the deault explain thing.
  ExplainOnePlan(plan, into, es, queryString,
                 params, queryEnv, &plan_duration);
}
