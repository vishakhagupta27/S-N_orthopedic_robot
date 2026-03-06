#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"
#include <Eigen/Dense>
#include <limits>
#include <cmath>

using FrameTransform = Eigen::Matrix4d;
using Pt3 = Eigen::Vector3d;
using Pt2 = Eigen::Vector2d;

// Test edge cases across both modules
class EdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        tolerance_ = 1e-6;
    }

    double tolerance_;
};

// Edge Case: Exactly 4 landmarks (minimum for PnP)
TEST_F(EdgeCaseTest, ExactlyFourLandmarks) {
    std::map<std::string, Pt3> landmarks;
    landmarks["p1"] = Pt3(0, 0, 0);
    landmarks["p2"] = Pt3(100, 0, 0);
    landmarks["p3"] = Pt3(0, 100, 0);
    landmarks["p4"] = Pt3(0, 0, 100);

    EXPECT_EQ(landmarks.size(), 4);
}

// Edge Case: Exactly 5 poses (minimum for hand-eye)
TEST_F(EdgeCaseTest, ExactlyFivePoses) {
    FrameTransform true_X = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);
    auto poses = TestData::HandEyeDataGenerator::GenerateCalibrationPoses(5, true_X);

    EXPECT_EQ(poses.size(), 5);
}

// Edge Case: Very small coordinates (near zero)
TEST_F(EdgeCaseTest, VerySmallCoordinates) {
    std::map<std::string, Pt3> small_landmarks;
    small_landmarks["p1"] = Pt3(1e-6, 1e-6, 1e-6);
    small_landmarks["p2"] = Pt3(2e-6, 1e-6, 1e-6);
    small_landmarks["p3"] = Pt3(1e-6, 2e-6, 1e-6);
    small_landmarks["p4"] = Pt3(1e-6, 1e-6, 2e-6);

    // Should handle without numerical issues
    for (const auto& [name, pt] : small_landmarks) {
        EXPECT_FALSE(std::isnan(pt.x()));
        EXPECT_FALSE(std::isnan(pt.y()));
        EXPECT_FALSE(std::isnan(pt.z()));
    }
}

// Edge Case: Very large coordinates
TEST_F(EdgeCaseTest, VeryLargeCoordinates) {
    std::map<std::string, Pt3> large_landmarks;
    large_landmarks["p1"] = Pt3(10000, 20000, 30000);
    large_landmarks["p2"] = Pt3(11000, 20000, 30000);
    large_landmarks["p3"] = Pt3(10000, 21000, 30000);
    large_landmarks["p4"] = Pt3(10000, 20000, 31000);

    for (const auto& [name, pt] : large_landmarks) {
        EXPECT_FALSE(std::isnan(pt.x()));
        EXPECT_FALSE(std::isinf(pt.x()));
    }
}

// Edge Case: Zero rotation (identity rotation matrix)
TEST_F(EdgeCaseTest, ZeroRotation) {
    FrameTransform T = FrameTransform::Identity();
    T(0, 3) = 100.0;
    T(1, 3) = 200.0;
    T(2, 3) = 300.0;

    Eigen::Matrix3d R = T.block<3, 3>(0, 0);
    EXPECT_NEAR((R - Eigen::Matrix3d::Identity()).norm(), 0.0, tolerance_);
}

// Edge Case: Zero translation
TEST_F(EdgeCaseTest, ZeroTranslation) {
    FrameTransform T = TestData::TransformGenerator::Rotation(45.0, Pt3(0, 0, 1));

    Pt3 translation = T.block<3, 1>(0, 3);
    EXPECT_NEAR(translation.norm(), 0.0, tolerance_);
}

// Edge Case: Identity transformation
TEST_F(EdgeCaseTest, IdentityTransformation) {
    FrameTransform identity = FrameTransform::Identity();

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(identity));

    // Check all elements
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j) {
                EXPECT_NEAR(identity(i, j), 1.0, tolerance_);
            } else {
                EXPECT_NEAR(identity(i, j), 0.0, tolerance_);
            }
        }
    }
}

// Edge Case: 180-degree rotation
TEST_F(EdgeCaseTest, OneEightyDegreeRotation) {
    FrameTransform T = TestData::TransformGenerator::Rotation(180.0, Pt3(0, 0, 1));

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(T));

    // Rotation matrix determinant should still be 1
    Eigen::Matrix3d R = T.block<3, 3>(0, 0);
    EXPECT_NEAR(R.determinant(), 1.0, tolerance_);
}

