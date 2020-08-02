#ifndef BAO_UTIL_H
#define BAO_UTIL_H

#include <arpa/inet.h>
#include <unistd.h>

#include "postgres.h"
#include "optimizer/planner.h"
#include "optimizer/cost.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "executor/execdesc.h"

// Utility functions and common structs used throughout Bao.

// JSON tags for sending to the Bao server.
static const char* START_QUERY_MESSAGE = "{\"type\": \"query\"}\n";
static const char *START_FEEDBACK_MESSAGE = "{\"type\": \"reward\"}\n";
static const char* START_PREDICTION_MESSAGE = "{\"type\": \"predict\"}\n";
static const char* TERMINAL_MESSAGE = "{\"final\": true}\n";


// Bao-specific information associated with a query plan.
typedef struct BaoQueryInfo {
  // A JSON representation of the query plan we can send to the Bao server.
  char* plan_json;

  // A JSON representation of the buffer state when the query was planned.
  char* buffer_json;
} BaoQueryInfo;


// A struct containing a PG query plan and the related Bao-specific information.
typedef struct BaoPlan {
  BaoQueryInfo* query_info;

  // The PostgreSQL plan.
  PlannedStmt* plan;

  // The arm index we used to generate this plan.
  unsigned int selection;
} BaoPlan;

// Free a BaoQueryInfo struct.
static void free_bao_query_info(BaoQueryInfo* info) {
  if (!info) return;
  if (info->plan_json) free(info->plan_json);
  if (info->buffer_json) free(info->buffer_json);
  free(info);
}

// Free a BaoPlan (including the contained BaoQueryInfo).
static void free_bao_plan(BaoPlan* plan) {
  if (!plan) return;
  if (plan->query_info) free_bao_query_info(plan->query_info);
  free(plan);
}

// Determine if we should report the reward of this query or not.
static bool should_report_reward(QueryDesc* queryDesc) {
  // before reporting a reward, check that:
  // (1) that the query ID is not zero (query ID is left as 0 for INSERT, UPDATE, etc.)
  // (2) that the query actually executed (e.g., was not an EXPLAIN).
  // (3) the the instrument_options is zero (e.g., was not an EXPLAIN ANALYZE)
  return (queryDesc->plannedstmt->queryId != 0
          && queryDesc->already_executed
          && queryDesc->instrument_options == 0);
}

// Determine if we should optimize this query or not.
static bool should_bao_optimize(Query* parse) {
  Oid relid;
  char* namespace;

  // Don't try and optimize anything that isn't a SELECT query.
  if (parse->commandType != CMD_SELECT) return false; 

  // Iterate over all the relations in this query.
  for (int i = 0; i < list_length(parse->rtable); i++) {
    relid = rt_fetch(i, parse->rtable)->relid;
    // A relid of zero seems to have a special meaning, and it causes
    // get_rel_namespace or get_namespace_name to crash. Relid of zero
    // doesn't seem to appear in "normal" queries though.
    if (!relid) return false;

    // Ignore queries that involve the pg_catalog (internal data used by PostgreSQL).
    namespace = get_namespace_name(get_rel_namespace(relid));
    if (strcmp(namespace, "pg_catalog") == 0) return false;
  }

  return true;

}


// https://stackoverflow.com/a/4770992/1464282
static bool starts_with(const char *str, const char *pre) {
  return strncmp(pre, str, strlen(pre)) == 0;
}

// Create a JSON object containing the reward, suitable to send to the Bao
// server.
static char* reward_json(double reward) {
  char* buf;
  size_t json_size;
  FILE* stream;
  pid_t pid = getpid();
  
  stream = open_memstream(&buf, &json_size);

  fprintf(stream, "{\"reward\": %f , \"pid\": %d }\n", reward, pid);
  fclose(stream);

  return buf;

}

// Write the entire string to the given socket.
static void write_all_to_socket(int conn_fd, const char* json) {
  size_t json_length;
  ssize_t written, written_total;
  json_length = strlen(json);
  written_total = 0;
  
  while (written_total != json_length) {
    written = write(conn_fd,
                    json + written_total,
                    json_length - written_total);
    written_total += written;
  }
}

// Connect to the Bao server.
static int connect_to_bao(const char* host, int port) {
  int ret, conn_fd;
  struct sockaddr_in server_addr = { 0 };

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, host, &server_addr.sin_addr);
  conn_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (conn_fd < 0) {
    return conn_fd;
  }
  
  ret = connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (ret == -1) {
    return ret;
  }

  return conn_fd;

}

// Get the relation name of a particular plan node with a PostgreSQL
// PlannedStmt.
static char* get_relation_name(PlannedStmt* stmt, Plan* node) {
  Index rti;

  switch (node->type) {
  case T_SeqScan:
  case T_SampleScan:
  case T_IndexScan:
  case T_IndexOnlyScan:
  case T_BitmapHeapScan:
  case T_BitmapIndexScan:
  case T_TidScan:
  case T_ForeignScan:
  case T_CustomScan:
  case T_ModifyTable:
    rti = ((Scan*)node)->scanrelid;
    return get_rel_name(rt_fetch(rti, stmt->rtable)->relid);
    break;
  default:
    return NULL;
  }
}


#endif
