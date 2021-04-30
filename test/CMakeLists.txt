set(TARGET_NAME "${PROJECT_NAME}-test")

set(SOURCES
    grid_order.cpp
    main.cpp
)

add_executable("${TARGET_NAME}" "${SOURCES}")

# target

target_link_libraries("${TARGET_NAME}" roq-client::roq-client gtest_main absl::hash absl::raw_hash_set)

target_compile_features("${TARGET_NAME}" PUBLIC cxx_std_17)

# run

add_test(NAME "${TARGET_NAME}" COMMAND "${TARGET_NAME}")