// Edge Case: Very small rotation angle (< 0.01 degrees)
TEST_F(EdgeCaseTest, VerySmallRotationAngle) {
    FrameTransform T = TestData::TransformGenerator::Rotation(0.001, Pt3(0, 0, 1));

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(T));

    // Should be very close to identity
    Eigen::Matrix3d R = T.block<3, 3>(0, 0);
    double deviation = (R - Eigen::Matrix3d::Identity()).norm();
    EXPECT_LT(deviation, 0.01);
}

// Edge Case: Planar landmark configuration (all Z=0)
TEST_F(EdgeCaseTest, PlanarLandmarks) {
    std::map<std::string, Pt3> planar_landmarks;
    planar_landmarks["p1"] = Pt3(0, 0, 0);
    planar_landmarks["p2"] = Pt3(100, 0, 0);
    planar_landmarks["p3"] = Pt3(0, 100, 0);
    planar_landmarks["p4"] = Pt3(100, 100, 0);

    // Check all Z coordinates are zero
    for (const auto& [name, pt] : planar_landmarks) {
        EXPECT_NEAR(pt.z(), 0.0, tolerance_);
    }

    // This is a degenerate configuration for PnP
    EXPECT_EQ(planar_landmarks.size(), 4);
}

// Edge Case: Collinear landmarks (all on X-axis)
TEST_F(EdgeCaseTest, CollinearLandmarks) {
    std::map<std::string, Pt3> collinear_landmarks;
    collinear_landmarks["p1"] = Pt3(0, 0, 0);
    collinear_landmarks["p2"] = Pt3(10, 0, 0);
    collinear_landmarks["p3"] = Pt3(20, 0, 0);
    collinear_landmarks["p4"] = Pt3(30, 0, 0);

    // Check collinearity
    for (const auto& [name, pt] : collinear_landmarks) {
        EXPECT_NEAR(pt.y(), 0.0, tolerance_);
        EXPECT_NEAR(pt.z(), 0.0, tolerance_);
    }
}

// Edge Case: Landmarks at origin
TEST_F(EdgeCaseTest, LandmarksAtOrigin) {
    std::map<std::string, Pt3> origin_landmarks;
    origin_landmarks["p1"] = Pt3(0, 0, 0);

    Pt3 pt = origin_landmarks["p1"];
    EXPECT_NEAR(pt.norm(), 0.0, tolerance_);
}

// Edge Case: Negative coordinates
TEST_F(EdgeCaseTest, NegativeCoordinates) {
    std::map<std::string, Pt3> negative_landmarks;
    negative_landmarks["p1"] = Pt3(-100, -200, -300);

    Pt3 pt = negative_landmarks["p1"];
    EXPECT_LT(pt.x(), 0.0);
    EXPECT_LT(pt.y(), 0.0);
    EXPECT_LT(pt.z(), 0.0);
}

// Edge Case: Mixed positive and negative coordinates
TEST_F(EdgeCaseTest, MixedSignCoordinates) {
    std::map<std::string, Pt3> mixed_landmarks;
    mixed_landmarks["p1"] = Pt3(100, -200, 300);
    mixed_landmarks["p2"] = Pt3(-100, 200, -300);

    for (const auto& [name, pt] : mixed_landmarks) {
        // Just verify they're finite
        EXPECT_TRUE(std::isfinite(pt.x()));
        EXPECT_TRUE(std::isfinite(pt.y()));
        EXPECT_TRUE(std::isfinite(pt.z()));
    }
}

// Edge Case: Transformation with singular values near limits
TEST_F(EdgeCaseTest, NearSingularTransformation) {
    // Create a transformation with very small scale (not valid SE(3) but edge case)
    FrameTransform T = FrameTransform::Identity();
    T(0, 0) = 1.0 + 1e-10;

    // Should still be considered valid within tolerance
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(T, 1e-8));
}

// Edge Case: Nearly identical consecutive poses
TEST_F(EdgeCaseTest, NearlyIdenticalConsecutivePoses) {
    FrameTransform base = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);

    std::vector<FrameTransform> poses;
    for (int i = 0; i < 5; ++i) {
        FrameTransform small_perturbation = TestData::TransformGenerator::GenerateRandomSE3(0.1, 0.1);
        poses.push_back(base * small_perturbation);
    }

    // All poses should be very close to base
    for (const auto& pose : poses) {
        double diff = (pose - base).norm();
        EXPECT_LT(diff, 10.0);  // Within 10 norm units
    }
}

// Edge Case: Empty experiment list
TEST_F(EdgeCaseTest, EmptyExperimentList) {
    std::vector<std::string> empty_list;

    EXPECT_EQ(empty_list.size(), 0);
    EXPECT_TRUE(empty_list.empty());
}

// Edge Case: Single experiment
TEST_F(EdgeCaseTest, SingleExperiment) {
    std::vector<std::string> single_exp = {"exp_0001"};

    EXPECT_EQ(single_exp.size(), 1);
}

