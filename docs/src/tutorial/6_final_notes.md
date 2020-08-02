# Final Notes

This tutorial was intended to give you an idea for what Bao could do and how the various components of Bao work. However, please note that the gains we saw in this tutorial might not be reflected in your real workload. Here's a few things to keep in mind:

* This workload was artifically constructed to represent a mixture of queries where the PostgreSQL optimizer found the best plan, and where PostgreSQL found a terrible plan. Most queries fall somewhere in between these two extremes. Check out [the Bao paper](https://rm.cab/bao) for a more detailed analysis on more realistic workloads.
* Bao's query optimizer has overhead. For queries that run very quickly (under 500ms), Bao is unlikely to be helpful, and may just slow down these queries with additional optimization time.
* Bao's value model needs to be retrained and verified, beyond what exploration mode could do. Most of the time, the verification done by Bao currently is sufficient. If training goes awry, Bao always saves the previous model. Training could easily be configured with a `cron` job.  We're currently working on more advanced mechanisms for this, but currenlty it has to be done manually.
