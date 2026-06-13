# Tests
#   Imported by the top-level CMakeLists.txt when IMDB_BUILD_TESTS=ON.

# ---- A trivial loader smoke test (foundation task owns the real one) ----
add_executable(test_loader test_loader.cpp)
target_link_libraries(test_loader PRIVATE imdb_core GTest::gtest GTest::gtest_main)
add_test(NAME test_loader COMMAND test_loader)

# ---- MinHeap unit tests ----
add_executable(test_minheap test_minheap.cpp)
target_link_libraries(test_minheap PRIVATE imdb_core GTest::gtest GTest::gtest_main)
add_test(NAME test_minheap COMMAND test_minheap)

# ---- RedBlackTree unit tests ----
add_executable(test_rbtree test_rbtree.cpp)
target_link_libraries(test_rbtree PRIVATE imdb_core GTest::gtest GTest::gtest_main)
add_test(NAME test_rbtree COMMAND test_rbtree)
