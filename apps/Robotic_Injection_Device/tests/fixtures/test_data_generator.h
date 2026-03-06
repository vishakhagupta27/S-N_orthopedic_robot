#ifndef TEST_DATA_GENERATOR_H
#define TEST_DATA_GENERATOR_H

#include <string>
#include <vector>
#include <map>
#include <Eigen/Dense>
#include <random>

namespace TestData {

using Pt2 = Eigen::Vector2d;
using Pt3 = Eigen::Vector3d;
using FrameTransform = Eigen::Matrix4d;

// Generate synthetic 3D landmarks for a device
class DeviceLandmarkGenerator {
public:
    DeviceLandmarkGenerator(int num_landmarks = 8);

    // Generate landmarks in a bounding box configuration
    std::map<std::string, Pt3> GenerateBoundingBoxLandmarks(
        double size_x = 100.0,
        double size_y = 50.0,
        double size_z = 200.0
    );

    // Add Gaussian noise to landmarks
    void AddNoise(std::map<std::string, Pt3>& landmarks, double stddev);

private:
    int num_landmarks_;
    std::mt19937 rng_;
};

// Generate synthetic camera models
class CameraModelGenerator {
public:
    struct CameraParams {
        double focal_length;
        double principal_x;
        double principal_y;
        int img_width;
        int img_height;
        FrameTransform extrinsic;
    };

    // Generate typical C-arm camera parameters
    static CameraParams GenerateCIOSFusionCamera(
        double focal_length = 1000.0,
        int img_width = 1024,
        int img_height = 1024
    );

    // Project 3D landmarks to 2D
    static std::map<std::string, Pt2> ProjectLandmarks(
        const std::map<std::string, Pt3>& landmarks_3d,
        const CameraParams& camera
    );
};

// Generate transformation matrices
class TransformGenerator {
public:
    // Generate random SE(3) transformation
    static FrameTransform GenerateRandomSE3(
        double max_translation = 100.0,
        double max_rotation_deg = 45.0
    );

    // Generate identity transformation
    static FrameTransform Identity();

    // Generate pure translation
    static FrameTransform Translation(double x, double y, double z);

    // Generate pure rotation (axis-angle)
    static FrameTransform Rotation(double angle_deg, const Pt3& axis);

    // Compose transformations
    static FrameTransform Compose(const FrameTransform& T1, const FrameTransform& T2);

    // Verify SE(3) properties
    static bool IsValidSE3(const FrameTransform& T, double tolerance = 1e-6);
};

// Generate test FCSV files
class FCSVGenerator {
public:
    // Generate FCSV content
    static std::string GenerateFCSV(
        const std::map<std::string, Pt3>& landmarks,
        bool use_RAS = true
    );

    // Generate 2D FCSV content
    static std::string Generate2DFCSV(
        const std::map<std::string, Pt2>& landmarks
    );

    // Parse FCSV content
    static std::map<std::string, Pt3> ParseFCSV(const std::string& content);
};

// Generate experiment lists
class ExperimentListGenerator {
public:
    static std::vector<std::string> GenerateExperimentIDs(int count);
    static std::string GenerateExperimentListFile(const std::vector<std::string>& ids);
};

// Hand-eye calibration test data
class HandEyeDataGenerator {
public:
    struct HandEyePose {
        FrameTransform robot_pose;  // A frame
        FrameTransform device_pose; // B frame
    };

    // Generate synthetic hand-eye calibration data
    static std::vector<HandEyePose> GenerateCalibrationPoses(
        int num_poses,
        const FrameTransform& true_X,
        double noise_stddev = 0.0
    );

    // Verify hand-eye solution
    static bool VerifyHandEyeSolution(
        const std::vector<HandEyePose>& poses,
        const FrameTransform& X,
        double tolerance = 1e-3
    );
};

} // namespace TestData

#endif // TEST_DATA_GENERATOR_H
