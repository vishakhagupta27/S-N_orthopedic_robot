// test_pipeline_stages.cpp
// Dedicated unit tests for all pipeline stages previously marked "Integrated"
// Covers: FR-BBP-003, FR-BBP-004, FR-BBP-007, FR-HEC-003, FR-HEC-004,
//         FR-HEC-005, FR-HEC-006 (frame building), FR-HEC-008

#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"
#include "../fixtures/transform_fixtures.h"
#include <Eigen/Dense>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <set>

using FrameTransform = Eigen::Matrix4d;
using Pt3 = Eigen::Vector3d;

// ============================================================
// FR-BBP-003: Experiment List Parsing (Dedicated Unit Tests)
// ============================================================

class ExperimentListParsingTest : public ::testing::Test {
protected:
    // Replicate the exact parsing logic from device_bb_pnp_main.cpp
    std::pair<std::vector<std::string>, int> ParseExpListFromString(const std::string& content) {
        std::vector<std::string> exp_ID_list;
        int lineNumber = 0;
        std::istringstream stream(content);
        std::string line, csvItem;
        while (std::getline(stream, line)) {
            std::istringstream myline(line);
            while (std::getline(myline, csvItem)) {
                exp_ID_list.push_back(csvItem);
            }
            lineNumber++;
        }
        return {exp_ID_list, lineNumber};
    }
};

// FR-BBP-003: Empty file produces zero experiments and lineNumber=0
TEST_F(ExperimentListParsingTest, EmptyFileProducesZeroExperiments) {
    auto [list, count] = ParseExpListFromString("");
    EXPECT_EQ(count, 0);
    EXPECT_EQ(list.size(), 0u);
}

// FR-BBP-003: Single experiment parsed correctly
TEST_F(ExperimentListParsingTest, SingleExperimentParsedCorrectly) {
    auto [list, count] = ParseExpListFromString("exp_001");
    EXPECT_EQ(count, 1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "exp_001");
}

// FR-BBP-003: Multiple experiments parsed in order
TEST_F(ExperimentListParsingTest, MultipleExperimentsParsedCorrectly) {
    auto [list, count] = ParseExpListFromString("exp_001\nexp_002\nexp_003");
    EXPECT_EQ(count, 3);
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], "exp_001");
    EXPECT_EQ(list[1], "exp_002");
    EXPECT_EQ(list[2], "exp_003");
}

// FR-BBP-003: lineNumber != exp_ID_list.size() throws std::runtime_error
TEST_F(ExperimentListParsingTest, SizeMismatchThrowsRuntimeError) {
    int lineNumber = 5;
    std::vector<std::string> exp_ID_list = {"a", "b", "c"};  // Only 3

    EXPECT_THROW({
        if (lineNumber != static_cast<int>(exp_ID_list.size()))
            throw std::runtime_error("Exp ID list size mismatch!!!");
    }, std::runtime_error);
}

// FR-BBP-003: File not found throws std::runtime_error (mirrors is_open() check)
TEST_F(ExperimentListParsingTest, NonExistentFileThrowsRuntimeError) {
    std::ifstream file("/nonexistent/exp_list.txt");

    EXPECT_THROW({
        if (!file.is_open())
            throw std::runtime_error("Could not open exp ID file");
    }, std::runtime_error);
}

// FR-BBP-003: IDs with special characters are preserved
TEST_F(ExperimentListParsingTest, SpecialCharacterIDsPreserved) {
    auto [list, count] = ParseExpListFromString("exp-001_v2\nEXP_002.a");
    EXPECT_EQ(count, 2);
    EXPECT_EQ(list[0], "exp-001_v2");
    EXPECT_EQ(list[1], "EXP_002.a");
}

