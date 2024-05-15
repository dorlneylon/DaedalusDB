set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE BENCH  CONFIGURE_DEPENDS  ${SOURCES} ./*.hpp ./*.cpp)
add_executable(${PROJECT_ARTIFACT_PERFOMANCE_BENCH}  ${BENCH})

target_include_directories(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} PRIVATE "../src/mem")
target_include_directories(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} PRIVATE "../src/db_struct")
target_include_directories(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} PRIVATE "../src/util")
target_include_directories(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} PRIVATE "../src/type_system")


target_link_libraries(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} benchmark)
target_include_directories(${PROJECT_ARTIFACT_PERFOMANCE_BENCH} PRIVATE "../benchmark/include/benchmark")


# add_test(NAME TypeTests COMMAND ${PROJECT_ARTIFACT_PERFOMANCE_BENCH} )
