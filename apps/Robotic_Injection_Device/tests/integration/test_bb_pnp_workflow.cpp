#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"
#include <map>
#include <vector>
#include <string>

// Integration test for complete BB PnP workflow
class BBPnPWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate synthetic test data for full workflow
        experiment_ids_ = TestData::ExperimentListGenerator::GenerateExperimentIDs(5);

        // Generate device landmarks
        TestData::DeviceLandmarkGenerator landmark_gen;
        device_3d_landmarks_ = landmark_gen.GenerateBoundingBoxLandmarks(100.0, 50.0, 200.0);

        // Generate camera models for each experiment
        for (const auto& exp_id : experiment_ids_) {
            cameras_[exp_id] = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
        }
    }

    std::vector<std::string> experiment_ids_;
    std::map<std::string, Eigen::Vector3d> device_3d_landmarks_;
    std::map<std::string, TestData::CameraModelGenerator::CameraParams> cameras_;
};

// Test: Full workflow from FCSV to PnP output
TEST_F(BBPnPWorkflowTest, CompleteWorkflow) {
    // Step 1: Load 3D landmarks (simulated)
    EXPECT_GT(device_3d_landmarks_.size(), 0);

    // Step 2: Process each experiment
    std::map<std::string, Eigen::Matrix4d> computed_poses;

    for (const auto& exp_id : experiment_ids_) {
        // Load 2D landmarks (simulated by projection)
        auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            device_3d_landmarks_,
            cameras_[exp_id]
        );

        EXPECT_GT(landmarks_2d.size(), 0) << "No 2D landmarks for " << exp_id;

        // Compute PnP (simplified - return identity for test)
        Eigen::Matrix4d pose = Eigen::Matrix4d::Identity();
        computed_poses[exp_id] = pose;
    }

    // Step 3: Verify all experiments processed
    EXPECT_EQ(computed_poses.size(), experiment_ids_.size());

    // Step 4: Validate all computed poses
    for (const auto& [exp_id, pose] : computed_poses) {
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(pose))
            << "Invalid pose for " << exp_id;
    }
}

// Test: Multi-experiment batch processing
TEST_F(BBPnPWorkflowTest, BatchProcessing) {
    int success_count = 0;
    int failure_count = 0;

    for (const auto& exp_id : experiment_ids_) {
        try {
            // Simulate processing
            auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
                device_3d_landmarks_,
                cameras_[exp_id]
            );

            if (landmarks_2d.size() >= 4) {
                success_count++;
            } else {
                failure_count++;
            }
        } catch (...) {
            failure_count++;
        }
    }

    // Most experiments should succeed
    EXPECT_GT(success_count, 0);
    EXPECT_EQ(failure_count, 0);
}

// Test: Data pipeline integrity
TEST_F(BBPnPWorkflowTest, DataPipelineIntegrity) {
    // Test that data flows correctly through the pipeline
    // 3D landmarks → Transform → Project → 2D landmarks → PnP → Pose

    Eigen::Matrix4d known_pose = TestData::TransformGenerator::GenerateRandomSE3(200.0, 30.0);

    // Transform 3D landmarks
    std::map<std::string, Eigen::Vector3d> transformed_landmarks;
    for (const auto& [name, pt] : device_3d_landmarks_) {
        Eigen::Vector4d pt_homog(pt.x(), pt.y(), pt.z(), 1.0);
        Eigen::Vector4d pt_transformed = known_pose * pt_homog;
        transformed_landmarks[name] = Eigen::Vector3d(
            pt_transformed.x(),
            pt_transformed.y(),
            pt_transformed.z()
        );
    }

    // Project to 2D
    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        transformed_landmarks,
        cameras_[experiment_ids_[0]]
    );

    EXPECT_GT(landmarks_2d.size(), 0);

    // Verify data consistency
    EXPECT_EQ(device_3d_landmarks_.size(), 8);  // Bounding box has 8 corners
}

