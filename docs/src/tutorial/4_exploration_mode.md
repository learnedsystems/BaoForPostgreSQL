# Use Exploration Mode

Bao is powered by reinforcement learning, and must therefore strike a balance between the *exploration* of new plans and the *exploitation* of plans that are known to be good. If you do no exploration, you'll never do any better than PostgreSQL -- if you do too much exploration, you'll suffer many query regressions.

Bao uses an algorithm called [Thompson sampling](https://en.wikipedia.org/wiki/Thompson_sampling), which has a theoretical guarantee about *regret*. Regret is the difference in performance between the plan chosen by Bao and the (unknown) best possible choice. Thompson sampling ensures that, in the long run, regret approaches zero.[^limit] This is considered to be an optimal balance of exploration and exploitation.

However, sometimes "in the long run" isn't good enough for an application. For example, you may need to ensure that a particular query *never* regresses. In order to do this, Bao supports *exploration mode*, a special facility that lets Bao explore plans for specific queries offline, and then ensure the best plan is *always* chosen at runtime.

At a high level, exploration mode works as follows. You tell Bao about a particular SQL query that you never want to regress. Then, you give Bao a fixed period of time -- like 20 minutes -- to run experiments against your database. Bao uses this time (and no more) to run as many experiments as it can, saving the results. When a new model is trained, that model is checked to make sure it would make the right decision for each executed experiment. If the model would not, the model is retrained, with increased emphasis on those experiments. 

We'll start from scratch, removing our previous Bao model and the experience we observed. Stop the Bao server (i.e., Control + C), then delete the model, Bao DB, and restart PostgreSQL:

```
$ rm -rf bao_server/bao_default_model
$ rm bao_server/bao.db
$ systemctl restart postgresql
```

## Configure Bao to talk to PostgreSQL

Until now, the PostgreSQL extension has communicated with the Bao server, but the Bao server has never directly connected to the database. For exploration mode, we'll need such a connection. Edit the `bao_server/bao.cfg` file to tell Bao how to connect to your PostgreSQL instance:

```ini
# ==============================================================
# EXPLORATION MODE SETTINGS
# ==============================================================

# maximum time a query should reasonably take (used in
# exploration mode).
MaxQueryTimeSeconds = 120

# psycopg2 / JDBC connection string to access PostgreSQL
# (used by the experiment runner to prevent regressions)
PostgreSQLConnectString = user=imdb

```

* `MaxQueryTimeSeconds` is an upper-bound on how long any non-regressed query plan you add to exploration mode should take. For this sample workload, 120 seconds was a reasonable value for this workload. Bao uses this value as a cutoff for its experiments, assuming any plan that takes longer than this amount of time must be a regression. Don't set this value too tightly, however, because Bao can still gain knowledge from observing *how much* a query plan regressed. 
* `PostgreSQLConnectString` is the JDBC-like string used by the Bao server to connect to the postgreSQL database. You can find documentation for it [from the psycopg docs](https://www.psycopg.org/docs/module.html#psycopg2.connect).


### Testing the connection

To test the connection, we can use the `baoctl.py` script. You can find this script in the `bao_server` directory. It should be executed on the same machine as the Bao server is running.

Run `baoctl.py --test-connection` to see if Bao can connect to your PostgreSQL instance:

```
$ python3 baoctl.py --test-connection
Connection successful!
```

## Adding exploration queries

Once Bao can connect to our PostgreSQL instance, we can add exploration queries. Let's add the first three workload queries:

```
$ python3 baoctl.py --add-test-query ../sample_queries/q1_8a463.sql 
Added new test query.
$ python3 baoctl.py --add-test-query ../sample_queries/q2_8a82.sql 
Added new test query.
$ python3 baoctl.py --add-test-query ../sample_queries/q3_7a99.sql 
Added new test query.
```

## Start exploration

You can give Bao as much or as little time to execute experiments as you'd like. It is a good idea to provide at least a little longer than `MaxQueryTimeSeconds`, to ensure that at least one experiment is fully executed.

Each query added creates 5 experiments, so we currently have 15 un-ran experiments. We can see this by running `baoctl.py --status`:

```
$ python3 baoctl.py --status
Unexecuted experiments : 15
Completed experiments  : 0
Exploration queries    : 3
```

We can start executing these 15 experiments by running `baoctl.py --experiment`. We'll give Bao 30 minutes to run these experiments so that they can all be finished:

```
$ python3 baoctl.py --experiment 1800
We have 15 unexecuted experiment(s).
Running on backend PID 57718
...
Finished all experiments
```


Note: sometimes, a particular configuration will cause the PostgreSQL session to crash. When this is the case, PostgreSQL automatically restarts / recovers. However, since this can be an expensive operation, Bao ends any experimentation when this occurs. Bao notes when a configuration causes such a crash, and will not execute it again. If this occurs during the tutorial, restart the exploration process until every experiment finishes. When this occurs, the output looks like this:

```
Time remaining: 1539696 ms
Server down after experiment with arm 1
Treating this as a timeout and ceasing further experiments.
Logged reward of 240000
Finished all experiments
```

Once these experiments finish, we can re-run the entire workload and analyze the performance:


```
$ python3 run_queries.py sample_queries/*.sql | tee ~/bao_with_regblock.txt
```

Grab yet another coffee. This one will be faster, but will still take some time.


# Notes

[^limit]: Precisely, the gaurantee is that the limit as time tends to infinity is that regret will tend towards zero. This gaurantee makes several assumptions about the underlying reward function, which do not strictly apply to query optimization. Nevertheless, Thompson sampling appears to be an effective algorithm in this domain.
