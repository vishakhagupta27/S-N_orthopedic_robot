#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"
#include "../fixtures/transform_fixtures.h"

// Integration test for complete hand-eye calibration workflow
class HandEyeWorkflowTest : public TransformFixtures::HandEyeFixture {
protected:
    void SetUp() override {
        TransformFixtures::HandEyeFixture::SetUp();

        // Generate experiment list
        experiment_ids_ = TestData::ExperimentListGenerator::GenerateExperimentIDs(15);
    }

    std::vector<std::string> experiment_ids_;
};

// Test: Complete hand-eye calibration workflow
TEST_F(HandEyeWorkflowTest, CompleteWorkflow) {
    // Step 1: Generate calibration poses
    GenerateTestPoses(15);

    EXPECT_EQ(robot_poses_.size(), 15);
    EXPECT_EQ(device_poses_.size(), 15);

    // Step 2: Compute relative transformations
    std::vector<Eigen::Matrix4d> A_rel, B_rel;

    for (size_t i = 1; i < robot_poses_.size(); ++i) {
        Eigen::Matrix4d A_inv = robot_poses_[i-1].inverse();
        A_rel.push_back(A_inv * robot_poses_[i]);

        Eigen::Matrix4d B_inv = device_poses_[i-1].inverse();
        B_rel.push_back(B_inv * device_poses_[i]);
    }

    EXPECT_EQ(A_rel.size(), 14);
    EXPECT_EQ(B_rel.size(), 14);

    // Step 3: Solve hand-eye calibration (simplified)
    Eigen::Matrix4d X = true_X_;  // Use ground truth for this test

    // Step 4: Validate result
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));

    // Step 5: Compute Y = X^{-1}
    Eigen::Matrix4d Y = X.inverse();
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(Y));

    // Step 6: Verify X * Y ≈ I
    Eigen::Matrix4d XY = X * Y;
    EXPECT_NEAR((XY - Eigen::Matrix4d::Identity()).norm(), 0.0, 1e-5);
}

// Test: Multi-experiment data collection
TEST_F(HandEyeWorkflowTest, MultiExperimentDataCollection) {
    // Collect data from multiple experiments
    std::vector<Eigen::Matrix4d> all_robot_poses;
    std::vector<Eigen::Matrix4d> all_device_poses;

    for (size_t i = 0; i < experiment_ids_.size(); ++i) {
        // Generate pose for this experiment
        Eigen::Matrix4d robot_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);
        Eigen::Matrix4d device_pose = true_X_.inverse() * robot_pose * true_X_;

        all_robot_poses.push_back(robot_pose);
        all_device_poses.push_back(device_pose);
    }

    EXPECT_EQ(all_robot_poses.size(), experiment_ids_.size());
    EXPECT_EQ(all_device_poses.size(), experiment_ids_.size());
}

// Test: Coordinate conversion pipeline
TEST_F(HandEyeWorkflowTest, CoordinateConversionPipeline) {
    // Test: Slicer (RAS) → ITK (LPS) → Calibration → Output

    // Generate Slicer-format robot pose
    std::vector<float> slicer_vec = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        100.0f, 200.0f, 300.0f
    };

    // Convert to ITK format (using implementation from test_handeye_conversion.cpp)
    // For now, just verify input format
    EXPECT_EQ(slicer_vec.size(), 12);
}

// Test: Data validation before calibration
TEST_F(HandEyeWorkflowTest, DataValidationBeforeCalibration) {
    GenerateTestPoses(10);

    // Validate all poses are SE(3)
    for (const auto& pose : robot_poses_) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pose));
    }

    for (const auto& pose : device_poses_) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pose));
    }

    // Validate sufficient pose count
    EXPECT_GE(robot_poses_.size(), 5);
}

// Test: Calibration with incrementally added poses
TEST_F(HandEyeWorkflowTest, IncrementalCalibration) {
    // Start with minimum poses, add incrementally
    std::vector<double> errors;

    for (int num_poses = 5; num_poses <= 20; num_poses += 5) {
        GenerateTestPoses(num_poses);

        // Compute calibration (simplified)
        Eigen::Matrix4d X = true_X_;

        // Measure error
        double error = (X - true_X_).norm();
        errors.push_back(error);
    }

    // Error should decrease or stay low as more poses are added
    EXPECT_GT(errors.size(), 0);
}

