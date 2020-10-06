# oz_libcds
Data structures (containers) supporting concurrency.

A) Lock-based containers:

0. concurrent_stack,
1. concurrent_queue,
2. concurrent_queue_fast,
3. concurrent_map,
4. concurrent_list.

B) Lock-free containers:

0. lock_free_stack_pop_count,
1. lock_free_stack_hp,
2. lock_free_stack_ref_count,
3. lock_free_queue_ref_count,
4. exp_stack_atomic_shared_ptr.

Key words: STL, data structures, containers (stack, queue, map, list), concurrency, fine-grained locking, lock-based programming, lock-free programming, lock-guards, mutex, shared_mutex, condition_variable, atomic functions, garbage collectors (pop counting, hazard pointers, reference counting, shared_ptr), memory orderings, thread_local static variables.

Literature:
Williams, A. C++ Concurrency In Action. 2nd edition. – Manning Publications, 2019. – 568 p.
