find_package(Threads REQUIRED)

add_executable(test_bounded_queue
    test_bounded_queue.cpp
)
target_link_libraries(test_bounded_queue
    PRIVATE
        visionflow_core
        Threads::Threads
)
visionflow_enable_warnings(test_bounded_queue)

add_test(NAME bounded_queue COMMAND test_bounded_queue)
# A broken wakeup shows up as a hang; fail it instead of waiting forever.
set_tests_properties(bounded_queue PROPERTIES TIMEOUT 30)