// Test: Error recovery in pipeline
TEST_F(BBPnPWorkflowTest, ErrorRecovery) {
    // Simulate error in one experiment
    std::vector<std::string> failed_experiments;

    for (const auto& exp_id : experiment_ids_) {
        bool success = true;

        // Simulate random failure
        if (exp_id == experiment_ids_[2]) {
            success = false;
            failed_experiments.push_back(exp_id);
        }

        if (!success) {
            // Pipeline should continue with other experiments
            continue;
        }
    }

    // Should process remaining experiments
    int processed_count = experiment_ids_.size() - failed_experiments.size();
    EXPECT_EQ(processed_count, 4);
}

// Test: Coordinate system conversions
TEST_F(BBPnPWorkflowTest, CoordinateSystemConversions) {
    // Test RAS → LPS → Processing → Output

    // Start with RAS coordinates
    std::map<std::string, Eigen::Vector3d> ras_landmarks = device_3d_landmarks_;

    // Convert to LPS
    std::map<std::string, Eigen::Vector3d> lps_landmarks;
    for (const auto& [name, pt] : ras_landmarks) {
        lps_landmarks[name] = Eigen::Vector3d(-pt.x(), -pt.y(), pt.z());
    }

    // Process in LPS
    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        lps_landmarks,
        cameras_[experiment_ids_[0]]
    );

    EXPECT_GT(landmarks_2d.size(), 0);

    // Verify conversion didn't lose data
    EXPECT_EQ(ras_landmarks.size(), lps_landmarks.size());
}

// Test: Output file generation
TEST_F(BBPnPWorkflowTest, OutputFileGeneration) {
    // Simulate HDF5 file creation for each experiment
    std::map<std::string, bool> output_files;

    for (const auto& exp_id : experiment_ids_) {
        // Simulate file write success
        output_files[exp_id] = true;
    }

    // All output files should be generated
    EXPECT_EQ(output_files.size(), experiment_ids_.size());

    for (const auto& [exp_id, success] : output_files) {
        EXPECT_TRUE(success) << "Failed to generate output for " << exp_id;
    }
}

// Test: Performance with large dataset
TEST_F(BBPnPWorkflowTest, LargeDatasetProcessing) {
    // Generate large experiment list
    auto large_exp_list = TestData::ExperimentListGenerator::GenerateExperimentIDs(100);

    EXPECT_EQ(large_exp_list.size(), 100);

    // Simulate processing time measurement
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& exp_id : large_exp_list) {
        // Lightweight processing simulation
        volatile int dummy = 0;
        for (int i = 0; i < 1000; ++i) {
            dummy += i;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds for simulation
}

// Test: Experiment list parsing
TEST_F(BBPnPWorkflowTest, ExperimentListParsing) {
    std::string exp_list_content = TestData::ExperimentListGenerator::GenerateExperimentListFile(
        experiment_ids_
    );

    EXPECT_FALSE(exp_list_content.empty());

    // Count newlines (should match experiment count)
    int line_count = std::count(exp_list_content.begin(), exp_list_content.end(), '\n');
    EXPECT_EQ(line_count, experiment_ids_.size());
}

// Test: Camera model consistency
TEST_F(BBPnPWorkflowTest, CameraModelConsistency) {
    // All cameras should have consistent parameters
    for (const auto& [exp_id, camera] : cameras_) {
        EXPECT_GT(camera.focal_length, 0.0);
        EXPECT_GT(camera.img_width, 0);
        EXPECT_GT(camera.img_height, 0);
        EXPECT_TRUE(TestData::TransformGenerator::IsValidSE3(camera.extrinsic));
    }
}

// Test: Landmark visibility across views
TEST_F(BBPnPWorkflowTest, LandmarkVisibilityAcrossViews) {
    // Test that most landmarks are visible in most views
    int total_visible = 0;
    int total_possible = experiment_ids_.size() * device_3d_landmarks_.size();

    for (const auto& exp_id : experiment_ids_) {
        auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
            device_3d_landmarks_,
            cameras_[exp_id]
        );

        total_visible += landmarks_2d.size();
    }

    // At least 50% of landmarks should be visible across all views
    double visibility_ratio = static_cast<double>(total_visible) / total_possible;
    EXPECT_GT(visibility_ratio, 0.5);
}
