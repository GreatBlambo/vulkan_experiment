// TODO: Make all this work

void test_memory_arena() {
    Memory::Arena main_arena(GB(8));

    int* test0 = main_arena.push<int>(1000000);

    test0[999999] = 1234;
    LOG_DEBUG("%d", test0[999999]);

    int* test1 = main_arena.push<int>(1000000);
    int* test2 = main_arena.push<int>(3000000);
    int* test3 = main_arena.push<int>(10000);

    ASSERT(test3 < test2);
    ASSERT(test3 > test0);
    ASSERT(test3 > test1);

    main_arena.reset();
    
    int* n_test0 = main_arena.push<int>(1000000);
    int* n_test1 = main_arena.push<int>(1000000);
    int* n_test2 = main_arena.push<int>(3000000);
    int* n_test3 = main_arena.push<int>(10000);

    ASSERT(n_test0 == test0);
    ASSERT(n_test1 == test1);
    ASSERT(n_test2 == test2);
    ASSERT(n_test3 == test3);

    Memory::Arena secondary_arena(KB(1000), &main_arena);
    int* test4 = secondary_arena.push<int>(1200);
    test4[1199] = 1234;
    LOG_DEBUG("%d", test4[1199]);

    int* test5 = secondary_arena.push<int>(1000000);
    ASSERT(test5 > test0);
    ASSERT(test5 > test1);
    ASSERT(test5 > test2);
    ASSERT(test5 > test3);

}