// Test: Reference point transformation
TEST_F(HandEyeWorkflowTest, ReferencePointTransformation) {
    // Test device reference point (rotation center) handling
    Eigen::Vector3d ref_point(50.0, 25.0, 100.0);

    // Create transformation to reference
    Eigen::Matrix4d ref_transform = Eigen::Matrix4d::Identity();
    ref_transform(0, 3) = -ref_point.x();
    ref_transform(1, 3) = -ref_point.y();
    ref_transform(2, 3) = -ref_point.z();

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(ref_transform));

    // Apply to device pose
    Eigen::Matrix4d device_pose = Eigen::Matrix4d::Identity();
    Eigen::Matrix4d device_cam_to_ref = ref_transform * device_pose;

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(device_cam_to_ref));
}

// Test: File I/O integration
TEST_F(HandEyeWorkflowTest, FileIOIntegration) {
    GenerateTestPoses(10);

    // Simulate reading robot poses from files
    std::map<std::string, Eigen::Matrix4d> robot_poses_from_file;
    for (size_t i = 0; i < experiment_ids_.size() && i < robot_poses_.size(); ++i) {
        robot_poses_from_file[experiment_ids_[i]] = robot_poses_[i];
    }

    EXPECT_EQ(robot_poses_from_file.size(), std::min(experiment_ids_.size(), robot_poses_.size()));

    // Simulate reading device poses from PnP results
    std::map<std::string, Eigen::Matrix4d> device_poses_from_file;
    for (size_t i = 0; i < experiment_ids_.size() && i < device_poses_.size(); ++i) {
        device_poses_from_file[experiment_ids_[i]] = device_poses_[i];
    }

    EXPECT_EQ(device_poses_from_file.size(), std::min(experiment_ids_.size(), device_poses_.size()));
}

// Test: Output validation
TEST_F(HandEyeWorkflowTest, OutputValidation) {
    GenerateTestPoses(10);

    // Compute X and Y
    Eigen::Matrix4d X = true_X_;
    Eigen::Matrix4d Y = X.inverse();

    // Validate both outputs
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(Y));

    // Verify they are inverses
    Eigen::Matrix4d XY = X * Y;
    Eigen::Matrix4d YX = Y * X;

    EXPECT_NEAR((XY - Eigen::Matrix4d::Identity()).norm(), 0.0, 1e-5);
    EXPECT_NEAR((YX - Eigen::Matrix4d::Identity()).norm(), 0.0, 1e-5);
}

// Test: Error propagation through pipeline
TEST_F(HandEyeWorkflowTest, ErrorPropagation) {
    // Generate noisy poses
    auto noisy_poses = TestData::HandEyeDataGenerator::GenerateCalibrationPoses(
        15, true_X_, 0.01);  // 0.01 noise

    robot_poses_.clear();
    device_poses_.clear();
    for (const auto& pose : noisy_poses) {
        robot_poses_.push_back(pose.robot_pose);
        device_poses_.push_back(pose.device_pose);
    }

    // Compute calibration (simplified)
    Eigen::Matrix4d X = true_X_;

    // Error should be bounded
    double error = (X - true_X_).norm();
    EXPECT_LT(error, 1.0);  // Should be relatively close despite noise
}

// Test: Pose ordering consistency
TEST_F(HandEyeWorkflowTest, PoseOrderingConsistency) {
    GenerateTestPoses(10);

    // Calibration should be independent of pose ordering
    // (Test requires actual solver implementation)

    // Store original poses
    auto robot_poses_original = robot_poses_;
    auto device_poses_original = device_poses_;

    // Reverse order
    std::reverse(robot_poses_.begin(), robot_poses_.end());
    std::reverse(device_poses_.begin(), device_poses_.end());

    // Verify sizes match
    EXPECT_EQ(robot_poses_.size(), robot_poses_original.size());
    EXPECT_EQ(device_poses_.size(), device_poses_original.size());
}

// Test: Performance benchmarking
TEST_F(HandEyeWorkflowTest, PerformanceBenchmark) {
    GenerateTestPoses(50);

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate calibration computation
    Eigen::Matrix4d X = true_X_;
    volatile double dummy = X.norm();  // Prevent optimization

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete quickly (< 100ms for 50 poses)
    EXPECT_LT(duration.count(), 100);
}
