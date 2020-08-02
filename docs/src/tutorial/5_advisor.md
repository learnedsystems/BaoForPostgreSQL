# Bao as an advisor

For some applications, any amount of exploration -- and thus regression -- on any query is unacceptable. In these scenarios, it is possible to use Bao as an advisor instead of a full-blown optimizer. 

To demonstrate this, let's use a simple `psql` session.

```
$ psql -U imdb -h localhost
psql (12.2, server 12.3)
Type "help" for help.

imdb=# 
```

Bao can be controlled through three main session-level PostgreSQL configs.

* `enable_bao` is the top level config. When set to `off`, the default, Bao does not observe or interfere with the query optimizer at all. When set to `on`, Bao will behave according to the values of `enable_bao_rewards` and `enable_bao_selection`.
* `enable_bao_rewards` determines whether or not Bao collects additional experience from queries from this session. 
* `enable_bao_selection` determines whether or not Bao will use its value model to select query plans.

To use Bao as a pure advisor, we can set `enable_bao` but disable `enable_bao_rewards` and `enable_bao_selection`. 

```
imdb=# SET enable_bao TO on;
SET
imdb=# SET enable_bao_selection TO off;
SET
imdb=# SET enable_bao_rewards TO off;
SET
```

Next, we'll execute a simple `EXPLAIN` statement:

```
imdb=# EXPLAIN SELECT count(*) FROM title;
                                        QUERY PLAN                                        
------------------------------------------------------------------------------------------
 Bao prediction: 894.349 ms
 Bao recommended hint: (no hint)
 Finalize Aggregate  (cost=50165.40..50165.41 rows=1 width=8)
   ->  Gather  (cost=50165.19..50165.40 rows=2 width=8)
         Workers Planned: 2
         ->  Partial Aggregate  (cost=49165.19..49165.20 rows=1 width=8)
               ->  Parallel Seq Scan on title  (cost=0.00..46531.75 rows=1053375 width=0)
(7 rows)

imdb=# 
```

Since `enable_bao_selection` is off, this plan is generated using the PostgreSQL optimizer, exactly as it would be if you were not using Bao. Two additional lines are added to the outputof the `EXPLAIN` plan:

* **Bao prediction** shows the time that Bao thinks this query plan will take to execute. In this case, about a second.
* **Bao recommended hint** shows the query hint that Bao *would* use if `enable_bao_selection` was on. In this case, Bao would not use any query hints.

Let's execute the query and run the same `EXPLAIN` statement again:

```
imdb=# SELECT count(*) FROM title;
  count  
---------
 2528312
(1 row)

imdb=# EXPLAIN SELECT count(*) FROM title;
                                        QUERY PLAN                                        
------------------------------------------------------------------------------------------
 Bao prediction: 661.193 ms
 Bao recommended hint: (no hint)
 Finalize Aggregate  (cost=50165.40..50165.41 rows=1 width=8)
   ->  Gather  (cost=50165.19..50165.40 rows=2 width=8)
         Workers Planned: 2
         ->  Partial Aggregate  (cost=49165.19..49165.20 rows=1 width=8)
               ->  Parallel Seq Scan on title  (cost=0.00..46531.75 rows=1053375 width=0)
(7 rows)
```

Bao's prediction for the query changed, even though the query plan, cost estimates, and cardinality estimates are all the same as before! This is because the buffer pool has changed states: after the execution of the `SELECT` query, more data relevant to this query has been cached, so Bao predicts that it will execute faster.

Of course, predictions are exactly that -- predictions. While Bao's value model should get better over time, these predictions should be used as *advice*, and have no bounds whatsoever (although Bao will never predict a negative query runtime).

Let's look at `q1` from our sample workload. You can copy and paste the below statement to see the `EXPLAIN` output.

```sql
EXPLAIN SELECT COUNT(*) FROM title as t, kind_type as kt, info_type as it1, movie_info as mi1, cast_info as ci, role_type as rt, name as n, movie_keyword as mk, keyword as k, movie_companies as mc, company_type as ct, company_name as cn WHERE t.id = ci.movie_id AND t.id = mc.movie_id AND t.id = mi1.movie_id AND t.id = mk.movie_id AND mc.company_type_id = ct.id AND mc.company_id = cn.id AND k.id = mk.keyword_id AND mi1.info_type_id = it1.id AND t.kind_id = kt.id AND ci.person_id = n.id AND ci.role_id = rt.id AND (it1.id IN ('7')) AND (mi1.info in ('MET:','OFM:35 mm','PCS:Digital Intermediate','PFM:35 mm','PFM:Video','RAT:1.33 : 1','RAT:1.37 : 1')) AND (kt.kind in ('episode','movie','tv movie')) AND (rt.role in ('actor','actress')) AND (n.gender in ('f','m') OR n.gender IS NULL) AND (n.name_pcode_cf in ('A5362','J5252','R1632','R2632','W4525')) AND (t.production_year <= 2015) AND (t.production_year >= 1925) AND (cn.name in ('Fox Network','Independent Television (ITV)','Metro-Goldwyn-Mayer (MGM)','National Broadcasting Company (NBC)','Paramount Pictures','Shout! Factory','Sony Pictures Home Entertainment','Universal Pictures','Universal TV')) AND (ct.kind in ('distributors','production companies'));
```

