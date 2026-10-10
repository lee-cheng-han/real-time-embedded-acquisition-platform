cmake_minimum_required(VERSION 3.20)
project(drats VERSION 0.1.0 LANGUAGES C)

option(DRATS_ENABLE_SANITIZERS "Enable host address/undefined sanitizers" ON)
option(DRATS_BUILD_TESTS "Build host tests" ON)

add_library(drats_core STATIC
    common/src/acquisition.c
    common/src/block_pool.c
    common/src/config_store.c
    common/src/crc32c.c
    common/src/fault_manager.c
    common/src/handle_queue.c
    common/src/image.c
    common/src/protocol.c
    common/src/supervisor.c
    common/src/update_manager.c
)
target_include_directories(drats_core PUBLIC common/include)
target_compile_features(drats_core PUBLIC c_std_17)
target_compile_options(drats_core PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror
)

if(DRATS_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(drats_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(drats_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(drats_sim simulation/main.c)
target_link_libraries(drats_sim PRIVATE drats_core)
target_compile_options(drats_sim PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(DRATS_BUILD_TESTS)
    include(CTest)
    function(drats_add_test name source)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE tests)
        target_link_libraries(${name} PRIVATE drats_core)
        target_compile_options(${name} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror
        )
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES TIMEOUT 30)
    endfunction()
    drats_add_test(unit_core tests/unit/test_core.c)
    drats_add_test(unit_reliability tests/unit/test_reliability.c)
    drats_add_test(unit_protocol_fuzz tests/unit/test_protocol_fuzz.c)
    drats_add_test(integration_pipeline tests/integration/test_pipeline.c)
    drats_add_test(fault_injection tests/fault_injection/test_faults.c)
    drats_add_test(endurance_pipeline tests/endurance/test_endurance.c)
    set_tests_properties(endurance_pipeline PROPERTIES LABELS endurance TIMEOUT 60)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME integration_host
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_host.py
                     $<TARGET_FILE:drats_sim>)
    set_tests_properties(integration_host PROPERTIES TIMEOUT 30)
endif()
