![Bao loves PostgreSQL](https://github.com/LearnedSystems/BaoForPostgreSQL/blob/master/branding/bao_loves_pg.svg)

This is a prototype implementation of Bao for PostgreSQL. Bao is a learned query optimizer that learns to "steer" the PostgreSQL optimizer by issuing coarse-grained query hints. For more information about Bao, [check out the paper](https://rm.cab/bao).

Documentation, including a tutorial, is available here: https://rmarcus.info/bao_docs/

While this repository contains working prototype implementations of many of the pieces required to build a production-ready learned query optimizer, this code itself should not be used in production in its current form. Notable limitations include:

* The reward function is currently restricted to being a user-supplied value or the query latency in wall time. Thus, results may be inconsistent with high degrees of parallelism.
* The Bao server component does not perform any level of authentication or encryption. Do not run it on a machine directly accessible from an untrusted network.
* The code has not been audited for security issues. Since the PostgreSQL integration is written using the C hooks system, there are almost certainly issues.

This software is available under the AGPLv3 license. 

## FAQ

### Bao seems to take a long time to optimize queries. Why is optimization time so high?**

For simplicity, this prototype plans each hint (arm) sequentially. That means that if you are using 5 arms, Bao calls the PostgreSQL query planner 5 times per query. This work is in-theory embarrassingly parallel, but this prototype does not use parallelism.

To compensate for this in a rough way, you can measure the maximum time it takes to plan any arm, then pretend that time is how long the entire planning process took (i.e., perfect parallelism). Obviously, this will underestimate the planning time. *If you want true measurements of Bao' planning time, you'll need to implement parallel planning.*

Note that parallel planning is not normally needed to get good performance. Each call to the planner typically takes 50-200ms. So if a query plan takes multiple minutes to execute, the additional time from planning will be inconsequential. However, *if you are optimizing shorter queries, this may not be the case.*

For more information, see "Query optimization time" in Section 6.2 of the Bao paper.

### Bao isn't improving query performance for me, what's wrong?**

The two most common reasons for poor performance with Bao are:

1. **Tuning the set of hints/arms used**. By default, this prototype uses 5 arms that we found to be optimal for a GCP N1-4 VM. Since the IMDb dataset is much smaller than the average analytics dataset, we intentionally choose a small VM. *If you test on different hardware, you need to choose a different set of arms.* The easiest way to select a good set of arms is with manual testing: run all of your queries with all possible arms, then pick the set of arms that has the potential to improve performance the most. See Section 6.3 of the Bao paper for more information.

2. **Training with too little data**. While we think Bao's neural network model is much more sample efficient than prior work (e.g., Neo), Bao is still relatively "data hungry." You will need to train on, at minimum, hundreds of query executions in order to get reasonable results. Note that since this prototype uses a sliding window based approach (Section 3.2 of the Bao paper), Bao will only retrain it's model every 25 queries. This means that if you execute 50 queries, the first 25 will be assigned the default optimizer plan, the second 25 will use the Bao model trained on the first 25 queries. *Thus, for the last query executed, you are evaluating Bao with 25, not 49, training examples*.

### How can I test Bao on my own training / test splits?

The core learning algorithm in Bao is a reinforcement learning based approach. The usual "training set" and "testing set" terminology do not typically apply: Bao is designed to continually learn from a stream of queries. In the paper, we describe this as "cross validation over time," where Bao makes a decision at time `t` based only on data from time `t-1`. This is technically true, but might not be the most intuitive way to think about how reinforcement learning works.

Since Bao is not magic, it cannot magically extrapolate to totally novel and unseen queries. As an extreme example, consider a database table with four tables `a`, `b`, `c`, and `d`. If Bao is "trained" on queries over `a` and `b`, and then "tested" on queries over `c` and `d`, performance will be poor! Bao takes advantage of the fact that all the queries issued up to time `t` *gives you information about the query at time `t+1`*. If you engineer a scenario where this is not the case, Bao will unsurprisingly fail.

Thus, if you want to test Bao on your own workload, we suggest putting your queries into a random order and running Bao as intended. To increase robustness, you can measure performance across multiple random orderings. If you don't have enough queries in your workload, you can either (a) add noise to your queries to create new ones, or (b) "loop" the workload by repeating each query 1 to 3 times (note that if you repeat each query too many times, Bao might have the opportunity to test out every single hint!).

## Other work

A non-exhaustive list of extensions and applications of Bao, both from us and from others:

* Microsoft published two papers describing how they built a Bao-like system into the SCOPE analytics system: [paper 1](https://dl.acm.org/doi/10.1145/3448016.3457568) [paper 2](https://dl.acm.org/doi/10.1145/3514221.3526052)
* Woltmann et al. published [FASTgres](https://dl.acm.org/doi/10.14778/3611479.3611528), which combines clustering and supervised learning to train hint selection models in an offline fashion.
* Annesser et al. published [AutoSteer](https://dl.acm.org/doi/10.14778/3611540.3611544), which shows how Meta built a Bao-like system for their dashboarding analytics system.
* Yi et al. published [LimeQO](https://doi.org/10.1145/3663742.3663974), which learns ideal hints for an entire query workload at once, in an offline fashion.

Feel free to open PRs or contact us to add more!