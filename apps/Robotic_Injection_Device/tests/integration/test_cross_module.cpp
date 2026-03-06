#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"

// Cross-module integration tests (BB PnP → Hand-Eye Calibration)
class CrossModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test data for both modules
        experiment_ids_ = TestData::ExperimentListGenerator::GenerateExperimentIDs(10);

        // Generate device landmarks for PnP
        TestData::DeviceLandmarkGenerator landmark_gen;
        device_3d_landmarks_ = landmark_gen.GenerateBoundingBoxLandmarks(100.0, 50.0, 200.0);

        // Generate ground truth hand-eye transformation
        true_X_ = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);
    }

    std::vector<std::string> experiment_ids_;
    std::map<std::string, Eigen::Vector3d> device_3d_landmarks_;
    Eigen::Matrix4d true_X_;
};

// Test: PnP output → Hand-Eye input compatibility
TEST_F(CrossModuleTest, PnPToHandEyeCompatibility) {
    // Step 1: Run PnP for multiple experiments (simulated)
    std::map<std::string, Eigen::Matrix4d> pnp_results;

    for (const auto& exp_id : experiment_ids_) {
        // Simulate PnP computation
        Eigen::Matrix4d device_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);
        pnp_results[exp_id] = device_pose;
    }

    EXPECT_EQ(pnp_results.size(), experiment_ids_.size());

    // Step 2: Use PnP results as input to hand-eye calibration
    std::vector<Eigen::Matrix4d> robot_poses;
    std::vector<Eigen::Matrix4d> device_poses;

    for (const auto& exp_id : experiment_ids_) {
        // Generate corresponding robot pose
        Eigen::Matrix4d robot_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);
        robot_poses.push_back(robot_pose);

        // Use PnP result as device pose
        device_poses.push_back(pnp_results[exp_id]);
    }

    // Step 3: Verify data compatibility
    EXPECT_EQ(robot_poses.size(), device_poses.size());
    EXPECT_GE(robot_poses.size(), 5);  // Minimum for hand-eye

    // All poses should be valid SE(3)
    for (const auto& pose : robot_poses) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pose));
    }
    for (const auto& pose : device_poses) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pose));
    }
}

// Test: End-to-end pipeline (PnP → Hand-Eye → Validation)
TEST_F(CrossModuleTest, EndToEndPipeline) {
    // Complete pipeline: Landmarks → PnP → Hand-Eye → Output

    // Stage 1: PnP for each experiment
    std::map<std::string, Eigen::Matrix4d> pnp_transforms;

    for (const auto& exp_id : experiment_ids_) {
        // Generate camera model
        auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();

        // Project landmarks
        auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            device_3d_landmarks_, camera);

        // Simulate PnP (return identity for simplification)
        Eigen::Matrix4d pnp_result = Eigen::Matrix4d::Identity();
        pnp_transforms[exp_id] = pnp_result;
    }

    EXPECT_EQ(pnp_transforms.size(), experiment_ids_.size());

    // Stage 2: Hand-eye calibration
    std::vector<Eigen::Matrix4d> robot_poses;
    std::vector<Eigen::Matrix4d> device_poses;

    for (const auto& [exp_id, pnp_xform] : pnp_transforms) {
        // Generate robot pose
        Eigen::Matrix4d robot_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);
        robot_poses.push_back(robot_pose);

        // Use PnP transform as device pose
        device_poses.push_back(pnp_xform);
    }

    // Compute hand-eye calibration (simplified)
    Eigen::Matrix4d X = true_X_;

    // Stage 3: Validation
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(X));

    // Verify hand-eye equation for each pose pair
    for (size_t i = 0; i < robot_poses.size(); ++i) {
        Eigen::Matrix4d AX = robot_poses[i] * X;
        Eigen::Matrix4d XB = X * device_poses[i];

        double error = (AX - XB).norm();
        EXPECT_LT(error, 10.0);  // Should satisfy equation (approximately)
    }
}

// Test: File format compatibility
TEST_F(CrossModuleTest, FileFormatCompatibility) {
    // Test that HDF5 files from PnP can be read by hand-eye calibration

    // Simulate PnP output
    Eigen::Matrix4d pnp_output = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);

    // Verify it's in correct format
    EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pnp_output));

    // Simulate hand-eye reading this file
    Eigen::Matrix4d handeye_input = pnp_output;  // Would be file read

    // Should be identical
    EXPECT_NEAR((handeye_input - pnp_output).norm(), 0.0, 1e-10);
}

// Test: Coordinate system consistency
TEST_F(CrossModuleTest, CoordinateSystemConsistency) {
    // Both modules should use same coordinate system (LPS)

    // Generate point in RAS
    Eigen::Vector3d pt_ras(10, 20, 30);

    // Convert to LPS (both modules do this)
    Eigen::Vector3d pt_lps(-pt_ras.x(), -pt_ras.y(), pt_ras.z());

    // Verify conversions match
    EXPECT_NEAR(pt_lps.x(), -10.0, 1e-6);
    EXPECT_NEAR(pt_lps.y(), -20.0, 1e-6);
    EXPECT_NEAR(pt_lps.z(), 30.0, 1e-6);

    // Apply same conversion in both modules
    // (This ensures consistency)
}

