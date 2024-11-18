# Dynamic Memory Allocation with Weight Balanced Heap Tree
This is a variant of Brent's algorithm.[1] The differences are as below.
1. Eliminating additional tree utilizing payload area.
2. The tree grows or shrinks dynamically, always keeping balance using weight of sub trees.
2. Eliminating linked list, using paging.
3. A multi-thread implementation.
The time complexity is O(log n).

# Internal Fragmentation vs External Fragmentation
External fragmentation is chosen over internal fragmentation, because external fragmentation can become available again upon coalesing, while internal fragmentation will not be available until the object is freed.

# Future Work
1. to implement reallof, reallocarry, aligned_alloc, posix_memalign, valloc, and memalign
2. to implement cache table and compare the performance
3. to implement multi-thread B-Tree-First-Fit, which is another variant of Brent's algorithm.[1]

# Reference
1. R. P. Brent (July 1989). "Efficient Implementation of the First-Fit Strategy for Dynamic Storage Allocation". ACM Transactions on i’rograrrlming &guages and Systems, Vol. 11, No. 3, July 1989. p. 388.
