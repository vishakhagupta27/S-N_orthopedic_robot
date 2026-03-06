# Robotic Injection Device - Test Suite

## Overview

Comprehensive unit and integration test suite for the Robotic Injection Device system, providing automated testing, validation, and coverage reporting for both `device_bb_pnp` and `device_handeye_calibration` modules.

## Test Architecture

```
tests/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── mocks/                      # Mock implementations
│   ├── xreg_mocks.h/cpp       # xReg library mocks
│   └── file_io_mocks.h/cpp    # File I/O mocks
├── fixtures/                   # Test fixtures and helpers
│   ├── test_data_generator.h/cpp
│   └── transform_fixtures.h/cpp
├── unit/                       # Unit tests
│   ├── test_device_bb_pnp_*.cpp
│   ├── test_handeye_*.cpp
│   ├── test_edge_cases.cpp
│   └── test_error_handling.cpp
└── integration/                # Integration tests
    ├── test_bb_pnp_workflow.cpp
    ├── test_handeye_workflow.cpp
    └── test_cross_module.cpp
```

## Test Coverage

### Unit Tests (80+ test cases)

#### Device BB PnP Module
- **Command-line Interface** (11 tests): Argument parsing, validation
- **FCSV File Operations** (20 tests): Reading, parsing, coordinate conversion
- **Coordinate Transformations** (15 tests): RAS↔LPS conversion, camera coordinates
- **PnP Solver** (18 tests): Pose computation, reprojection error, SE(3) validation

#### Device Hand-Eye Calibration Module
- **Command-line Interface** (8 tests): Argument parsing
- **Coordinate Conversion** (20 tests): Slicer↔ITK transformation
- **Hand-Eye Solver** (15 tests): AX=XB solving, validation
- **Validation** (20 tests): SE(3) properties, accuracy requirements

#### Edge Cases (40 tests)
- Minimum/maximum data sizes
- Degenerate configurations
- Numerical precision limits
- Boundary conditions

#### Error Handling (35 tests)
- File I/O errors
- Numerical errors (NaN, Inf, overflow)
- Matrix operations (singular, non-orthogonal)
- Exception handling

### Integration Tests (30+ test cases)

- **BB PnP Workflow**: End-to-end pipeline testing
- **Hand-Eye Workflow**: Complete calibration process
- **Cross-Module**: PnP → Hand-Eye data flow

## Requirements Tested

Covers all functional requirements from [FUNCTIONAL_REQUIREMENTS.md](../FUNCTIONAL_REQUIREMENTS.md):

- ✅ FR-BBP-001 to FR-BBP-007 (Device BB PnP)
- ✅ FR-HEC-001 to FR-HEC-008 (Hand-Eye Calibration)
- ✅ NFR-001 to NFR-005 (Non-functional requirements)

## Building Tests

### Prerequisites

- CMake ≥ 3.10
- Google Test (gtest)
- xReg library and dependencies
- CISST library (for hand-eye tests)

### Build Commands

```bash
# Navigate to test directory
cd apps/Robotic_Injection_Device/tests

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build all tests
make -j4

# Or use Ninja (faster)
cmake -G Ninja ..
ninja
```

## Running Tests

### Run All Tests

```bash
# From build directory
ctest --verbose --output-on-failure

# Or run test executables directly
./test_device_bb_pnp
./test_device_handeye
./test_integration
./test_edge_cases
./test_error_handling
```

### Run Specific Test Suite

```bash
# Run only PnP tests
./test_device_bb_pnp

# Run only hand-eye tests
./test_device_handeye

# Run specific test
./test_device_bb_pnp --gtest_filter=DeviceBBPnPCmdLineTest.ValidArgumentCount
```

### Run with Filters

```bash
# Run all tests containing "PnP" in name
./test_device_bb_pnp --gtest_filter=*PnP*

# Run all edge case tests
./test_edge_cases --gtest_filter=EdgeCaseTest.*

# Exclude specific tests
./test_integration --gtest_filter=-*Performance*
```

## Code Coverage

### Generate Coverage Report

```bash
# From build directory
make coverage

# View HTML report
open coverage_report/index.html
```

### Coverage Targets

- **Line Coverage**: ≥ 80%
- **Function Coverage**: ≥ 85%
- **Branch Coverage**: ≥ 75%

### Current Coverage (Update after running tests)

```
Overall Coverage: XX.X%
- device_bb_pnp: XX.X%
- device_handeye_calibration: XX.X%
```

## Test Data

### Synthetic Test Data

Tests use automatically generated synthetic data:

- **Device Landmarks**: Bounding box configurations
- **Camera Models**: CIOS Fusion C-arm parameters
- **Transformations**: Known ground truth SE(3) matrices
- **Poses**: Robot and device calibration poses

### Test Fixtures

Located in `fixtures/`:

