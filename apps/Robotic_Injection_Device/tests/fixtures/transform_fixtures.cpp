#include "transform_fixtures.h"
#include "test_data_generator.h"
#include <cmath>

namespace TransformFixtures {

// TransformTestFixture implementation
void TransformTestFixture::SetUp() {
    identity_ = FrameTransform::Identity();

    // Simple translation: move by (10, 20, 30)
    simple_translation_ = FrameTransform::Identity();
    simple_translation_(0, 3) = 10.0;
    simple_translation_(1, 3) = 20.0;
    simple_translation_(2, 3) = 30.0;

    // Simple rotation: 90 degrees around Z-axis
    simple_rotation_ = FrameTransform::Identity();
    simple_rotation_(0, 0) = 0.0;  simple_rotation_(0, 1) = -1.0;
    simple_rotation_(1, 0) = 1.0;  simple_rotation_(1, 1) = 0.0;

    // Complex transform: combination of rotation and translation
    complex_transform_ = TestData::TransformGenerator::GenerateRandomSE3(100.0, 30.0);
}

void TransformTestFixture::TearDown() {
    // Cleanup if needed
}

// CoordinateConversionFixture implementation
void CoordinateConversionFixture::SetUp() {
    // RAS to LPS conversion: flip X and Y
    ras_to_lps_ = FrameTransform::Identity();
    ras_to_lps_(0, 0) = -1.0;
    ras_to_lps_(1, 1) = -1.0;

    // Define test points in RAS
    points_ras_["origin"] = Pt3(0, 0, 0);
    points_ras_["positive_x"] = Pt3(10, 0, 0);
    points_ras_["positive_y"] = Pt3(0, 10, 0);
    points_ras_["positive_z"] = Pt3(0, 0, 10);
    points_ras_["arbitrary"] = Pt3(5, -3, 7);

    // Compute corresponding LPS points
    for (const auto& [name, pt_ras] : points_ras_) {
        Eigen::Vector4d pt_homog(pt_ras.x(), pt_ras.y(), pt_ras.z(), 1.0);
        Eigen::Vector4d pt_lps_homog = ras_to_lps_ * pt_homog;
        points_lps_[name] = Pt3(pt_lps_homog.x(), pt_lps_homog.y(), pt_lps_homog.z());
    }
}

// HandEyeFixture implementation
void HandEyeFixture::SetUp() {
    // Define a known ground truth X transformation
    true_X_ = TestData::TransformGenerator::GenerateRandomSE3(200.0, 45.0);
}

void HandEyeFixture::GenerateTestPoses(int num_poses) {
    robot_poses_.clear();
    device_poses_.clear();

    auto poses = TestData::HandEyeDataGenerator::GenerateCalibrationPoses(
        num_poses, true_X_, 0.0);

    for (const auto& pose : poses) {
        robot_poses_.push_back(pose.robot_pose);
        device_poses_.push_back(pose.device_pose);
    }
}

bool HandEyeFixture::VerifyCalibration(const FrameTransform& computed_X, double tolerance) {
    // Check if computed_X is close to true_X_
    double error = (computed_X - true_X_).norm();
    return error < tolerance;
}

} // namespace TransformFixtures
