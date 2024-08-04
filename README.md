# graphISO
Graph Isomorphism

Tools to compute the Maximum Common Subgraph between two graphs
Please refer to the following paper for a complete description of the tools:

S. Quer, A. Marcelli, G. Squillero, "The Maximum Common Subgraph Problem: A Parallel and Multi-Engine Approach," Computation, Publisher MDPI AG, Switzerland, Vol. 8, N. 2, Article Number. 48, 2020, pp. 1-29, https://www.mdpi.com/2079-3197/8/2/48/pdf, ISSN 2079-3197, DOI: 10.3390/computation8020048.

The available tools extend the work by

Ciaran McCreesh, Patrick Prosser, and James Trimble, "A Partitioning Algorithm for Maximum Common Subgraph Problems," Proceedings of the 26th International Joint Conference on Artificial Intelligence, IJCAI'17, 2017, 712--719, Melbourne, Australia, AAAI Press, http://dl.acm.org/citation.cfm?id=3171642.3171744, isbn: 978-0-9992411-0-3

in the following directions:

- Version v1 is a sequential re-implementation of the original code in C language. This code constitutes the starting version for all subsequent implementations, as it is more coherent with all following requirements, modifications, and choices.

- Version v2 is a C multi-thread version logically derived from v1.

- Version v3 is an intermediate CPU single-thread implementation that removes recursion and decreases memory usage. It is logically the starting point for comparison for the following two versions.

- Version v4 is a CPU multi-thread implementation based on the same principles of the following CUDA implementation.

- Version v5 is the GPU many-thread implementation based on v3 and v4.

Please refer to
https://arxiv.org/abs/1908.06418
for further details.

Copyright © 2019 Stefano Quer <stefano.quer@polito.it> 

graphISO is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
