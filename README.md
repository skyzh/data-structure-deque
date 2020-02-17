# data-structure-deque

A deque of O(sqrt(n)) complexity on access, insert and remove, with an optimization for O(nlogn) access based on fenwick tree.

You may found the optimized version [here](https://github.com/skyzh/data-structure-deque/blob/master/deque_fenwick_tree_vector.hpp).
This one is four times faster than [Sqrt Vector](https://github.com/skyzh/data-structure-deque/blob/master/deque_sqrt_vector.cpp)
version on large data.

This repo is migrated from my [GitHub gist](https://gist.github.com/skyzh/2597b532ad191036ae4a6dc785859e5b).

## Failed Attempts and Other Implementations

* [Linked List](https://github.com/skyzh/data-structure-deque/blob/master/deque_linkedlist.cpp): O(n) access, O(1) insert & remove
* [Ring Buffer](https://github.com/skyzh/data-structure-deque/blob/master/deque_ring_buffer.cpp): O(1) access, O(n) insert & remove (Like the one bundled with GNU C++ STL)
* [Sqrt Vector](https://github.com/skyzh/data-structure-deque/blob/master/deque_sqrt_vector.cpp): O(sqrt(n)) access, O(sqrt(n)) insert & remove
* [Chunk Vector](https://github.com/skyzh/data-structure-deque/blob/master/deque_vector_chunk.cpp): O(n/chunk_size) access, O(n) insert & move

## Related Works

Another data-structure project I've done is a [B+ Tree](https://github.com/skyzh/BPlusTree) built from scratch.
This B+ tree supports on-disk persistence, on-demand paging and LRU. I've written several unit tests for it.