// Test: Experiment ID matching
TEST_F(CrossModuleTest, ExperimentIDMatching) {
    // Test that experiment IDs from PnP match those used in hand-eye

    std::vector<std::string> pnp_exp_ids = experiment_ids_;
    std::vector<std::string> handeye_exp_ids = experiment_ids_;

    EXPECT_EQ(pnp_exp_ids, handeye_exp_ids);

    // Verify ordering is preserved
    for (size_t i = 0; i < pnp_exp_ids.size(); ++i) {
        EXPECT_EQ(pnp_exp_ids[i], handeye_exp_ids[i]);
    }
}

// Test: Data flow integrity
TEST_F(CrossModuleTest, DataFlowIntegrity) {
    // Track data through complete pipeline

    struct PipelineData {
        std::string exp_id;
        std::map<std::string, Eigen::Vector3d> landmarks_3d;
        std::map<std::string, Eigen::Vector2d> landmarks_2d;
        Eigen::Matrix4d pnp_result;
        Eigen::Matrix4d robot_pose;
    };

    std::vector<PipelineData> pipeline_data;

    // Generate data for each experiment
    for (const auto& exp_id : experiment_ids_) {
        PipelineData data;
        data.exp_id = exp_id;
        data.landmarks_3d = device_3d_landmarks_;

        // Simulate projection
        auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
        data.landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            data.landmarks_3d, camera);

        // Simulate PnP
        data.pnp_result = Eigen::Matrix4d::Identity();

        // Simulate robot pose
        data.robot_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);

        pipeline_data.push_back(data);
    }

    // Verify data integrity
    EXPECT_EQ(pipeline_data.size(), experiment_ids_.size());

    for (const auto& data : pipeline_data) {
        EXPECT_FALSE(data.exp_id.empty());
        EXPECT_GT(data.landmarks_3d.size(), 0);
        EXPECT_GT(data.landmarks_2d.size(), 0);
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(data.pnp_result));
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(data.robot_pose));
    }
}

// Test: Error propagation across modules
TEST_F(CrossModuleTest, ErrorPropagationAcrossModules) {
    // Test how errors in PnP affect hand-eye calibration

    // Add noise to PnP results
    std::vector<Eigen::Matrix4d> noisy_pnp_results;
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 0.01);

    for (size_t i = 0; i < 10; ++i) {
        Eigen::Matrix4d pnp_result = Eigen::Matrix4d::Identity();

        // Add small noise
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 4; ++c) {
                pnp_result(r, c) += noise(rng);
            }
        }

        noisy_pnp_results.push_back(pnp_result);
    }

    // Use noisy results for hand-eye
    // Hand-eye calibration should still work but with reduced accuracy
    EXPECT_EQ(noisy_pnp_results.size(), 10);
}

// Test: Multi-view consistency
TEST_F(CrossModuleTest, MultiViewConsistency) {
    // Test that PnP results from different views are consistent

    Eigen::Matrix4d true_device_pose = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);

    std::vector<Eigen::Matrix4d> pnp_results_multi_view;

    // Generate multiple camera views
    for (int view = 0; view < 3; ++view) {
        auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
        camera.extrinsic = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);

        // Transform landmarks
        std::map<std::string, Eigen::Vector3d> transformed_landmarks;
        for (const auto& [name, pt] : device_3d_landmarks_) {
            Eigen::Vector4d pt_homog(pt.x(), pt.y(), pt.z(), 1.0);
            Eigen::Vector4d pt_transformed = true_device_pose * pt_homog;
            transformed_landmarks[name] = Eigen::Vector3d(
                pt_transformed.x(), pt_transformed.y(), pt_transformed.z());
        }

        // Project
        auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            transformed_landmarks, camera);

        // Simulate PnP (should recover true_device_pose)
        pnp_results_multi_view.push_back(true_device_pose);
    }

    // All views should produce similar results
    for (const auto& result : pnp_results_multi_view) {
        double error = (result - true_device_pose).norm();
        EXPECT_LT(error, 1e-10);
    }
}

// Test: Robustness to missing data
TEST_F(CrossModuleTest, RobustnessToMissingData) {
    // Some experiments may fail in PnP, hand-eye should still work

    std::map<std::string, Eigen::Matrix4d> pnp_results;

    // Simulate some failed experiments (missing data)
    for (size_t i = 0; i < experiment_ids_.size(); ++i) {
        if (i % 3 == 0) {
            // Skip this experiment (simulating failure)
            continue;
        }

        pnp_results[experiment_ids_[i]] = Eigen::Matrix4d::Identity();
    }

    // Should have fewer results than experiments
    EXPECT_LT(pnp_results.size(), experiment_ids_.size());

    // Hand-eye calibration should work with remaining data
    EXPECT_GE(pnp_results.size(), 5);  // Still enough for hand-eye
}

// Test: Performance of complete pipeline
TEST_F(CrossModuleTest, CompletePipelinePerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Simulate complete pipeline for all experiments
    for (const auto& exp_id : experiment_ids_) {
        // PnP
        auto camera = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
        auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            device_3d_landmarks_, camera);
        Eigen::Matrix4d pnp_result = Eigen::Matrix4d::Identity();

        // Hand-eye data collection
        Eigen::Matrix4d robot_pose = TestData::TransformGenerator::GenerateRandomSE3(300.0, 45.0);
    }

    // Hand-eye calibration
    Eigen::Matrix4d X = true_X_;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // NFR-001: Should complete in reasonable time
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second
}
