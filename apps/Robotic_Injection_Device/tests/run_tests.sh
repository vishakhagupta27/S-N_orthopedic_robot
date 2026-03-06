#!/bin/bash
# Test execution script for Robotic Injection Device
# Usage: ./run_tests.sh [options]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default options
BUILD_DIR="build"
COVERAGE=false
VERBOSE=false
FILTER=""
PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--coverage)
            COVERAGE=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -c, --coverage          Generate code coverage report"
            echo "  -v, --verbose           Verbose test output"
            echo "  -f, --filter PATTERN    Run tests matching PATTERN"
            echo "  -b, --build-dir DIR     Build directory (default: build)"
            echo "  -j, --jobs N            Number of parallel build jobs (default: $(nproc))"
            echo "  -h, --help              Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                      # Run all tests"
            echo "  $0 --coverage           # Run tests with coverage"
            echo "  $0 --filter=PnP*        # Run only PnP tests"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Robotic Injection Device - Test Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
if $COVERAGE; then
    cmake -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-g -O0 --coverage" \
          .. || exit 1
else
    cmake -DCMAKE_BUILD_TYPE=Release .. || exit 1
fi

# Build tests
echo -e "${YELLOW}Building tests...${NC}"
cmake --build . -j"$PARALLEL_JOBS" || exit 1

echo ""
echo -e "${GREEN}Build successful!${NC}"
echo ""

# Run tests
echo -e "${YELLOW}Running tests...${NC}"
echo ""

TEST_EXECUTABLES=(
    "test_device_bb_pnp"
    "test_device_handeye"
    "test_integration"
    "test_edge_cases"
    "test_error_handling"
)

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

for TEST_EXE in "${TEST_EXECUTABLES[@]}"; do
    if [ ! -f "$TEST_EXE" ]; then
        echo -e "${YELLOW}Warning: $TEST_EXE not found, skipping...${NC}"
        continue
    fi

    echo -e "${BLUE}Running $TEST_EXE...${NC}"

    TEST_CMD="./$TEST_EXE"

    if [ -n "$FILTER" ]; then
        TEST_CMD="$TEST_CMD --gtest_filter=$FILTER"
    fi

    if $VERBOSE; then
        TEST_CMD="$TEST_CMD --gtest_print_time=1"
    fi

    # Run test and capture output
    if $TEST_CMD; then
        echo -e "${GREEN}✓ $TEST_EXE passed${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ $TEST_EXE failed${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi

    echo ""
done

TOTAL_TESTS=$((PASSED_TESTS + FAILED_TESTS))

# Generate coverage report if requested
if $COVERAGE && [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${YELLOW}Generating coverage report...${NC}"

    # Capture coverage data
    lcov --capture --directory . --output-file coverage.info || {
        echo -e "${RED}Failed to capture coverage data${NC}"
        exit 1
    }

    # Filter out system and test files
    lcov --remove coverage.info '/usr/*' '*/test/*' '*/mocks/*' '*/fixtures/*' \
         --output-file coverage_filtered.info || {
        echo -e "${RED}Failed to filter coverage data${NC}"
        exit 1
    }

    # Generate HTML report
    genhtml coverage_filtered.info --output-directory coverage_report || {
        echo -e "${RED}Failed to generate HTML coverage report${NC}"
        exit 1
    }

    echo -e "${GREEN}Coverage report generated: coverage_report/index.html${NC}"

    # Display coverage summary
    echo ""
    echo -e "${BLUE}Coverage Summary:${NC}"
    lcov --summary coverage_filtered.info
fi

# Print summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total test suites: ${TOTAL_TESTS}"
echo -e "Passed: ${GREEN}${PASSED_TESTS}${NC}"
echo -e "Failed: ${RED}${FAILED_TESTS}${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests passed! ✓${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed! ✗${NC}"
    exit 1
fi
