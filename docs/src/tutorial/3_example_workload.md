# Run an example workload

Next, we'll use Bao to execute a small sample workload. Then, we'll execute that same workload without Bao, and analyze the results.

The `sample_queries` folder in the repository root contains 40 sample queries drawn from the original join order benchmark (JOB)[^howgood] and from extended JOB[^neo]:

```
$ cd sample_queries
$ ls
q10_2a265.sql  q17_7a164.sql  q23_19d.sql    q29_6e.sql      q36_7a136.sql   q5_8a423.sql
q11_17e.sql    q18_7a103.sql  q24_32a.sql    q30_18c.sql     q37_2a1291.sql  q6_16b.sql
q12_17a.sql    q1_8a463.sql   q25_13d.sql    q31_2a39.sql    q3_7a99.sql     q7_7a48.sql
q13_7a121.sql  q19_2a471.sql  q26_2a274.sql  q32_2a493.sql   q38_2a1870.sql  q8_6a505.sql
q14_6a349.sql  q20_24b.sql    q27_3c.sql     q33_2a156.sql   q39_2a2781.sql  q9_5a48.sql
q15_18a.sql    q21_2a396.sql  q28_13a.sql    q34_1a275.sql   q40_2a8120.sql
q16_26c.sql    q22_8a27.sql   q2_8a82.sql    q35_1a1508.sql  q4_8a122.sql
```

The `run_queries.py` script will execute a random workload with 500 queries drawn from these samples. First, 25 queries will be executed to provide some basic training data for Bao. Then, and for every 25 queries processed afterwards, the script will pause query execution to retrain Bao's model.

The `run_queries.py` script assumes your DB is reachable on `localhost`, with the username `imdb` in the database `imdb`. If this is not the case, modify the `PG_CONNECTION_STR` variable at the top of the file.

Start the run:
```
$ python3 run_queries.py sample_queries/*.sql | tee ~/bao_run.txt
```

We use the `tee` command to both show us the output and redirect the output to a file, which we analyze later. Grab a coffee, this run will take a while to finish (around 75 minutes on my hardware).

Next, once this run is finished, change the line in `run_queries.py`:

```python
USE_BAO = True
```

To:


```python
USE_BAO = False
```

This will cause the `run_queries.py` script to execute the exact same workload, but without using Bao to select query plans and without retraining a model every 25 queries. Start the run:

```
$ python3 run_queries.py sample_queries/*.sql | tee ~/pg_run.txt

```

... and grab another coffee. This took just under 3 hours on my hardware. The fact that the workload finishes faster with Bao enabled is already telling, but next we will analyze these two runs in detail.

# Notes

[^howgood]: Leis, Viktor, Andrey Gubichev, Atanas Mirchev, Peter Boncz, Alfons Kemper, and Thomas Neumann. “How Good Are Query Optimizers, Really?” PVLDB, VLDB ’15, 9, no. 3 (2015): 204–215. https://doi.org/10.14778/2850583.2850594.

[^neo]: Marcus, Ryan, Parimarjan Negi, Hongzi Mao, Chi Zhang, Mohammad Alizadeh, Tim Kraska, Olga Papaemmanouil, and Nesime Tatbul. “Neo: A Learned Query Optimizer.” PVLDB, VLDB ’19, 12, no. 11 (2019): 1705–18.
