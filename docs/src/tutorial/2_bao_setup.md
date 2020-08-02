# Bao Server Setup

After configuring the PostgreSQL extension, we'll next setup the Bao server. The Bao server is responsible for loading, maintaining, and serving the learned query optimization model. For now, we'll assume that the Bao server will run on the same machine as PostgreSQL, although this is not required.

The Bao server is configured with the `BaoForPostgreSQL/bao_server/bao.cfg` configuration file, but the defaults should work fine for this tutorial. Before starting the server, several Python dependencies are required. If you are using the VM, these are already installed. On Arch Linux, these can be installed by:

```
pacman -S python-scikit-learn python-numpy python-joblib python-pytorch-opt
```

If you are using a different Linux distribution, the package names may be slightly different. If you'd prefer, you can also install these dependencies with `pip`:

```
pip3 install scikit-learn numpy joblib
pip3 install torch==1.5.0+cpu -f https://download.pytorch.org/whl/torch_stable.html
```

Once these dependencies are installed, we can launch the Bao server.

```
$ cd bao_server
$ python3 main.py
Listening on localhost port 9381
Spawning server process...
```

With the Bao server running, we can now test to see if PostgreSQL can connect to it. If you are not running the Bao server on the same node as PostgreSQL, you'll need to change `ListenAddress` in `bao.cfg` and set the PostgreSQL variables `bao_host` and `bao_port`.

```
# add -h localhost if you are connecting to the VM from your host machine
$ psql -U imdb 
psql (12.3)
Type "help" for help.

imdb=# SET enable_bao TO on;
SET
imdb=# EXPLAIN SELECT count(*) FROM title;
                                        QUERY PLAN                                        
------------------------------------------------------------------------------------------
 Bao prediction: NaN
 Bao recommended hint: (no hint)
 Finalize Aggregate  (cost=50166.51..50166.52 rows=1 width=8)
   ->  Gather  (cost=50166.29..50166.50 rows=2 width=8)
         Workers Planned: 2
         ->  Partial Aggregate  (cost=49166.29..49166.30 rows=1 width=8)
               ->  Parallel Seq Scan on title  (cost=0.00..46532.63 rows=1053463 width=0)
(7 rows)
```

If everything is setup correctly, you should see two lines in the `EXPLAIN` output related to Bao -- the prediction and the hint. Since Bao currently has no experience, it has no model, so there's no prediction or hint provided. 

Next, we'll test to make sure Bao can correctly record feedback from PostgreSQL query executions. In the same session (with `enable_bao` set to `ON`), execute a query:

```
imdb=# SELECT count(*) FROM title;
  count  
---------
 2528312
(1 row)

```

If you look at the `stdout` of the Bao server, you should see a line like:
```
Logged reward of 2103.556027
```

This indicates that Bao has recorded a runtime of 2 seconds for this simple query plan. Note that Bao does not record your SQL queries, only a scrubbed version of your query plans. Bao keeps the type of each node, the estimated cost and cardinality, and the names of the involved relations. Bao never stores, for example, the predicate values from an executed query. Here's what Bao stores for the above query:

```json
{
  "Plan": {
    "Node Type": "Other",
    "Node Type ID": "42",
    "Total Cost": 50166.515833,
    "Plan Rows": 1,
    "Plans": [
      {
        "Node Type": "Other",
        "Node Type ID": "45",
        "Total Cost": 50166.500833,
        "Plan Rows": 2,
        "Plans": [
          {
            "Node Type": "Other",
            "Node Type ID": "42",
            "Total Cost": 49166.300833,
            "Plan Rows": 1,
            "Plans": [
              {
                "Node Type": "Seq Scan",
                "Node Type ID": "19",
                "Relation Name": "title",
                "Total Cost": 46532.633333,
                "Plan Rows": 1053463
              }
            ]
          }
        ]
      }
    ]
  },
  "Buffers": {
    "title_pkey": 1,
    "kind_id_title": 1
  }
}
```

