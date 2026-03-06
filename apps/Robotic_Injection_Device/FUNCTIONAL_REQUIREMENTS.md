# Functional Requirements Document
## Robotic Injection Device - Unit Testing Requirements

**Generated:** 2026-03-05
**Version:** 1.0

---

## Table of Contents
1. [System Overview](#system-overview)
2. [Module Breakdown](#module-breakdown)
3. [Functional Requirements](#functional-requirements)
4. [Non-Functional Requirements](#non-functional-requirements)
5. [Test Coverage Areas](#test-coverage-areas)

---

## 1. System Overview

The Robotic Injection Device system consists of two primary executables:
- **device_bb_pnp**: Computes device pose using Perspective-n-Point (PnP) solutions
- **device_handeye_calibration**: Performs hand-eye calibration for robot-camera coordination

### Technology Stack
- **Language**: C++11
- **Dependencies**: xReg framework, ITK, Eigen3, HDF5, CISST library
- **Build System**: CMake
- **File Formats**: FCSV (landmarks), DICOM (medical images), HDF5 (transforms)

---

## 2. Module Breakdown

### 2.1 Device BB PnP Module (`device_bb_pnp_main.cpp`)

#### Components:
- **Main Function**: Entry point and workflow orchestration
- **Command-Line Parser**: Argument processing using ProgOpts
- **File I/O Operations**: FCSV and DICOM reading, HDF5 writing
- **Coordinate Transformations**: RAS to LPS conversions
- **PnP Solver Integration**: POSIT + CMA-ES optimization
- **Camera Model**: CIOS Fusion C-arm camera modeling

#### Key Functions/Operations:
1. `main(int argc, char* argv[])`
2. Landmark file reading (3D device landmarks)
3. Experiment list parsing
4. DICOM image loading per experiment
5. 2D landmark extraction and coordinate conversion
6. PnP pose computation
7. Transform file output (HDF5)

#### Data Structures:
- `LandMap2`: 2D landmark map (string → Pt2)
- `device_3d_fcsv`: 3D landmark map (string → Pt3)
- `ProjPreProc`: Projection preprocessing
- `FrameTransform`: 4x4 homogeneous transformation matrix
- `CIOSFusionDICOMInfo`: DICOM metadata structure

---

### 2.2 Device Hand-Eye Calibration Module (`device_handeye_calibration_main.cpp`)

#### Components:
- **Main Function**: Entry point and workflow orchestration
- **Command-Line Parser**: Argument processing using ProgOpts
- **Coordinate System Converter**: Slicer (RAS) to ITK (LPS)
- **File I/O Operations**: HDF5 reading/writing, FCSV reading
- **Hand-Eye Solver**: AX=XB calibration algorithm
- **Transformation Management**: Robot and camera frame handling

#### Key Functions/Operations:
1. `main(int argc, char* argv[])`
2. `ConvertSlicerToITK(std::vector<float> slicer_vec)` → FrameTransform
3. Device reference landmark loading
4. Robot end-effector pose reading (from Slicer H5)
5. PnP transform reading
6. Relative transformation computation
7. Hand-eye calibration (BIGSS::ax_xb)
8. Result output (X and Y transforms)

#### Data Structures:
- `vctFrm4x4`: 4x4 transformation matrix (CISST format)
- `vctDoubleMat`: Double matrix for AX, BX, AY, BY
- `A_frames`, `B_frames`: Vectors of transformation frames
- `FrameTransform`: Eigen-based 4x4 matrix

---

## 3. Functional Requirements

### 3.1 Device BB PnP Requirements

#### FR-BBP-001: Command-Line Interface
**Description**: Program accepts 5 positional arguments
**Inputs**:
- `device_2d_fcsv_path`: Root path to 2D landmark annotations
- `device_3d_fcsv_path`: Path to 3D device landmark file
- `exp_list_path`: Path to experiment ID list file
- `dicom_path`: Root path to DICOM images
- `output_path`: Output directory for transforms

**Expected Behavior**:
- Validate argument count (must be exactly 5)
- Parse and store arguments correctly
- Display help message if `--help` flag provided
- Return error code 1 for invalid usage

**Error Conditions**:
- Insufficient arguments: Display usage and exit with code 1
- Invalid paths: Throw runtime_error
- Missing files: Throw runtime_error

---

#### FR-BBP-002: 3D Landmark File Reading
**Description**: Read device 3D landmarks from FCSV file
**Inputs**: FCSV file path (string)
**Outputs**: LandmarkMap (string → Pt3)

**Expected Behavior**:
- Successfully parse FCSV format
- Convert RAS coordinates to LPS
- Store landmarks in map structure

**Edge Cases**:
- Empty FCSV file
- Malformed FCSV format
- Missing landmark names
- Invalid coordinate values (NaN, Inf)

**Error Conditions**:
- File not found: Throw exception
- Parse error: Throw exception

---

#### FR-BBP-003: Experiment List Parsing
**Description**: Parse experiment ID list from text file
**Inputs**: Text file path with experiment IDs (one per line)
**Outputs**: Vector of experiment ID strings

**Expected Behavior**:
- Read file line by line
- Store each ID in vector
- Validate line count matches vector size

**Edge Cases**:
- Empty file: lineNumber = 0, empty vector
- File with blank lines
- Very long experiment IDs
- Special characters in IDs

**Error Conditions**:
- File not found: Throw runtime_error
- Size mismatch: Throw runtime_error

---

#### FR-BBP-004: DICOM Image Loading
**Description**: Load DICOM images from CIOS Fusion C-arm
**Inputs**: DICOM file path (per experiment)
**Outputs**:
- Image data (float array)
- CIOS metadata structure
- Camera model

**Expected Behavior**:
- Parse DICOM header
- Extract image pixels
- Create naive camera model from metadata
- Handle CIOS Fusion specific tags

**Edge Cases**:
- Non-standard DICOM tags
- Different image dimensions
- Compressed DICOM
- Multi-frame DICOM

**Error Conditions**:
- File not found
- Invalid DICOM format
- Missing required tags
- Unsupported compression

---

#### FR-BBP-005: 2D Landmark Extraction
**Description**: Extract 2D landmarks from FCSV and update for CIOS metadata
**Inputs**:
- 2D FCSV file path
- CIOS metadata

**Outputs**: 2D landmark map (string → Pt2)

**Expected Behavior**:
- Read FCSV file
- Convert RAS to LPS
- Update landmarks based on CIOS geometry
- Verify minimum landmark count (> 3)

**Edge Cases**:
- Exactly 4 landmarks (minimum for PnP)
- Very large number of landmarks (>100)
- Landmarks outside image bounds

**Error Conditions**:
- Less than 4 landmarks: xregASSERT fails
- File not found
- Coordinate out of range

---

#### FR-BBP-006: PnP Pose Computation
**Description**: Compute camera-to-device transformation using PnP
**Inputs**:
- Camera model
- 3D landmarks (device space)
- 2D landmarks (image space)

**Outputs**: FrameTransform (4x4 homogeneous matrix)

**Expected Behavior**:
- Run POSIT algorithm for initial estimate
- Refine with CMA-ES optimization
- Minimize reprojection error
- Return valid SE(3) transformation

**Edge Cases**:
- Planar landmark configuration
- Nearly collinear landmarks
- Large reprojection errors (>10 pixels)
- Degenerate camera geometry

**Error Conditions**:
- Optimization failure: No convergence
- Invalid transformation (non-orthogonal rotation)
- Numerical instability (determinant near 0)

---

#### FR-BBP-007: Transform File Output
**Description**: Save computed transformation to HDF5 file
**Inputs**:
- Output path
- Experiment ID
- FrameTransform

**Outputs**: HDF5 file with ITK affine transform

**Expected Behavior**:
- Create HDF5 file
- Write transform in ITK format
- Include metadata
- Handle existing files (overwrite)

**Error Conditions**:
- Directory doesn't exist
- Insufficient permissions
- Disk space exhausted
- HDF5 write error

---

### 3.2 Device Hand-Eye Calibration Requirements

#### FR-HEC-001: Command-Line Interface
**Description**: Program accepts 6 positional arguments
**Inputs**:
- `root_debug_path`: Debug output directory
- `root_slicer_path`: Slicer H5 file root
- `root_pnp_path`: PnP transform root
- `deviceref_fcsv_path`: Device reference landmark
- `exp_list_path`: Experiment list file
- `file_prefix`: Output filename prefix

**Expected Behavior**:
- Validate argument count
- Parse arguments correctly
- Display help if requested

**Error Conditions**:
- Wrong argument count: Exit with code 1

---

#### FR-HEC-002: Slicer Coordinate Conversion
**Description**: Convert Slicer (RAS) transforms to ITK (LPS)
**Function**: `ConvertSlicerToITK(std::vector<float> slicer_vec)`
**Inputs**: 12-element float vector (3x4 matrix flattened)
**Outputs**: FrameTransform (4x4 homogeneous matrix)

**Expected Behavior**:
- Parse 12 elements into 3x4 matrix
- Add bottom row [0, 0, 0, 1]
- Apply RAS→LPS conversion: [-1,0,0; 0,-1,0; 0,0,1]
- Transform translation component correctly
- Return valid SE(3) transformation

**Mathematical Requirements**:
```
RAS2LPS = diag([-1, -1, 1, 1])
ITK_xform = RAS2LPS * Slicer_matrix * RAS2LPS
Translation correction:
  tmp1 = R[0,:] · t
  tmp2 = R[1,:] · t
  tmp3 = R[2,:] · t
  t_new = -[tmp1, tmp2, tmp3]
```

**Edge Cases**:
- Identity transformation
- Pure rotation (no translation)
- Pure translation (no rotation)
- Very small rotation angles (<1e-6)

**Error Conditions**:
- Vector size != 12: Undefined behavior
- NaN values in input
- Inf values in input

---

#### FR-HEC-003: Device Reference Landmark Loading
**Description**: Load rotation center reference point from FCSV
**Inputs**: FCSV file path
**Outputs**:
- FrameTransform with translation to reference
- Reference point (Pt3)

**Expected Behavior**:
- Read FCSV file
- Find landmark named "HandeyeRef"
- Convert RAS to LPS
- Create transformation matrix with negative translation

**Edge Cases**:
- Multiple reference points (use first)
- Reference at origin [0,0,0]
- Very large reference coordinates (>1000mm)

**Error Conditions**:
- "HandeyeRef" not found: Error message printed
- File not found
- Invalid FCSV format

---

#### FR-HEC-004: Robot End-Effector Pose Reading
**Description**: Read robot poses from Slicer H5 files
**Inputs**: H5 file path (per experiment)
**Outputs**: FrameTransform (robot end-effector pose)

**Expected Behavior**:
- Open H5 file
- Navigate to TransformGroup/0
- Read "TranformParameters" dataset (note: typo in original)
- Convert to ITK format using ConvertSlicerToITK

**Edge Cases**:
- Multiple transform groups (use 0)
- Transform parameters with extra elements
- Missing transform group

**Error Conditions**:
- File not found
- Invalid H5 structure
- Missing TransformGroup or dataset
- H5 read error

---

#### FR-HEC-005: PnP Transform Reading
**Description**: Read PnP-computed transforms from HDF5
**Inputs**: PnP transform file path
**Outputs**: FrameTransform

**Expected Behavior**:
- Load ITK affine transform from H5
- Parse into FrameTransform
- Validate matrix structure

**Error Conditions**:
- File not found
- Invalid transform format
- Corrupted H5 file

---

#### FR-HEC-006: Relative Transformation Computation
**Description**: Compute frame pairs for hand-eye calibration
**Inputs**:
- A_frames: Robot end-effector poses
- B_frames: Device-to-camera poses

**Outputs**:
- AX, BX: Matrices for X calibration (4N×4)
- AY, BY: Matrices for Y calibration (4N×4)

**Expected Behavior**:
- Compute relative transformations between consecutive frames
- For i=1 to N-1:
  - AX[4i:4i+4, :] = A[i-1]⁻¹ * A[i]
  - BX[4i:4i+4, :] = B[i-1]⁻¹ * B[i]
  - AY[4i:4i+4, :] = A[i-1] * A[i]⁻¹
  - BY[4i:4i+4, :] = B[i-1] * B[i]⁻¹
- First block (i=0) is identity

**Mathematical Requirements**:
- Matrix inversion must be numerically stable
- Preserve SE(3) structure

**Edge Cases**:
- Exactly 5 frames (minimum required)
- Very large number of frames (>100)
- Nearly identical consecutive frames

**Error Conditions**:
- Less than 5 frames: Error + exit code 1
- Singular matrix (determinant = 0)
- Numerical instability

---

#### FR-HEC-007: Hand-Eye Calibration Solver
**Description**: Solve AX=XB hand-eye calibration
**Inputs**:
- AX, BX: Transformation matrices (4N×4)
- AY, BY: Transformation matrices (4N×4)

**Outputs**:
- X: Hand-eye transformation (robot base to camera)
- Y: Inverse hand-eye transformation

**Expected Behavior**:
- Call BIGSS::ax_xb(AX, BX, X)
- Call BIGSS::ax_xb(AY, BY, Y)
- Both should converge to valid SE(3) transformations
- X and Y should be approximate inverses

**Mathematical Properties**:
- X * Y ≈ Identity
- det(X) = det(Y) = 1
- X and Y preserve SE(3) structure

**Validation Checks**:
- Verify Y ≈ X⁻¹ (within tolerance)
- Check rotation matrix orthonormality
- Verify determinant = 1

**Error Conditions**:
- Solver doesn't converge
- Invalid output (non-SE(3))
- Inconsistent data (no solution exists)

---

#### FR-HEC-008: Calibration Result Output
**Description**: Save X and Y transforms to HDF5 files
**Inputs**:
- Output directory
- Filename prefix
- X and Y transforms

**Outputs**:
- `{prefix}handeye_pnp_X.h5`
- `{prefix}handeye_pnp_Y.h5`

**Expected Behavior**:
- Convert vctFrm4x4 to FrameTransform
- Write as ITK affine transforms
- Include metadata

**Error Conditions**:
- Write permission denied
- Disk full
- Invalid path

---

## 4. Non-Functional Requirements

### NFR-001: Performance
- **BBP**: Process 100 experiments in < 5 minutes
- **HEC**: Complete calibration with 50 frames in < 10 seconds

### NFR-002: Memory
- Peak memory usage < 2GB per process

### NFR-003: Accuracy
- **PnP**: Reprojection error < 2 pixels (RMS)
- **Hand-eye**: Calibration error < 2mm translation, < 2° rotation

### NFR-004: Robustness
- Handle missing experiments gracefully
- Recover from single-image failures
- Provide informative error messages

### NFR-005: Portability
- Compile on Windows, Linux, macOS
- Support different C-arm models via configuration

---

## 5. Test Coverage Areas

### 5.1 Unit Tests (Isolated Components)

#### Input/Output Operations
- FCSV file reading (valid, invalid, edge cases)
- DICOM loading (various formats)
- HDF5 reading/writing
- Experiment list parsing

#### Coordinate Transformations
- RAS to LPS conversion
- Slicer to ITK conversion
- Identity transform handling
- Composition and inversion

#### Data Validation
- Landmark count checks (< 4, = 4, > 4, large)
- Coordinate bounds checking
- Matrix singularity detection
- Transform validation (SE(3) properties)

#### Mathematical Operations
- Matrix inversion
- Relative pose computation
- Frame composition

---

### 5.2 Integration Tests (Component Interaction)

#### BBP Workflow
- Full pipeline: FCSV → DICOM → PnP → HDF5
- Multi-experiment batch processing
- Camera model consistency

#### HEC Workflow
- Full pipeline: Robot + PnP → Hand-eye → Output
- Frame accumulation
- Solver integration

#### Cross-Module
- BBP output → HEC input compatibility
- Coordinate system consistency

---

### 5.3 System Tests (End-to-End)

#### Synthetic Data Tests
- Generate known transforms
- Add noise at various levels
- Verify reconstruction accuracy

#### Regression Tests
- Compare with validated baseline results
- Check numerical stability over iterations

#### Error Recovery Tests
- Missing files
- Corrupted data
- Invalid inputs

---

### 5.4 Edge Case Tests

#### Minimal Data
- Exactly 4 landmarks for PnP
- Exactly 5 frames for hand-eye

#### Degenerate Cases
- Planar landmarks
- Collinear points
- Identity transformations

#### Extreme Values
- Very large coordinates (>10,000mm)
- Very small angles (<0.01°)
- Near-singular configurations

---

### 5.5 Error Handling Tests

#### File System Errors
- Non-existent paths
- Permission denied
- Disk full

#### Data Format Errors
- Invalid FCSV syntax
- Corrupted DICOM
- Malformed HDF5

#### Numerical Errors
- NaN propagation
- Inf handling
- Division by zero

---

## 6. Test Data Requirements

### 6.1 Synthetic Test Data
- Known landmark positions (3D)
- Projected landmarks (2D) with known camera
- Known transformations for validation
- Noise models (Gaussian, uniform)

### 6.2 Fixture Files
- Valid FCSV files (3D and 2D)
- Sample DICOM images
- Pre-computed PnP transforms
- Experiment list files
- Robot pose H5 files

### 6.3 Golden Reference Data
- Validated output transforms
- Expected reprojection errors
- Calibration accuracy metrics

---

## 7. Mock/Stub Requirements

### 7.1 External Library Mocks
- **xReg Framework Functions**:
  - ReadFCSVFileNamePtMap
  - ConvertRASToLPS
  - ReadCIOSFusionDICOMFloat
  - PnPPOSITAndReprojCMAES
  - WriteITKAffineTransform
  - ReadITKAffineTransformFromFile

- **BIGSS Library**:
  - BIGSS::ax_xb

### 7.2 File I/O Mocks
- Mock file readers (FCSV, DICOM, H5)
- Mock file writers (H5)
- Simulate file system errors

---

## 8. Test Execution Requirements

### 8.1 Test Framework
- **Recommended**: Google Test (gtest) or Catch2
- Support for fixtures, mocks, parameterized tests

### 8.2 Build Integration
- CMake test targets
- CTest integration
- Separate test executables per module

### 8.3 Continuous Integration
- Automated test execution on commit
- Coverage reporting
- Performance benchmarking

---

## 9. Acceptance Criteria

### Test Suite Completeness
- [ ] ≥ 80% code coverage
- [ ] All public functions tested
- [ ] All error paths tested
- [ ] All edge cases covered

### Test Quality
- [ ] Tests are deterministic (no flaky tests)
- [ ] Tests are independent
- [ ] Tests are maintainable
- [ ] Tests execute in < 60 seconds total

### Documentation
- [ ] Each test has clear description
- [ ] Test data is documented
- [ ] Setup/teardown is explained
- [ ] Failure modes are documented

---

## 10. Risk Assessment

### High Risk Areas
1. **PnP Solver**: Complex optimization, can fail to converge
2. **Hand-eye Calibration**: Sensitive to input quality
3. **Coordinate Transformations**: Easy to introduce sign errors
4. **File I/O**: Many failure modes

### Mitigation Strategies
- Extensive testing of mathematical operations
- Validation of transform properties (det=1, orthogonality)
- Comprehensive error handling tests
- Integration with validated reference implementations

---

## 11. Future Enhancements

### Testing Infrastructure
- Property-based testing (hypothesis-style)
- Mutation testing
- Fuzz testing for file parsers
- Performance regression tracking

### Test Coverage Expansion
- Multi-threaded execution tests
- Large-scale batch processing
- Memory leak detection (valgrind)
- Thread safety analysis

---

**END OF FUNCTIONAL REQUIREMENTS DOCUMENT**

---

## SUBMIT FOR HUMAN VERIFICATION

This functional requirements document is ready for human review and approval.

Please verify:
1. Completeness of identified requirements
2. Accuracy of functional descriptions
3. Appropriateness of edge cases and error conditions
4. Coverage of all critical functionality
5. Feasibility of proposed test approach

Once approved, unit test generation will commence.
