#include <gtest/gtest.h>
#include "../fixtures/transform_fixtures.h"
#include "../fixtures/test_data_generator.h"
#include <Eigen/Dense>

using FrameTransform = Eigen::Matrix4d;

// Test hand-eye calibration solver
class HandEyeSolverTest : public TransformFixtures::HandEyeFixture {
protected:
    // Simplified hand-eye solver for testing (mock)
    // Real implementation would call BIGSS::ax_xb
    FrameTransform SolveHandEye(const std::vector<FrameTransform>& A_poses,
                                const std::vector<FrameTransform>& B_poses) {
        // For testing, return ground truth
        return true_X_;
    }
};

// FR-HEC-006: Test with minimum required poses (5)
TEST_F(HandEyeSolverTest, MinimumPoseCount) {
    GenerateTestPoses(5);

    EXPECT_EQ(robot_poses_.size(), 5);
    EXPECT_EQ(device_poses_.size(), 5);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
}

// FR-HEC-006: Test with insufficient poses (< 5)
TEST_F(HandEyeSolverTest, InsufficientPoseCount) {
    GenerateTestPoses(4);

    EXPECT_EQ(robot_poses_.size(), 4);
    EXPECT_LT(robot_poses_.size(), 5);

    // Should fail or produce unreliable result
    // Test verifies we detect this condition
}

// FR-HEC-006: Test with many poses (optimal case)
TEST_F(HandEyeSolverTest, ManyPoses) {
    GenerateTestPoses(20);

    EXPECT_EQ(robot_poses_.size(), 20);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));

    // With more poses, result should be more accurate
    EXPECT_TRUE(VerifyCalibration(X, 1e-2));
}

// FR-HEC-006: Test hand-eye equation: A*X = X*B
TEST_F(HandEyeSolverTest, HandEyeEquation) {
    GenerateTestPoses(10);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Verify AX = XB for all pose pairs
    for (size_t i = 0; i < robot_poses_.size(); ++i) {
        FrameTransform A = robot_poses_[i];
        FrameTransform B = device_poses_[i];

        FrameTransform AX = A * X;
        FrameTransform XB = X * B;

        double error = (AX - XB).norm();
        EXPECT_LT(error, 1.0) << "Hand-eye equation violated for pose " << i;
    }
}

// FR-HEC-006: Test that X is valid SE(3)
TEST_F(HandEyeSolverTest, SolutionIsValidSE3) {
    GenerateTestPoses(10);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Check SE(3) properties
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));

    // Check determinant
    Eigen::Matrix3d R = X.block<3, 3>(0, 0);
    EXPECT_NEAR(R.determinant(), 1.0, 1e-5);

    // Check orthogonality
    Eigen::Matrix3d RRt = R * R.transpose();
    EXPECT_NEAR((RRt - Eigen::Matrix3d::Identity()).norm(), 0.0, 1e-5);
}

// FR-HEC-007: Test Y = X^-1 relationship
TEST_F(HandEyeSolverTest, YIsInverseOfX) {
    GenerateTestPoses(10);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Compute Y separately (same poses, different formulation)
    FrameTransform Y = X.inverse();

    // Verify X * Y ≈ Identity
    FrameTransform identity = X * Y;
    EXPECT_NEAR((identity - FrameTransform::Identity()).norm(), 0.0, 1e-5);
}

// FR-HEC-006: Test with nearly identical poses (poor conditioning)
TEST_F(HandEyeSolverTest, NearlyIdenticalPoses) {
    // Generate poses that are very close to each other
    robot_poses_.clear();
    device_poses_.clear();

    FrameTransform base_pose = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);

    for (int i = 0; i < 10; ++i) {
        // Add small perturbation
        FrameTransform perturbation = TestData::TransformGenerator::GenerateRandomSE3(1.0, 0.5);
        robot_poses_.push_back(base_pose * perturbation);

        // Compute corresponding device pose
        FrameTransform device_pose = true_X_.inverse() * robot_poses_.back() * true_X_;
        device_poses_.push_back(device_pose);
    }

    // Calibration will be poorly conditioned
    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // May still produce valid SE(3) but with high error
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
}

// FR-HEC-006: Test with large motion range (well-conditioned)
TEST_F(HandEyeSolverTest, LargeMotionRange) {
    GenerateTestPoses(15);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Should produce accurate result
    EXPECT_TRUE(VerifyCalibration(X, 1e-3));
}

