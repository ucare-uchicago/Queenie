# Queenie

Queenie is a user-level tool for extracting SSD internal properties such as the page size, write buffer size, etc. Queenie issues carefully designed I/O workloads and analyzes the latency distribution to speculate the hidden properties of an SSD. Results are gathered in a dataset named Kelpie, which is also made public for academic purposes. We report interesting findings from Kelpie in our [SYSTOR 22 paper](https://dl.acm.org/doi/abs/10.1145/3534056.3534940).  

This repository contains the pseudo-code (`pseudo_code.pdf`) and the source code (`src/`) of Queenie's probing functions. Some probing data from Kelpie is also made public (`data/`).

