import sqlite3
import json
import itertools

from common import BaoException

def _bao_db():
    conn = sqlite3.connect("bao.db")
    c = conn.cursor()
    c.execute("""
CREATE TABLE IF NOT EXISTS experience (
    id INTEGER PRIMARY KEY,
    pg_pid INTEGER,
    plan TEXT, 
    reward REAL
)""")
    c.execute("""
CREATE TABLE IF NOT EXISTS experimental_query (
    id INTEGER PRIMARY KEY, 
    query TEXT UNIQUE
)""")
    c.execute("""
CREATE TABLE IF NOT EXISTS experience_for_experimental (
    experience_id INTEGER,
    experimental_id INTEGER,
    arm_idx INTEGER,
    FOREIGN KEY (experience_id) REFERENCES experience(id),
    FOREIGN KEY (experimental_id) REFERENCES experimental_query(id),
    PRIMARY KEY (experience_id, experimental_id, arm_idx)
)""")
    conn.commit()
    return conn

def record_reward(plan, reward, pid):
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("INSERT INTO experience (plan, reward, pg_pid) VALUES (?, ?, ?)",
                  (json.dumps(plan), reward, pid))
        conn.commit()

    print("Logged reward of", reward)

def last_reward_from_pid(pid):
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("SELECT id FROM experience WHERE pg_pid = ? ORDER BY id DESC LIMIT 1",
                  (pid,))
        res = c.fetchall()
        if not res:
            return None
        return res[0][0]

def experience():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("SELECT plan, reward FROM experience")
        return c.fetchall()

def experiment_experience():
    all_experiment_experience = []
    for res in experiment_results():
        all_experiment_experience.extend(
            [(x["plan"], x["reward"]) for x in res]
        )
    return all_experiment_experience
    
def experience_size():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("SELECT count(*) FROM experience")
        return c.fetchone()[0]

def clear_experience():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("DELETE FROM experience")
        conn.commit()

def record_experimental_query(sql):
    try:
        with _bao_db() as conn:
            c = conn.cursor()
            c.execute("INSERT INTO experimental_query (query) VALUES(?)",
                      (sql,))
            conn.commit()
    except sqlite3.IntegrityError as e:
        raise BaoException("Could not add experimental query. "
                           + "Was it already added?") from e

    print("Added new test query.")

def num_experimental_queries():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("SELECT count(*) FROM experimental_query")
        return c.fetchall()[0][0]
    
def unexecuted_experiments():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("CREATE TEMP TABLE arms (arm_idx INTEGER)")
        c.execute("INSERT INTO arms (arm_idx) VALUES (0),(1),(2),(3),(4)")

        c.execute("""
SELECT eq.id, eq.query, arms.arm_idx 
FROM experimental_query eq, arms
LEFT OUTER JOIN experience_for_experimental efe 
     ON eq.id = efe.experimental_id AND arms.arm_idx = efe.arm_idx
WHERE efe.experience_id IS NULL
""")
        return [{"id": x[0], "query": x[1], "arm": x[2]}
                for x in c.fetchall()]

def experiment_results():
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("""
SELECT eq.id, e.reward, e.plan, efe.arm_idx
FROM experimental_query eq, 
     experience_for_experimental efe, 
     experience e 
WHERE eq.id = efe.experimental_id AND e.id = efe.experience_id
ORDER BY eq.id, efe.arm_idx;
""")
        for eq_id, grp in itertools.groupby(c, key=lambda x: x[0]):
            yield ({"reward": x[1], "plan": x[2], "arm": x[3]} for x in grp)
        

def record_experiment(experimental_id, experience_id, arm_idx):
    with _bao_db() as conn:
        c = conn.cursor()
        c.execute("""
INSERT INTO experience_for_experimental (experience_id, experimental_id, arm_idx)
VALUES (?, ?, ?)""", (experience_id, experimental_id, arm_idx))
        conn.commit()


# select eq.id, efe.arm_idx, min(e.reward) from experimental_query eq, experience_for_experimental efe, experience e WHERE eq.id = efe.experimental_id AND e.id = efe.experience_id GROUP BY eq.id;
