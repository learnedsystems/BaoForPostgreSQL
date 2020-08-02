# Introduction

This is the documentatation for Bao for PostgreSQL.

* GitHub: [https://learned.systems/bao](https://learned.systems/bao)


Note that Bao requires at least PostgreSQL 12.

Bao is a learned query optimizer for PostgreSQL. Bao works by providing automatic coarse-grained query hints (e.g., `SET enable_nestloop TO off`) on a per-query basis. Bao uses reinforcement learning, so Bao learns from it mistakes.

Bao has two components: the *Bao server*, which is a standalone Python application, and the *PostgreSQL extension*, which integrates directly with PostgreSQL and communicates with the Bao server. The best way to try out Bao is to follow [the tutorial](./tutorial.md).

This implementation has a number of features:

* In the default configuration, Bao works as a learned query optimizer, providing coarse-grained hints to the PostgreSQL query planner and incorporating feedback from query execution to improve its recommendations.
* Bao provides a continually-updated *query performance prediction* model that is custom-tailored to your DB and workload. Even if you do not use Bao for query optimization, you can still use it to predict the runtime of your queries. Runtime predictions are made available via `EXPLAIN`.
* Bao can be used as an *advisor*, simply providing the coarse-grained hints that Bao would use if Bao were running as a full optimizer. This allows you to manually apply Bao's recommendations to only a few queries.
* Since Bao uses reinforcement learning, Bao must balance exploration and exploitation, and will occasionally try out a query plan that may be slower than the one chosen by PostgreSQL; you have to make mistakes in order to learn! However, when regressions on certain queries are unacceptable, these special queries can be pre-explored using Bao's *exploratory mode*. Queries added to Bao's exploratory mode are tested at user-defined times, and future Bao models are checked to properly handle these queries. Bao will *never* pick a regressed query plan for a query processed in exploratory mode.
* Separate server process that can run on a different machine from your database. You can offload model training, potentially to a machine with a GPU. You can also co-locate multiple Bao servers together, if you have multiple DBs, so they share training resources.

Bao is provided under a GPLv3 license. Specifically, please note:

> THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
> APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
> HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY
> OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
> THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
> PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM
> IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF
> ALL NECESSARY SERVICING, REPAIR OR CORRECTION.
