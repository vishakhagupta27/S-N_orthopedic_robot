#ifndef TRANSFORM_FIXTURES_H
#define TRANSFORM_FIXTURES_H

#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <map>
#include <string>

namespace TransformFixtures {

using FrameTransform = Eigen::Matrix4d;
using Pt3 = Eigen::Vector3d;

// Base fixture for transformation tests
class TransformTestFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Common transformations
    FrameTransform identity_;
    FrameTransform simple_translation_;
    FrameTransform simple_rotation_;
    FrameTransform complex_transform_;

    // Tolerance for floating-point comparisons
    static constexpr double kTolerance = 1e-6;
};

// Fixture for coordinate conversion tests
class CoordinateConversionFixture : public ::testing::Test {
protected:
    void SetUp() override;

    // RAS to LPS conversion matrix
    FrameTransform ras_to_lps_;

    // Test points in both coordinate systems
    std::map<std::string, Pt3> points_ras_;
    std::map<std::string, Pt3> points_lps_;
};

// Fixture for hand-eye calibration tests
class HandEyeFixture : public ::testing::Test {
protected:
    void SetUp() override;

    // Known ground truth X transformation
    FrameTransform true_X_;

    // Generate test poses
    void GenerateTestPoses(int num_poses);

    // Verify calibration result
    bool VerifyCalibration(const FrameTransform& computed_X, double tolerance);

    std::vector<FrameTransform> robot_poses_;
    std::vector<FrameTransform> device_poses_;
};

} // namespace TransformFixtures

#endif // TRANSFORM_FIXTURES_H