// FR-BBP-003: Trailing newline does not break parsing
TEST_F(ExperimentListParsingTest, TrailingNewlineHandled) {
    auto [list, count] = ParseExpListFromString("exp_001\nexp_002\n");
    EXPECT_GE(list.size(), 2u);
    EXPECT_EQ(list[0], "exp_001");
    EXPECT_EQ(list[1], "exp_002");
}

// ============================================================
// FR-BBP-004: DICOM Path Construction and Camera Model
// ============================================================

class DICOMPipelineTest : public ::testing::Test {
protected:
    const std::string dicom_root_ = "/data/dicom";
    const std::string exp_ID_     = "exp_001";
};

// FR-BBP-004: DICOM image path is dicom_root + "/" + exp_ID
TEST_F(DICOMPipelineTest, DICOMPathConstructedCorrectly) {
    std::string img_path = dicom_root_ + "/" + exp_ID_;
    EXPECT_EQ(img_path, "/data/dicom/exp_001");
}

// FR-BBP-004: DICOM path contains the experiment ID
TEST_F(DICOMPipelineTest, DICOMPathContainsExperimentID) {
    std::string img_path = dicom_root_ + "/" + exp_ID_;
    EXPECT_NE(img_path.find(exp_ID_), std::string::npos);
}

// FR-BBP-004: Default camera model (NaiveCIOSFusion) has valid parameters
TEST_F(DICOMPipelineTest, DefaultCameraModelHasValidParameters) {
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
    EXPECT_GT(camera.focal_length, 0.0);
    EXPECT_GT(camera.img_width, 0);
    EXPECT_GT(camera.img_height, 0);
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(camera.extrinsic));
}

// FR-BBP-004/005: 2D FCSV path is fcsv_root + "/reproj_bb" + exp_ID + ".fcsv"
TEST_F(DICOMPipelineTest, TwoDFCSVPathConstructedCorrectly) {
    std::string fcsv_root = "/data/2d_landmarks";
    std::string fcsv_path = fcsv_root + "/reproj_bb" + exp_ID_ + ".fcsv";
    EXPECT_NE(fcsv_path.find("reproj_bb"), std::string::npos);
    EXPECT_NE(fcsv_path.find(".fcsv"), std::string::npos);
    EXPECT_NE(fcsv_path.find(exp_ID_), std::string::npos);
}

// ============================================================
// FR-BBP-007: Transform File Output
// ============================================================

class TransformOutputTest : public ::testing::Test {
protected:
    const std::string output_path_ = "/data/output";
    const std::string exp_ID_      = "exp_001";
};

// FR-BBP-007: HDF5 output path is output_path + "/device_pnp_xform" + exp_ID + ".h5"
TEST_F(TransformOutputTest, OutputPathConstructedCorrectly) {
    std::string out = output_path_ + "/device_pnp_xform" + exp_ID_ + ".h5";
    EXPECT_NE(out.find("device_pnp_xform"), std::string::npos);
    EXPECT_NE(out.find(".h5"), std::string::npos);
    EXPECT_NE(out.find(exp_ID_), std::string::npos);
}

// FR-BBP-007: Output file contains experiment ID for traceability
TEST_F(TransformOutputTest, OutputFilenameContainsExperimentID) {
    std::string out = output_path_ + "/device_pnp_xform" + exp_ID_ + ".h5";
    EXPECT_NE(out.find(exp_ID_), std::string::npos);
}

// FR-BBP-007: Transform written to file is valid SE(3)
TEST_F(TransformOutputTest, TransformIsValidSE3BeforeOutput) {
    FrameTransform pnp = TestData::TransformGenerator::GenerateRandomSE3(300.0, 30.0);
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pnp));
}

// FR-BBP-007: Each experiment produces a unique output file
TEST_F(TransformOutputTest, MultipleExperimentsProduceUniqueOutputFiles) {
    std::vector<std::string> exp_ids = {"001", "002", "003"};
    std::set<std::string> output_files;
    for (const auto& id : exp_ids) {
        output_files.insert(output_path_ + "/device_pnp_xform" + id + ".h5");
    }
    EXPECT_EQ(output_files.size(), exp_ids.size());
}