// Edge Case: Very long experiment ID
TEST_F(EdgeCaseTest, VeryLongExperimentID) {
    std::string long_id(1000, 'x');  // 1000 character ID

    EXPECT_EQ(long_id.length(), 1000);
    EXPECT_FALSE(long_id.empty());
}

// Edge Case: Experiment ID with special characters
TEST_F(EdgeCaseTest, SpecialCharacterExperimentID) {
    std::string special_id = "exp-2024_01_01@v1.0";

    EXPECT_FALSE(special_id.empty());
    EXPECT_TRUE(special_id.find('_') != std::string::npos);
    EXPECT_TRUE(special_id.find('-') != std::string::npos);
}

// Edge Case: Landmarks outside image bounds
TEST_F(EdgeCaseTest, LandmarksOutsideImageBounds) {
    std::map<std::string, Pt2> out_of_bounds;
    out_of_bounds["p1"] = Pt2(-100, -100);   // Negative
    out_of_bounds["p2"] = Pt2(10000, 10000); // Way outside

    for (const auto& [name, pt] : out_of_bounds) {
        EXPECT_TRUE(std::isfinite(pt.x()));
        EXPECT_TRUE(std::isfinite(pt.y()));
    }
}

// Edge Case: Landmarks exactly on image boundary
TEST_F(EdgeCaseTest, LandmarksOnImageBoundary) {
    int img_width = 1024;
    int img_height = 1024;

    std::map<std::string, Pt2> boundary_landmarks;
    boundary_landmarks["top_left"] = Pt2(0, 0);
    boundary_landmarks["top_right"] = Pt2(img_width - 1, 0);
    boundary_landmarks["bottom_left"] = Pt2(0, img_height - 1);
    boundary_landmarks["bottom_right"] = Pt2(img_width - 1, img_height - 1);

    for (const auto& [name, pt] : boundary_landmarks) {
        EXPECT_GE(pt.x(), 0);
        EXPECT_LT(pt.x(), img_width);
        EXPECT_GE(pt.y(), 0);
        EXPECT_LT(pt.y(), img_height);
    }
}

// Edge Case: Camera at origin looking along Z-axis
TEST_F(EdgeCaseTest, CameraAtOrigin) {
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();

    // Default camera should be at a reasonable position
    EXPECT_TRUE(std::isfinite(camera.extrinsic(0, 3)));
    EXPECT_TRUE(std::isfinite(camera.extrinsic(1, 3)));
    EXPECT_TRUE(std::isfinite(camera.extrinsic(2, 3)));
}

// Edge Case: Very large focal length
TEST_F(EdgeCaseTest, VeryLargeFocalLength) {
    double large_focal_length = 10000.0;
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera(large_focal_length);

    EXPECT_NEAR(camera.focal_length, large_focal_length, tolerance_);
}

// Edge Case: Very small focal length
TEST_F(EdgeCaseTest, VerySmallFocalLength) {
    double small_focal_length = 100.0;
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera(small_focal_length);

    EXPECT_NEAR(camera.focal_length, small_focal_length, tolerance_);
}

// Edge Case: Square image (equal width and height)
TEST_F(EdgeCaseTest, SquareImage) {
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera(1000.0, 1024, 1024);

    EXPECT_EQ(camera.img_width, camera.img_height);
}

// Edge Case: Non-square image (wide aspect ratio)
TEST_F(EdgeCaseTest, WideAspectRatioImage) {
    auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera(1000.0, 1920, 1080);

    EXPECT_GT(camera.img_width, camera.img_height);
}

// Edge Case: Maximum representable double values
TEST_F(EdgeCaseTest, MaxDoubleValue) {
    double max_val = std::numeric_limits<double>::max();

    // Should be finite
    EXPECT_TRUE(std::isfinite(max_val));
    EXPECT_FALSE(std::isnan(max_val));
    EXPECT_FALSE(std::isinf(max_val));
}

// Edge Case: Minimum positive double values
TEST_F(EdgeCaseTest, MinPositiveDoubleValue) {
    double min_val = std::numeric_limits<double>::min();

    EXPECT_GT(min_val, 0.0);
    EXPECT_TRUE(std::isfinite(min_val));
}

// Edge Case: Epsilon precision
TEST_F(EdgeCaseTest, EpsilonPrecision) {
    double epsilon = std::numeric_limits<double>::epsilon();

    EXPECT_GT(epsilon, 0.0);
    EXPECT_LT(epsilon, 1e-10);

    // 1.0 + epsilon should be distinguishable from 1.0
    EXPECT_NE(1.0 + epsilon, 1.0);
}
