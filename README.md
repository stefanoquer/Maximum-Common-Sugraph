# graphISO
Graph Isomorphism

This repositpry stores some programs to compute the Maximum Common Subgraph between two graphs.
The code in written in C/C++ language.
Please refer to the following paper for a complete description of the tools:

S. Quer, A. Marcelli, G. Squillero, "The Maximum Common Subgraph Problem: A Parallel and Multi-Engine Approach," Computation, Publisher MDPI AG, Switzerland, Vol. 8, N. 2, Article Number. 48, 2020, pp. 1-29, https://www.mdpi.com/2079-3197/8/2/48/pdf, ISSN 2079-3197, DOI: 10.3390/computation8020048.

The available tools extend the work by

Ciaran McCreesh, Patrick Prosser, and James Trimble, "A Partitioning Algorithm for Maximum Common Subgraph Problems," Proceedings of the 26th International Joint Conference on Artificial Intelligence, IJCAI'17, 2017, 712--719, Melbourne, Australia, AAAI Press, http://dl.acm.org/citation.cfm?id=3171642.3171744, isbn: 978-0-9992411-0-3

in the following directions:

- Version v1 is a sequential re-implementation of the original code in C language. This code constitutes the starting version for all subsequent implementations, as it is more coherent with all the following requirements, modifications, and choices.

- Version v2 is a C multi-thread version logically derived from v1.

- Version v3 is an intermediate CPU single-thread implementation that removes recursion and decreases memory usage. It is logically the starting point for the comparison of the following two versions.

- Version v4 is a CPU multi-thread implementation based on the same principles as the following CUDA implementation.

- Version v5 is the GPU many-thread implementation based on v3 and v4.

All versions should have a small online help that can be activated with the option "--help":

-c, --connected            Solve max common CONNECTED subgraph problem
-l, --lad                  Read LAD format
-q, --quiet                Quiet output
-t, --timeout=timeout      Set timeout of TIMEOUT seconds
-v, --verbose              Verbose output
-?, --help                 Give this help list
    --usage                Give a short usage message

The tools should accept graphs in different formats, i.e., at least in bin aty and ladder format. Here is an example of how to run version v1 on the graph pair {mcs10_r02_s20.A00, mcs10_r02_s20.B00}:

$ ./v1 -v mcs10_r02_s20.A00 mcs10_r02_s20.B00
20 vertices
new Incumbent: |0 0|
new Incumbent: |0 0| |1 3|
new Incumbent: |0 0| |1 3| |11 8|
new Incumbent: |0 0| |1 3| |11 8| |19 18|
new Incumbent: |0 0| |1 3| |11 8| |19 18| |9 2|
new Incumbent: |0 0| |1 3| |11 8| |19 18| |9 2| |4 4|
new Incumbent: |0 0| |1 3| |11 8| |19 18| |9 2| |4 4| |13 5|
new Incumbent: |0 0| |1 3| |11 8| |19 18| |9 2| |4 4| |13 5| |6 6|
new Incumbent: |0 0| |1 3| |11 8| |19 18| |18 2| |10 14| |4 6| |15 11| |12 5|
new Incumbent: |0 0| |1 3| |11 13| |19 17| |2 16| |7 2| |5 12| |12 5| |13 1| |18 11|
new Incumbent: |0 0| |1 13| |19 17| |16 16| |6 5| |13 2| |14 18| |8 4| |12 7| |9 11| |17 19|
Solution size 11
(0 -> 0) (1 -> 13) (6 -> 5) (8 -> 4) (9 -> 11) (12 -> 7) (13 -> 2) (14 -> 18) (16 -> 16) (17 -> 19) (19 -> 17)
Final Size 11 -  Final Time 0000.1508333300

Please refer to
https://arxiv.org/abs/1908.06418
for further details.

Copyright © 2019 Stefano Quer <stefano.quer@polito.it> 

graphISO is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