This should result in something like this:

```
imdb=# EXPLAIN SELECT COUNT(*) FROM title as t, kind_type as kt, info_type as it1, movie_info as mi1, cast_info as ci, role_type as rt, name as n, movie_keyword as mk, keyword as k, movie_companies as mc, company_type as ct, company_name as cn WHERE t.id = ci.movie_id AND t.id = mc.movie_id AND t.id = mi1.movie_id AND t.id = mk.movie_id AND mc.company_type_id = ct.id AND mc.company_id = cn.id AND k.id = mk.keyword_id AND mi1.info_type_id = it1.id AND t.kind_id = kt.id AND ci.person_id = n.id AND ci.role_id = rt.id AND (it1.id IN ('7')) AND (mi1.info in ('MET:','OFM:35 mm','PCS:Digital Intermediate','PFM:35 mm','PFM:Video','RAT:1.33 : 1','RAT:1.37 : 1')) AND (kt.kind in ('episode','movie','tv movie')) AND (rt.role in ('actor','actress')) AND (n.gender in ('f','m') OR n.gender IS NULL) AND (n.name_pcode_cf in ('A5362','J5252','R1632','R2632','W4525')) AND (t.production_year <= 2015) AND (t.production_year >= 1925) AND (cn.name in ('Fox Network','Independent Television (ITV)','Metro-Goldwyn-Mayer (MGM)','National Broadcasting Company (NBC)','Paramount Pictures','Shout! Factory','Sony Pictures Home Entertainment','Universal Pictures','Universal TV')) AND (ct.kind in ('distributors','production companies'));


                               QUERY PLAN                                                                                                                  
----------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------
 Bao prediction: 74053.945 ms
 Bao recommended hint: SET enable_nestloop TO off; 
 Aggregate  (cost=9644.44..9644.45 rows=1 width=8)
   ->  Gather  (cost=1006.53..9644.44 rows=1 width=0)
         Workers Planned: 1
         ->  Nested Loop  (cost=6.53..8644.34 rows=1 width=0)
               ->  Hash Join  (cost=6.10..8643.30 rows=1 width=4)
               ...
```

Here, Bao thinks the query plan generated by PostgreSQL will take a little over a minute to execute. Bao recommends a hint to disable nested loop joins for this query. Let's take the hint, and re-run the EXPLAIN:

```
imdb=# SET enable_nestloop TO off;
SET
imdb=# EXPLAIN SELECT COUNT(*) FROM title as t, kind_type as kt, info_type as it1, movie_info as mi1, cast_info as ci, role_type as rt, name as n, movie_keyword as mk, keyword as k, movie_companies as mc, company_type as ct, company_name as cn WHERE t.id = ci.movie_id AND t.id = mc.movie_id AND t.id = mi1.movie_id AND t.id = mk.movie_id AND mc.company_type_id = ct.id AND mc.company_id = cn.id AND k.id = mk.keyword_id AND mi1.info_type_id = it1.id AND t.kind_id = kt.id AND ci.person_id = n.id AND ci.role_id = rt.id AND (it1.id IN ('7')) AND (mi1.info in ('MET:','OFM:35 mm','PCS:Digital Intermediate','PFM:35 mm','PFM:Video','RAT:1.33 : 1','RAT:1.37 : 1')) AND (kt.kind in ('episode','movie','tv movie')) AND (rt.role in ('actor','actress')) AND (n.gender in ('f','m') OR n.gender IS NULL) AND (n.name_pcode_cf in ('A5362','J5252','R1632','R2632','W4525')) AND (t.production_year <= 2015) AND (t.production_year >= 1925) AND (cn.name in ('Fox Network','Independent Television (ITV)','Metro-Goldwyn-Mayer (MGM)','National Broadcasting Company (NBC)','Paramount Pictures','Shout! Factory','Sony Pictures Home Entertainment','Universal Pictures','Universal TV')) AND (ct.kind in ('distributors','production companies'));


                                        QUERY PLAN                                                                                                                    
           
----------------------------------------------------------------------------------
 Bao prediction: 15032.299 ms
 Bao recommended hint: SET enable_nestloop TO off; 
 Aggregate  (cost=10000977592.82..10000977592.83 rows=1 width=8)
   ->  Nested Loop  (cost=10000683246.19..10000977592.82 rows=1 width=0)
         ->  Seq Scan on info_type it1  (cost=0.00..2.41 rows=1 width=4)
               Filter: (id = 7)
               ...
```

For the new query plan, Bao predicts a time of only 15 seconds. If you'd like, you can execute both query plans to measure the quality of Bao's predictions.

Keep in mind that using Bao as we've configured it here won't let the value model get any better. In order to improve its model, Bao needs experience. For any given session, you can allow Bao to collect experience and thus improve its model, but still prevent Bao from modifying any query plans. To do this, just set `enable_bao` and `enable_bao_rewards` to `ON`, but set `enable_bao_selection` to `OFF`.