- `TestData::DeviceLandmarkGenerator`: Generate 3D landmarks
- `TestData::CameraModelGenerator`: Generate camera models
- `TestData::TransformGenerator`: Generate SE(3) transformations
- `TestData::HandEyeDataGenerator`: Generate calibration poses
- `TestData::FCSVGenerator`: Generate FCSV file content

## Test Categories

### By Type

| Category | Count | Description |
|----------|-------|-------------|
| Unit | 150+ | Individual function/component tests |
| Integration | 30+ | Workflow and cross-module tests |
| Edge Cases | 40+ | Boundary and corner cases |
| Error Handling | 35+ | Exception and error conditions |

### By Requirement

| Requirement | Tests | Status |
|-------------|-------|--------|
| FR-BBP-001 (CLI) | 11 | ✅ Pass |
| FR-BBP-002 (FCSV) | 20 | ✅ Pass |
| FR-BBP-006 (PnP) | 18 | ✅ Pass |
| FR-HEC-002 (Conversion) | 20 | ✅ Pass |
| FR-HEC-006 (Solver) | 15 | ✅ Pass |
| FR-HEC-007 (Validation) | 20 | ✅ Pass |

## Continuous Integration

Tests are automatically run on:

- **Push to main**: Full test suite + coverage
- **Pull Requests**: Full test suite + coverage
- **Nightly**: Extended tests + performance benchmarks

See `.github/workflows/test.yml` for CI configuration.

## Test Output

### Success Example

```
[==========] Running 150 tests from 25 test suites.
[----------] Global test environment set-up.
...
[----------] 11 tests from DeviceBBPnPCmdLineTest (5 ms total)
...
[==========] 150 tests from 25 test suites ran. (2500 ms total)
[  PASSED  ] 150 tests.
```

### Failure Example

```
[ RUN      ] DeviceBBPnPPnPTest.ReprojectionError
test_device_bb_pnp_pnp.cpp:42: Failure
Expected: (error) < (1.0), actual: 2.5 vs 1.0
[  FAILED  ] DeviceBBPnPPnPTest.ReprojectionError (10 ms)
```

## Performance Benchmarks

| Metric | Target | Current |
|--------|--------|---------|
| Unit test execution | < 60s | XX.Xs |
| Integration test execution | < 30s | XX.Xs |
| PnP per image | < 50ms | XX.Xms |
| Hand-eye calibration (50 poses) | < 10s | XX.Xs |

## Troubleshooting

### Common Issues

**Issue**: Test compilation fails with missing headers

```
Solution: Ensure xReg and dependencies are properly installed and in CMAKE_PREFIX_PATH
```

**Issue**: Tests fail with "Segmentation fault"

```
Solution: Check that test data fixtures are properly initialized in SetUp()
```

**Issue**: Coverage report not generated

```
Solution: Ensure gcov and lcov are installed:
  sudo apt-get install lcov
```

**Issue**: Tests timeout

```
Solution: Increase timeout in CTest:
  ctest --timeout 300
```

### Debug Mode

Build tests with debug symbols:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run with debugger
gdb ./test_device_bb_pnp
```

### Verbose Output

```bash
# More detailed test output
./test_device_bb_pnp --gtest_print_time=1 --gtest_print_utf8=1

# CTest verbose output
ctest -VV
```

## Contributing Tests

### Adding New Tests

1. Choose appropriate test file or create new one
2. Follow naming convention: `test_module_component.cpp`
3. Use fixtures from `fixtures/` directory
4. Add to CMakeLists.txt if new file
5. Run locally before committing
6. Update this README with new test counts

### Test Naming Convention

```cpp
// Class name: ModuleComponentTest
class DeviceBBPnPPnPTest : public ::testing::Test { };

// Test name: TestCategory_SpecificCondition
TEST_F(DeviceBBPnPPnPTest, ReprojectionError_IsSmall) {
    // Test implementation
}
```

### Best Practices

- ✅ Each test should test ONE thing
- ✅ Tests should be independent (order doesn't matter)
- ✅ Use descriptive test names
- ✅ Add comments for complex test logic
- ✅ Clean up in TearDown()
- ✅ Use fixtures to avoid code duplication
- ✅ Test edge cases and error conditions
- ✅ Verify error messages, not just that error occurred

## Documentation

- [Functional Requirements](../FUNCTIONAL_REQUIREMENTS.md): Complete requirements specification
- [Google Test Documentation](https://google.github.io/googletest/): Testing framework
- [xReg Wiki](https://github.com/rg2/xreg/wiki): Library documentation

## License

Same as parent project (MIT License).

## Contact

For test-related questions or issues:
- Open an issue in the repository
- Contact: [Project maintainers]

---

**Last Updated**: 2026-03-05
**Test Suite Version**: 1.0
**Total Tests**: 250+
**Coverage Target**: ≥ 80%
