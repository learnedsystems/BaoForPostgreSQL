import random
import time
import psycopg2
import json

import storage
from common import BaoException
from config import read_config

# Code to block models that would create query regressions on important queries.
# The basic methodology is to allow the user to submit the SQL of important queries,
# which we store in an internal database. When triggered, we execute different
# plans for those queries, and record their performance (this is "exploration mode.")
# When a new model is proposed, we can compute it's maximum regression on the known
# queries.

_ALL_OPTIONS = [
    "enable_nestloop", "enable_hashjoin", "enable_mergejoin",
    "enable_seqscan", "enable_indexscan", "enable_indexonlyscan"
]

def _arm_idx_to_hints(arm_idx):
    hints = []
    for option in _ALL_OPTIONS:
        hints.append(f"SET {option} TO off")

    if arm_idx == 0:
        for option in _ALL_OPTIONS:
            hints.append(f"SET {option} TO on")
    elif arm_idx == 1:
        hints.append("SET enable_hashjoin TO on")
        hints.append("SET enable_indexonlyscan TO on")
        hints.append("SET enable_indexscan TO on")
        hints.append("SET enable_mergejoin TO on")
        hints.append("SET enable_seqscan TO on")
    elif arm_idx == 2:
        hints.append("SET enable_hashjoin TO on")
        hints.append("SET enable_indexonlyscan TO on")
        hints.append("SET enable_nestloop TO on")
        hints.append("SET enable_seqscan TO on")
    elif arm_idx == 3:
        hints.append("SET enable_hashjoin TO on")
        hints.append("SET enable_indexonlyscan TO on")
        hints.append("SET enable_seqscan TO on")
    elif arm_idx == 4:
        hints.append("SET enable_hashjoin TO on")
        hints.append("SET enable_indexonlyscan TO on")
        hints.append("SET enable_indexscan TO on")
        hints.append("SET enable_nestloop TO on")
        hints.append("SET enable_seqscan TO on")
    else:
        raise BaoException("RegBlocker only supports the first 5 arms")
    return hints
        
class ExperimentRunner:
    def __init__(self):
        config = read_config()
        self.__pg_connect_str = config["PostgreSQLConnectString"]
        self.__max_query_time = int(config["MaxQueryTimeSeconds"]) * 1000

    def __get_pg_cursor(self):
        try:
            conn = psycopg2.connect(self.__pg_connect_str)
            return conn.cursor()
        except psycopg2.OperationalError as e:
            raise BaoException("Could not connect to PG database") from e
        
    def add_experimental_query(self, sql):
        sql = sql.strip()
        if not sql.upper().startswith("SELECT"):
            raise BaoException("Experiment queries must be SELECT queries.")
        
        # First, make sure this query parses and that we can connect to PG.
        with self.__get_pg_cursor() as cur:
            try:
                cur.execute(f"EXPLAIN {sql}")
                cur.fetchall()
            except psycopg2.errors.ProgrammingError as e:
                raise BaoException(
                    "Could not generate EXPLAIN output for experimental query, "
                    + "it will not be added.") from e

        # Add this as an experimental query.
        storage.record_experimental_query(sql)

    def test_connection(self):
        with self.__get_pg_cursor() as cur:
            try:
                cur.execute("SELECT 1")
                cur.fetchall()
            except Exception as e:
                raise BaoException("Could not connect to the PostgreSQL database.") from e
        return True

    def status(self):
        to_r = {}
        
        to_r["Unexecuted experiments"] = len(storage.unexecuted_experiments())
        to_r["Completed experiments"] = len(storage.experiment_experience())
        to_r["Exploration queries"] = storage.num_experimental_queries()
        
        return to_r
        
    def explore(self, time_limit):
        start = time.time()
        unexecuted = storage.unexecuted_experiments()

        if not unexecuted:
            print("All experiments have been executed.")
            return

        print("We have", len(unexecuted), "unexecuted experiment(s).")
        random.shuffle(unexecuted)

        with self.__get_pg_cursor() as c:
            c.execute("SELECT pg_backend_pid()")
            pid = c.fetchall()[0][0]
            print("Running on backend PID", pid)
            c.execute("SET bao_include_json_in_explain TO on")
            c.execute("SET enable_bao TO on")
            c.execute("SET enable_bao_selection TO off")
            c.execute("SET enable_bao_rewards TO on")
            c.execute("commit")

            for experiment in unexecuted:
                experiment_id = experiment["id"]
                sql = experiment["query"]
                arm_idx = experiment["arm"]
                prev_id = storage.last_reward_from_pid(pid)

                time_remaining = round((time_limit - (time.time() - start)) * 1000.0)
                print("Time remaining:", time_remaining, "ms")
                if time_remaining < 0:
                    break

                statement_timeout = min(self.__max_query_time, time_remaining)
                is_timeout_from_time_remaining = time_remaining < self.__max_query_time

                # set PG to timeout and to use the arm we want to test
                c.execute(f"SET statement_timeout TO {statement_timeout}")
                for stmt in _arm_idx_to_hints(arm_idx):
                    c.execute(stmt)
                
                # get the Bao plan JSON so we can record a timeout if there is one
                c.execute("EXPLAIN (FORMAT JSON) " + sql)
                explain_json = c.fetchall()[0][0]
                
                bao_props, _qplan = explain_json

                bao_plan = json.loads(bao_props["Bao"]["Bao plan JSON"])
                bao_buffer = json.loads(bao_props["Bao"]["Bao buffer JSON"])
                
                try:
                    c.execute(sql)
                    c.fetchall()
                except psycopg2.errors.QueryCanceled as e:
                    assert "timeout" in str(e)
                    if is_timeout_from_time_remaining:
                        print("Hit experimental timeout, stopping.")
                        break

                    # otherwise, the timeout was because we went past the
                    # reasonable query limit. We should record that experiene.
                    print("Query hit timeout, recording 2*timeout as the reward.")
                    bao_plan["Buffers"] = bao_buffer
                    storage.record_reward(bao_plan, 2 * self.__max_query_time,
                                          pid)
                    c.execute("rollback")
                except psycopg2.OperationalError as e:
                    # this query caused the server to go down! give it a
                    # bit to restart, then try again.
                    print("Server down after experiment with arm", arm_idx)
                    if arm_idx != 0:
                        print("Treating this as a timeout and ceasing further experiments.")
                        bao_plan["Buffers"] = bao_buffer
                        storage.record_reward(bao_plan, 2 * self.__max_query_time,
                                              pid)
                    raise BaoException(f"Server down after experiment with arm {arm_idx}") from e

                retries_remaining = 5
                while (last_id := storage.last_reward_from_pid(pid)) == prev_id:
                    # wait a second to make sure the reward is flushed to the DB
                    time.sleep(1)
                    retries_remaining -= 1
                    if retries_remaining <= 0:
                        raise BaoException(
                            "Reward for experiment did not appear after 5 seconds, "
                            + "is the Bao server running?")

                # last_id is the ID of the experience for this experiment
                storage.record_experiment(experiment_id, last_id, arm_idx)
        print("Finished all experiments")