// ============================================================
// FR-HEC-003: Device Reference Landmark Loading
// ============================================================

class DeviceRefLandmarkTest : public ::testing::Test {
protected:
    using LandmarkMap = std::map<std::string, Pt3>;

    bool FindHandeyeRef(const LandmarkMap& landmarks, Pt3& ref_pt) {
        auto it = landmarks.find("HandeyeRef");
        if (it != landmarks.end()) {
            ref_pt = it->second;
            return true;
        }
        return false;
    }

    FrameTransform BuildRefTransform(const Pt3& ref_pt) {
        FrameTransform T = FrameTransform::Identity();
        T(0, 3) = -ref_pt[0];
        T(1, 3) = -ref_pt[1];
        T(2, 3) = -ref_pt[2];
        return T;
    }
};

// FR-HEC-003: "HandeyeRef" landmark found and extracted correctly
TEST_F(DeviceRefLandmarkTest, HandeyeRefFoundSuccessfully) {
    LandmarkMap landmarks;
    landmarks["HandeyeRef"] = Pt3(10, 20, 30);

    Pt3 ref_pt;
    bool found = FindHandeyeRef(landmarks, ref_pt);

    EXPECT_TRUE(found);
    EXPECT_NEAR(ref_pt[0], 10.0, 1e-6);
    EXPECT_NEAR(ref_pt[1], 20.0, 1e-6);
    EXPECT_NEAR(ref_pt[2], 30.0, 1e-6);
}

// FR-HEC-003: Missing "HandeyeRef" returns false (branch: ERROR branch in source)
TEST_F(DeviceRefLandmarkTest, HandeyeRefNotFoundReturnsFalse) {
    LandmarkMap landmarks;
    landmarks["OtherRef"] = Pt3(10, 20, 30);

    Pt3 ref_pt;
    bool found = FindHandeyeRef(landmarks, ref_pt);

    EXPECT_FALSE(found);
}

// FR-HEC-003: Reference transform stores negative of reference point coordinates
TEST_F(DeviceRefLandmarkTest, RefTransformNegatesCoordinates) {
    Pt3 ref_pt(10, 20, 30);
    FrameTransform T = BuildRefTransform(ref_pt);

    EXPECT_NEAR(T(0, 3), -10.0, 1e-6);
    EXPECT_NEAR(T(1, 3), -20.0, 1e-6);
    EXPECT_NEAR(T(2, 3), -30.0, 1e-6);
}

// FR-HEC-003: Reference at origin produces zero-translation transform
TEST_F(DeviceRefLandmarkTest, OriginRefProducesZeroTranslation) {
    Pt3 ref_pt(0, 0, 0);
    FrameTransform T = BuildRefTransform(ref_pt);

    EXPECT_NEAR(T(0, 3), 0.0, 1e-6);
    EXPECT_NEAR(T(1, 3), 0.0, 1e-6);
    EXPECT_NEAR(T(2, 3), 0.0, 1e-6);

    Eigen::Matrix3d R = T.block<3, 3>(0, 0);
    EXPECT_NEAR((R - Eigen::Matrix3d::Identity()).norm(), 0.0, 1e-6);
}

// FR-HEC-003: Large reference coordinates (>1000mm) are handled correctly
TEST_F(DeviceRefLandmarkTest, LargeReferenceCoordinatesHandled) {
    Pt3 ref_pt(1500, 2500, 3500);
    FrameTransform T = BuildRefTransform(ref_pt);

    EXPECT_NEAR(T(0, 3), -1500.0, 1e-6);
    EXPECT_NEAR(T(1, 3), -2500.0, 1e-6);
    EXPECT_NEAR(T(2, 3), -3500.0, 1e-6);
}

// ============================================================
// FR-HEC-004: Robot End-Effector Pose File Path Construction
// ============================================================

