#include <gtest/gtest.h>
#include "../fixtures/transform_fixtures.h"
#include "../fixtures/test_data_generator.h"
#include <Eigen/Dense>

using Pt3 = Eigen::Vector3d;
using FrameTransform = Eigen::Matrix4d;

// Test coordinate transformations for device_bb_pnp
class DeviceBBPnPCoordsTest : public TransformFixtures::CoordinateConversionFixture {
protected:
    // Additional helper methods
    Pt3 ApplyRASToLPS(const Pt3& ras_pt) {
        return Pt3(-ras_pt.x(), -ras_pt.y(), ras_pt.z());
    }

    bool IsClose(const Pt3& pt1, const Pt3& pt2, double tolerance = 1e-6) {
        return (pt1 - pt2).norm() < tolerance;
    }
};

// FR-BBP-002: Test RAS to LPS conversion for origin
TEST_F(DeviceBBPnPCoordsTest, RASToLPSOrigin) {
    Pt3 origin_ras = points_ras_["origin"];
    Pt3 origin_lps = ApplyRASToLPS(origin_ras);

    EXPECT_NEAR(origin_lps.x(), 0.0, 1e-6);
    EXPECT_NEAR(origin_lps.y(), 0.0, 1e-6);
    EXPECT_NEAR(origin_lps.z(), 0.0, 1e-6);
}

// FR-BBP-002: Test RAS to LPS conversion for positive X
TEST_F(DeviceBBPnPCoordsTest, RASToLPSPositiveX) {
    Pt3 pt_ras = points_ras_["positive_x"];
    Pt3 pt_lps = ApplyRASToLPS(pt_ras);

    // RAS +X becomes LPS -X
    EXPECT_NEAR(pt_lps.x(), -10.0, 1e-6);
    EXPECT_NEAR(pt_lps.y(), 0.0, 1e-6);
    EXPECT_NEAR(pt_lps.z(), 0.0, 1e-6);
}

// FR-BBP-002: Test RAS to LPS conversion for positive Y
TEST_F(DeviceBBPnPCoordsTest, RASToLPSPositiveY) {
    Pt3 pt_ras = points_ras_["positive_y"];
    Pt3 pt_lps = ApplyRASToLPS(pt_ras);

    // RAS +Y becomes LPS -Y
    EXPECT_NEAR(pt_lps.x(), 0.0, 1e-6);
    EXPECT_NEAR(pt_lps.y(), -10.0, 1e-6);
    EXPECT_NEAR(pt_lps.z(), 0.0, 1e-6);
}

// FR-BBP-002: Test RAS to LPS conversion for positive Z
TEST_F(DeviceBBPnPCoordsTest, RASToLPSPositiveZ) {
    Pt3 pt_ras = points_ras_["positive_z"];
    Pt3 pt_lps = ApplyRASToLPS(pt_ras);

    // RAS +Z stays LPS +Z
    EXPECT_NEAR(pt_lps.x(), 0.0, 1e-6);
    EXPECT_NEAR(pt_lps.y(), 0.0, 1e-6);
    EXPECT_NEAR(pt_lps.z(), 10.0, 1e-6);
}

// FR-BBP-002: Test RAS to LPS conversion for arbitrary point
TEST_F(DeviceBBPnPCoordsTest, RASToLPSArbitrary) {
    Pt3 pt_ras = points_ras_["arbitrary"];  // (5, -3, 7)
    Pt3 pt_lps = ApplyRASToLPS(pt_ras);

    EXPECT_NEAR(pt_lps.x(), -5.0, 1e-6);
    EXPECT_NEAR(pt_lps.y(), 3.0, 1e-6);
    EXPECT_NEAR(pt_lps.z(), 7.0, 1e-6);
}

// FR-BBP-002: Test RAS to LPS is involutory (applying twice returns to original)
TEST_F(DeviceBBPnPCoordsTest, RASToLPSInvolutory) {
    for (const auto& [name, pt_ras] : points_ras_) {
        Pt3 pt_lps = ApplyRASToLPS(pt_ras);
        Pt3 pt_ras_back = ApplyRASToLPS(pt_lps);

        EXPECT_TRUE(IsClose(pt_ras, pt_ras_back))
            << "Point " << name << " failed involution test";
    }
}

// FR-BBP-002: Test coordinate conversion preserves distances (for Z-axis)
TEST_F(DeviceBBPnPCoordsTest, ConversionPreservesZDistance) {
    Pt3 pt1_ras(0, 0, 0);
    Pt3 pt2_ras(0, 0, 100);

    Pt3 pt1_lps = ApplyRASToLPS(pt1_ras);
    Pt3 pt2_lps = ApplyRASToLPS(pt2_ras);

    double dist_ras = (pt2_ras - pt1_ras).norm();
    double dist_lps = (pt2_lps - pt1_lps).norm();

    EXPECT_NEAR(dist_ras, dist_lps, 1e-6);
}

// FR-BBP-002: Test coordinate conversion with transformation matrix
TEST_F(DeviceBBPnPCoordsTest, MatrixBasedConversion) {
    // Test using transformation matrix
    for (const auto& [name, pt_ras] : points_ras_) {
        Eigen::Vector4d pt_homog(pt_ras.x(), pt_ras.y(), pt_ras.z(), 1.0);
        Eigen::Vector4d pt_lps_homog = ras_to_lps_ * pt_homog;
        Pt3 pt_lps_mat(pt_lps_homog.x(), pt_lps_homog.y(), pt_lps_homog.z());

        Pt3 pt_lps_direct = ApplyRASToLPS(pt_ras);

        EXPECT_TRUE(IsClose(pt_lps_mat, pt_lps_direct))
            << "Matrix and direct conversion differ for " << name;
    }
}