def compute_regressions(bao_reg):
    total_regressed = 0
    total_regression = 0
    for plan_group in storage.experiment_results():
        plan_group = list(plan_group)
        plans = [x["plan"] for x in plan_group]
        best_latency = min(plan_group, key=lambda x: x["reward"])["reward"]
        
        if bao_reg:
            selection = bao_reg.predict(plans).argmin()
        else:
            # If bao_reg is false-y, compare against PostgreSQL.
            selection = 0
                
        selected_plan_latency = plan_group[selection]["reward"]
        
        # Check to see if the regression is more than 1%.
        if selected_plan_latency > best_latency * 1.01:
            total_regressed += 1

        total_regression += selected_plan_latency - best_latency

    return (total_regressed, total_regression)


def should_replace_model(old_model, new_model):
    # Check the trained model for regressions on experimental queries.
    new_num_reg, new_reg_amnt = compute_regressions(new_model)
    cur_num_reg, cur_reg_amnt = compute_regressions(old_model)

    print("Old model # regressions:", cur_num_reg,
          "regression amount:", cur_reg_amnt)
    print("New model # regressions:", new_num_reg,
          "regression amount:", new_reg_amnt)

    # If our new model has no regressions, always accept it.
    # Otherwise, see if our regression profile is strictly better than
    # the previous model.
    if new_num_reg == 0:
        print("New model had no regressions.")
        return True
    elif cur_num_reg >= new_num_reg and cur_reg_amnt >= new_reg_amnt:
        print("New model with better regression profile",
              "than the old model.")
        return True
    else:
        print("New model did not have a better regression profile.")
        return False


if __name__ == "__main__":
    import model
    
    # Add Q1 and Q2 as sample queries.
    tmp = ExperimentRunner()

    try:
        with open("../sample_queries/q1_8a463.sql") as f:
            sql = f.read()
            tmp.add_experimental_query(sql)

        with open("../sample_queries/q2_8a82.sql") as f:
            sql = f.read()
            tmp.add_experimental_query(sql)
    except BaoException:
        pass # already have them

    tmp.explore(5 * 60)

    new_model = model.BaoRegression(have_cache_data=True)
    new_model.load("bao_default_model")
    print(compute_regressions(new_model))
    print(compute_regressions(None))