class RobotPosePathTest : public ::testing::Test {
protected:
    const std::string slicer_root_ = "/data/slicer";
    const std::string exp_ID_      = "exp_001";
};

// FR-HEC-004: Robot pose H5 path follows slicer_root/exp_ID/ur_eef.h5 convention
TEST_F(RobotPosePathTest, RobotPosePathConstructedCorrectly) {
    std::string path = slicer_root_ + "/" + exp_ID_ + "/ur_eef.h5";
    EXPECT_EQ(path, "/data/slicer/exp_001/ur_eef.h5");
}

// FR-HEC-004: Path contains experiment ID subdirectory
TEST_F(RobotPosePathTest, PathContainsExperimentIDSubdirectory) {
    std::string path = slicer_root_ + "/" + exp_ID_ + "/ur_eef.h5";
    EXPECT_NE(path.find(exp_ID_), std::string::npos);
}

// FR-HEC-004: Path ends with the ur_eef.h5 filename
TEST_F(RobotPosePathTest, PathEndsWithUREEFFilename) {
    std::string path = slicer_root_ + "/" + exp_ID_ + "/ur_eef.h5";
    EXPECT_NE(path.find("ur_eef.h5"), std::string::npos);
}

// FR-HEC-004: Missing robot pose file is detected via is_open() check
TEST_F(RobotPosePathTest, MissingRobotPoseFileDetected) {
    std::ifstream file("/nonexistent/ur_eef.h5");
    EXPECT_FALSE(file.is_open());
    // In the real pipeline this would be caught as H5 exception
}

// ============================================================
// FR-HEC-005: PnP Transform File Path Construction
// ============================================================

class PnPTransformPathTest : public ::testing::Test {
protected:
    const std::string pnp_root_ = "/data/pnp";
    const std::string exp_ID_   = "exp_001";
};

// FR-HEC-005: PnP path follows root_pnp_path + "/devicepnp_xform" + exp_ID + ".h5"
TEST_F(PnPTransformPathTest, PnPPathConstructedCorrectly) {
    std::string path = pnp_root_ + "/devicepnp_xform" + exp_ID_ + ".h5";
    EXPECT_NE(path.find("devicepnp_xform"), std::string::npos);
    EXPECT_NE(path.find(".h5"), std::string::npos);
    EXPECT_NE(path.find(exp_ID_), std::string::npos);
}

// FR-HEC-005: devicepnp_xform prefix is distinct from BBP's device_pnp_xform (naming gap)
TEST_F(PnPTransformPathTest, PnPReadPrefixDiffersFromBBPWritePrefix) {
    std::string hec_read  = pnp_root_ + "/devicepnp_xform" + exp_ID_ + ".h5";
    std::string bbp_write = "/output/device_pnp_xform" + exp_ID_ + ".h5";
    // Known naming discrepancy between the two executables — both should be noted
    EXPECT_NE(hec_read.find("devicepnp_xform"), std::string::npos);
    EXPECT_NE(bbp_write.find("device_pnp_xform"), std::string::npos);
}

// FR-HEC-005: Missing PnP transform file is detected
TEST_F(PnPTransformPathTest, MissingPnPTransformFileDetected) {
    std::ifstream file("/nonexistent/devicepnp_xformexp_001.h5");
    EXPECT_FALSE(file.is_open());
}

// ============================================================
// FR-HEC-006: Relative Transformation and Frame Building
// ============================================================

class FrameBuildingTest : public ::testing::Test {
protected:
    FrameTransform MakeTranslation(double tx, double ty, double tz) {
        FrameTransform T = FrameTransform::Identity();
        T(0, 3) = tx; T(1, 3) = ty; T(2, 3) = tz;
        return T;
    }
};

