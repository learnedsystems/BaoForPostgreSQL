![Bao loves PostgreSQL](https://github.com/LearnedSystems/BaoForPostgreSQL/blob/master/branding/bao_loves_pg.svg)

This is a prototype implementation of Bao for PostgreSQL. Bao is a learned query optimizer that learns to "steer" the PostgreSQL optimizer by issuing coarse-grained query hints. For more information about Bao, [check out the paper](https://rm.cab/bao).

Documentation, including a tutorial, is available here: https://rmarcus.info/bao_docs/

While this repository contains working prototype implementations of many of the pieces required to build a production-ready learned query optimizer, this code itself should not be used in production in its current form. Notable limitations include:

* The reward function is currently restricted to being a user-supplied value or the query latency in wall time. Thus, results may be inconsistent with high degrees of parallelism.
* The Bao server component does not perform any level of authentication or encryption. Do not run it on a machine directly accessible from an untrusted network.
* The code has not been audited for security issues. Since the PostgreSQL integration is written using the C hooks system, there are almost certainly issues.

This software is available under the AGPLv3 license. 
