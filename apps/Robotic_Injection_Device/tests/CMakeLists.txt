# CMakeLists.txt for Robotic Injection Device Unit Tests
cmake_minimum_required(VERSION 3.10)
project(RoboticInjectionDevice_Tests)

# Find GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Find required packages
find_package(Eigen3 REQUIRED)
find_package(ITK REQUIRED)
find_package(HDF5 REQUIRED)
find_package(cisst REQUIRED)

include(${ITK_USE_FILE})
include(${CISST_USE_FILE})

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../device_bb_pnp
    ${CMAKE_CURRENT_SOURCE_DIR}/../device_handeye_calibration
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    ${EIGEN3_INCLUDE_DIR}
    ${HDF5_INCLUDE_DIRS}
)

# Enable testing
enable_testing()
include(GoogleTest)

# Compiler flags for coverage
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")

# Mock library
add_library(test_mocks STATIC
    mocks/xreg_mocks.cpp
    mocks/file_io_mocks.cpp
)
target_link_libraries(test_mocks
    ${GTEST_LIBRARIES}
    ${XREG_EXE_LIBS_TO_LINK}
)

# Test fixtures library
add_library(test_fixtures STATIC
    fixtures/test_data_generator.cpp
    fixtures/transform_fixtures.cpp
)
target_link_libraries(test_fixtures
    ${GTEST_LIBRARIES}
    ${EIGEN3_LIBS}
)

# Unit Tests - Device BB PnP
add_executable(test_device_bb_pnp
    unit/test_device_bb_pnp_cmdline.cpp
    unit/test_device_bb_pnp_fcsv.cpp
    unit/test_device_bb_pnp_coords.cpp
    unit/test_device_bb_pnp_pnp.cpp
)
target_link_libraries(test_device_bb_pnp
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    test_mocks
    test_fixtures
    ${XREG_EXE_LIBS_TO_LINK}
    pthread
    gcov
)
gtest_discover_tests(test_device_bb_pnp)

# Unit Tests - Device Hand-Eye Calibration
add_executable(test_device_handeye
    unit/test_handeye_cmdline.cpp
    unit/test_handeye_conversion.cpp
    unit/test_handeye_solver.cpp
    unit/test_handeye_validation.cpp
)
target_link_libraries(test_device_handeye
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    test_mocks
    test_fixtures
    ${XREG_EXE_LIBS_TO_LINK}
    cisstCommon
    cisstVector
    pthread
    gcov
)
gtest_discover_tests(test_device_handeye)

# Integration Tests
add_executable(test_integration
    integration/test_bb_pnp_workflow.cpp
    integration/test_handeye_workflow.cpp
    integration/test_cross_module.cpp
)
target_link_libraries(test_integration
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    test_mocks
    test_fixtures
    ${XREG_EXE_LIBS_TO_LINK}
    cisstCommon
    cisstVector
    pthread
    gcov
)
gtest_discover_tests(test_integration)

# Edge Case Tests
add_executable(test_edge_cases
    unit/test_edge_cases.cpp
)
target_link_libraries(test_edge_cases
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    test_mocks
    test_fixtures
    pthread
    gcov
)
gtest_discover_tests(test_edge_cases)

# Error Handling Tests
add_executable(test_error_handling
    unit/test_error_handling.cpp
)
target_link_libraries(test_error_handling
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    test_mocks
    test_fixtures
    pthread
    gcov
)
gtest_discover_tests(test_error_handling)

# Coverage target
add_custom_target(coverage
    COMMAND lcov --capture --directory . --output-file coverage.info
    COMMAND lcov --remove coverage.info '/usr/*' '*/test/*' --output-file coverage_filtered.info
    COMMAND genhtml coverage_filtered.info --output-directory coverage_report
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating code coverage report"
)

# Add custom test target
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
    DEPENDS test_device_bb_pnp test_device_handeye test_integration test_edge_cases test_error_handling
)