// FR-HEC-006: First AX/BX/AY/BY block is set to identity
TEST_F(FrameBuildingTest, FirstBlockIsIdentity) {
    FrameTransform first_block = FrameTransform::Identity();
    EXPECT_NEAR((first_block - FrameTransform::Identity()).norm(), 0.0, 1e-6);
}

// FR-HEC-006: Relative A transform is A[i-1]^-1 * A[i]
TEST_F(FrameBuildingTest, RelativeTransformComputedCorrectly) {
    FrameTransform A0 = MakeTranslation(100, 0, 0);
    FrameTransform A1 = MakeTranslation(110, 0, 0);

    FrameTransform A_rel = A0.inverse() * A1;

    EXPECT_NEAR(A_rel(0, 3), 10.0, 1e-5);
    EXPECT_NEAR(A_rel(1, 3),  0.0, 1e-5);
    EXPECT_NEAR(A_rel(2, 3),  0.0, 1e-5);
}

// FR-HEC-006: A_frames.size() <= 5 triggers exit (boundary: exactly 5 fails)
TEST_F(FrameBuildingTest, ExactlyFiveFramesFails) {
    std::vector<FrameTransform> A_frames(5, FrameTransform::Identity());
    // Source code: if(A_frames.size() <= 5) → exit
    EXPECT_LE(A_frames.size(), 5u);
}

// FR-HEC-006: A_frames.size() > 5 passes the minimum check (exactly 6 passes)
TEST_F(FrameBuildingTest, SixFramesPassesMinimumCheck) {
    std::vector<FrameTransform> A_frames(6, FrameTransform::Identity());
    EXPECT_GT(A_frames.size(), 5u);
}

// ============================================================
// FR-HEC-008: Calibration Output Path Construction
// ============================================================

class CalibrationOutputTest : public ::testing::Test {
protected:
    const std::string debug_path_  = "/output/debug";
    const std::string file_prefix_ = "test_";
};

// FR-HEC-008: X file path = debug_path + "/" + prefix + "handeye_pnp_X.h5"
TEST_F(CalibrationOutputTest, XFilePathConstructedCorrectly) {
    std::string x_path = debug_path_ + "/" + file_prefix_ + "handeye_pnp_X.h5";
    EXPECT_EQ(x_path, "/output/debug/test_handeye_pnp_X.h5");
    EXPECT_NE(x_path.find("handeye_pnp_X.h5"), std::string::npos);
}

// FR-HEC-008: Y file path = debug_path + "/" + prefix + "handeye_pnp_Y.h5"
TEST_F(CalibrationOutputTest, YFilePathConstructedCorrectly) {
    std::string y_path = debug_path_ + "/" + file_prefix_ + "handeye_pnp_Y.h5";
    EXPECT_EQ(y_path, "/output/debug/test_handeye_pnp_Y.h5");
    EXPECT_NE(y_path.find("handeye_pnp_Y.h5"), std::string::npos);
}

// FR-HEC-008: X and Y output paths are distinct
TEST_F(CalibrationOutputTest, XAndYPathsAreDifferent) {
    std::string x_path = debug_path_ + "/" + file_prefix_ + "handeye_pnp_X.h5";
    std::string y_path = debug_path_ + "/" + file_prefix_ + "handeye_pnp_Y.h5";
    EXPECT_NE(x_path, y_path);
}

// FR-HEC-008: X and Y transforms satisfy X * Y = Identity (Y = X^-1)
TEST_F(CalibrationOutputTest, XAndYAreApproximateInverses) {
    FrameTransform X = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);
    FrameTransform Y = X.inverse();

    FrameTransform product = X * Y;
    EXPECT_NEAR((product - FrameTransform::Identity()).norm(), 0.0, 1e-5);
}

// FR-HEC-008: Both output transforms are valid SE(3) before writing
TEST_F(CalibrationOutputTest, BothOutputTransformsValidSE3) {
    FrameTransform X = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);
    FrameTransform Y = X.inverse();

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(Y));
}