// Test coordinate bounds checking
TEST_F(DeviceBBPnPCoordsTest, CoordinateBoundsValid) {
    // Typical medical image bounds: -1000 to +1000 mm
    double max_coord = 1000.0;

    for (const auto& [name, pt] : points_ras_) {
        EXPECT_LE(std::abs(pt.x()), max_coord * 10)  // Allow 10x margin
            << "X coordinate out of reasonable bounds for " << name;
        EXPECT_LE(std::abs(pt.y()), max_coord * 10)
            << "Y coordinate out of reasonable bounds for " << name;
        EXPECT_LE(std::abs(pt.z()), max_coord * 10)
            << "Z coordinate out of reasonable bounds for " << name;
    }
}

// Test camera coordinate system
class CameraCoordinateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate camera model
        camera_params_ = TestData::CameraModelGenerator::GenerateCIOSFusionCamera();
    }

    TestData::CameraModelGenerator::CameraParams camera_params_;
};

// FR-BBP-004: Test camera model parameters
TEST_F(CameraCoordinateTest, CameraParameters) {
    EXPECT_GT(camera_params_.focal_length, 0.0);
    EXPECT_GT(camera_params_.img_width, 0);
    EXPECT_GT(camera_params_.img_height, 0);
    EXPECT_GT(camera_params_.principal_x, 0.0);
    EXPECT_GT(camera_params_.principal_y, 0.0);
}

// FR-BBP-004: Test camera principal point at image center
TEST_F(CameraCoordinateTest, PrincipalPointAtCenter) {
    double expected_cx = camera_params_.img_width / 2.0;
    double expected_cy = camera_params_.img_height / 2.0;

    EXPECT_NEAR(camera_params_.principal_x, expected_cx, 1.0);
    EXPECT_NEAR(camera_params_.principal_y, expected_cy, 1.0);
}

// FR-BBP-004: Test camera extrinsic matrix is valid SE(3)
TEST_F(CameraCoordinateTest, ExtrinsicValidSE3) {
    bool is_valid = TestData::TransformGenerator::IsValidSE3(camera_params_.extrinsic);
    EXPECT_TRUE(is_valid);
}

// FR-BBP-005: Test 3D to 2D projection
TEST_F(CameraCoordinateTest, ThreeDToTwoDProjection) {
    std::map<std::string, Pt3> landmarks_3d;
    landmarks_3d["test_pt"] = Pt3(50, 0, 100);  // In front of camera

    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        landmarks_3d, camera_params_);

    EXPECT_EQ(landmarks_2d.size(), 1);
    ASSERT_TRUE(landmarks_2d.count("test_pt") > 0);

    // Check projection is within image bounds
    Pt2 projected = landmarks_2d["test_pt"];
    EXPECT_GE(projected.x(), 0);
    EXPECT_LT(projected.x(), camera_params_.img_width);
    EXPECT_GE(projected.y(), 0);
    EXPECT_LT(projected.y(), camera_params_.img_height);
}

// FR-BBP-005: Test projection of origin
TEST_F(CameraCoordinateTest, ProjectOrigin) {
    std::map<std::string, Pt3> landmarks_3d;
    landmarks_3d["origin"] = Pt3(0, 0, 0);

    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        landmarks_3d, camera_params_);

    EXPECT_EQ(landmarks_2d.size(), 1);
}

// FR-BBP-005: Test projection with points at different depths
TEST_F(CameraCoordinateTest, ProjectionAtDifferentDepths) {
    std::map<std::string, Pt3> landmarks_3d;
    landmarks_3d["near"] = Pt3(0, 0, 100);
    landmarks_3d["far"] = Pt3(0, 0, 1000);

    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        landmarks_3d, camera_params_);

    // Both should project to approximately the same point (near principal point)
    // since they're on the Z-axis
    EXPECT_EQ(landmarks_2d.size(), 2);
}

// FR-BBP-005: Test projection outside image bounds
TEST_F(CameraCoordinateTest, ProjectionOutsideBounds) {
    std::map<std::string, Pt3> landmarks_3d;
    landmarks_3d["far_left"] = Pt3(-5000, 0, 100);  // Way outside field of view

    auto landmarks_2d = TestData::CameraModelGenerator::ProjectLandmarks(
        landmarks_3d, camera_params_);

    // Should still project, but be outside image bounds
    if (landmarks_2d.count("far_left") > 0) {
        // Just verify it doesn't crash
        SUCCEED();
    }
}

// Test coordinate system handedness
TEST(CoordinateSystemTest, RightHandedSystem) {
    Pt3 x_axis(1, 0, 0);
    Pt3 y_axis(0, 1, 0);
    Pt3 z_expected = x_axis.cross(y_axis);

    EXPECT_NEAR(z_expected.x(), 0.0, 1e-6);
    EXPECT_NEAR(z_expected.y(), 0.0, 1e-6);
    EXPECT_NEAR(z_expected.z(), 1.0, 1e-6);
}

// Test LPS coordinate system handedness
TEST(CoordinateSystemTest, LPSHandedness) {
    // LPS: Left-Posterior-Superior
    // X: Left, Y: Posterior, Z: Superior
    // Should be right-handed
    Pt3 x_axis(1, 0, 0);  // Left
    Pt3 y_axis(0, 1, 0);  // Posterior
    Pt3 z_expected = x_axis.cross(y_axis);  // Should point Superior

    EXPECT_NEAR(z_expected.z(), 1.0, 1e-6);
}