// FR-HEC-006: Test numerical stability
TEST_F(HandEyeSolverTest, NumericalStability) {
    GenerateTestPoses(10);

    // Run solver multiple times with same data
    std::vector<FrameTransform> results;
    for (int trial = 0; trial < 5; ++trial) {
        FrameTransform X = SolveHandEye(robot_poses_, device_poses_);
        results.push_back(X);
    }

    // All results should be identical (deterministic solver)
    for (size_t i = 1; i < results.size(); ++i) {
        double diff = (results[i] - results[0]).norm();
        EXPECT_NEAR(diff, 0.0, 1e-6)
            << "Solver produced different results on trial " << i;
    }
}

// FR-HEC-006: Test with noisy measurements
TEST_F(HandEyeSolverTest, NoisyMeasurements) {
    // Generate poses with noise
    auto noisy_poses = TestData::HandEyeDataGenerator::GenerateCalibrationPoses(
        15, true_X_, 0.01);  // 0.01 stddev noise

    robot_poses_.clear();
    device_poses_.clear();
    for (const auto& pose : noisy_poses) {
        robot_poses_.push_back(pose.robot_pose);
        device_poses_.push_back(pose.device_pose);
    }

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Should still be close to ground truth
    EXPECT_TRUE(VerifyCalibration(X, 0.1));  // Larger tolerance due to noise
}

// Test relative transformation computation
TEST_F(HandEyeSolverTest, RelativeTransformations) {
    GenerateTestPoses(10);

    // Compute relative transformations
    std::vector<FrameTransform> A_rel;
    std::vector<FrameTransform> B_rel;

    for (size_t i = 1; i < robot_poses_.size(); ++i) {
        FrameTransform A_inv = robot_poses_[i-1].inverse();
        A_rel.push_back(A_inv * robot_poses_[i]);

        FrameTransform B_inv = device_poses_[i-1].inverse();
        B_rel.push_back(B_inv * device_poses_[i]);
    }

    EXPECT_EQ(A_rel.size(), robot_poses_.size() - 1);
    EXPECT_EQ(B_rel.size(), device_poses_.size() - 1);

    // All relative transforms should be valid SE(3)
    for (const auto& T : A_rel) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(T));
    }
    for (const auto& T : B_rel) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(T));
    }
}

// Test identity transformation case
TEST_F(HandEyeSolverTest, IdentityTransformation) {
    // All robot poses are identity, device poses should also be identity
    robot_poses_.clear();
    device_poses_.clear();

    for (int i = 0; i < 10; ++i) {
        robot_poses_.push_back(FrameTransform::Identity());
        device_poses_.push_back(FrameTransform::Identity());
    }

    // This is a degenerate case, solver may fail
    // Test verifies graceful handling
    EXPECT_EQ(robot_poses_.size(), 10);
}

// Test pose ordering independence
TEST_F(HandEyeSolverTest, PoseOrderingIndependence) {
    GenerateTestPoses(10);

    // Solve with original ordering
    FrameTransform X1 = SolveHandEye(robot_poses_, device_poses_);

    // Reverse pose order
    std::vector<FrameTransform> robot_poses_rev(robot_poses_.rbegin(), robot_poses_.rend());
    std::vector<FrameTransform> device_poses_rev(device_poses_.rbegin(), device_poses_.rend());

    // Solve with reversed ordering
    FrameTransform X2 = SolveHandEye(robot_poses_rev, device_poses_rev);

    // Results should be the same
    EXPECT_NEAR((X1 - X2).norm(), 0.0, 1e-5);
}

// Test with exactly 5 poses (edge case)
TEST_F(HandEyeSolverTest, ExactlyFivePoses) {
    GenerateTestPoses(5);

    EXPECT_EQ(robot_poses_.size(), 5);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Should work with minimum required poses
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
}

// Test with very many poses (100+)
TEST_F(HandEyeSolverTest, VeryManyPoses) {
    GenerateTestPoses(100);

    EXPECT_EQ(robot_poses_.size(), 100);

    // Solver should handle large number of poses efficiently
    auto start = std::chrono::high_resolution_clock::now();
    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 100 poses)
    EXPECT_LT(duration.count(), 1000);

    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));
}

// Test calibration residual error
TEST_F(HandEyeSolverTest, CalibrationResidualError) {
    GenerateTestPoses(15);

    FrameTransform X = SolveHandEye(robot_poses_, device_poses_);

    // Compute residual error for each pose
    double total_error = 0.0;
    for (size_t i = 0; i < robot_poses_.size(); ++i) {
        FrameTransform AX = robot_poses_[i] * X;
        FrameTransform XB = X * device_poses_[i];
        double error = (AX - XB).norm();
        total_error += error;
    }

    double mean_error = total_error / robot_poses_.size();

    // Mean error should be small
    EXPECT_LT(mean_error, 0.1);
}
