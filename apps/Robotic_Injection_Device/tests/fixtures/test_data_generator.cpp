#include "test_data_generator.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace TestData {

// DeviceLandmarkGenerator implementation
DeviceLandmarkGenerator::DeviceLandmarkGenerator(int num_landmarks)
    : num_landmarks_(num_landmarks), rng_(std::random_device{}()) {}

std::map<std::string, Pt3> DeviceLandmarkGenerator::GenerateBoundingBoxLandmarks(
    double size_x, double size_y, double size_z) {

    std::map<std::string, Pt3> landmarks;

    // Generate 8 corner points of a bounding box
    landmarks["corner_000"] = Pt3(0, 0, 0);
    landmarks["corner_001"] = Pt3(0, 0, size_z);
    landmarks["corner_010"] = Pt3(0, size_y, 0);
    landmarks["corner_011"] = Pt3(0, size_y, size_z);
    landmarks["corner_100"] = Pt3(size_x, 0, 0);
    landmarks["corner_101"] = Pt3(size_x, 0, size_z);
    landmarks["corner_110"] = Pt3(size_x, size_y, 0);
    landmarks["corner_111"] = Pt3(size_x, size_y, size_z);

    return landmarks;
}

void DeviceLandmarkGenerator::AddNoise(std::map<std::string, Pt3>& landmarks, double stddev) {
    std::normal_distribution<double> dist(0.0, stddev);

    for (auto& [name, pt] : landmarks) {
        pt.x() += dist(rng_);
        pt.y() += dist(rng_);
        pt.z() += dist(rng_);
    }
}

// CameraModelGenerator implementation
CameraModelGenerator::CameraParams CameraModelGenerator::GenerateCIOSFusionCamera(
    double focal_length, int img_width, int img_height) {

    CameraParams params;
    params.focal_length = focal_length;
    params.principal_x = img_width / 2.0;
    params.principal_y = img_height / 2.0;
    params.img_width = img_width;
    params.img_height = img_height;

    // Default extrinsic: camera at (0, 0, -1000) looking at origin
    params.extrinsic = FrameTransform::Identity();
    params.extrinsic(2, 3) = -1000.0;

    return params;
}

std::map<std::string, Pt2> CameraModelGenerator::ProjectLandmarks(
    const std::map<std::string, Pt3>& landmarks_3d,
    const CameraParams& camera) {

    std::map<std::string, Pt2> landmarks_2d;

    for (const auto& [name, pt3] : landmarks_3d) {
        // Transform to camera coordinates
        Eigen::Vector4d pt_homog(pt3.x(), pt3.y(), pt3.z(), 1.0);
        Eigen::Vector4d pt_cam = camera.extrinsic * pt_homog;

        // Project to image plane
        if (std::abs(pt_cam.z()) > 1e-6) {
            double x_proj = camera.focal_length * pt_cam.x() / pt_cam.z() + camera.principal_x;
            double y_proj = camera.focal_length * pt_cam.y() / pt_cam.z() + camera.principal_y;

            landmarks_2d[name] = Pt2(x_proj, y_proj);
        }
    }

    return landmarks_2d;
}

// TransformGenerator implementation
FrameTransform TransformGenerator::GenerateRandomSE3(
    double max_translation, double max_rotation_deg) {

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> trans_dist(-max_translation, max_translation);
    std::uniform_real_distribution<double> rot_dist(-max_rotation_deg, max_rotation_deg);
    std::uniform_real_distribution<double> axis_dist(-1.0, 1.0);

    // Random translation
    Pt3 translation(trans_dist(rng), trans_dist(rng), trans_dist(rng));

    // Random rotation
    Pt3 axis(axis_dist(rng), axis_dist(rng), axis_dist(rng));
    axis.normalize();
    double angle_deg = rot_dist(rng);

    FrameTransform T = Rotation(angle_deg, axis);
    T(0, 3) = translation.x();
    T(1, 3) = translation.y();
    T(2, 3) = translation.z();

    return T;
}

FrameTransform TransformGenerator::Identity() {
    return FrameTransform::Identity();
}

FrameTransform TransformGenerator::Translation(double x, double y, double z) {
    FrameTransform T = FrameTransform::Identity();
    T(0, 3) = x;
    T(1, 3) = y;
    T(2, 3) = z;
    return T;
}

FrameTransform TransformGenerator::Rotation(double angle_deg, const Pt3& axis) {
    double angle_rad = angle_deg * M_PI / 180.0;
    Pt3 normalized_axis = axis.normalized();

    // Rodrigues' rotation formula
    double c = std::cos(angle_rad);
    double s = std::sin(angle_rad);
    double t = 1.0 - c;

    double x = normalized_axis.x();
    double y = normalized_axis.y();
    double z = normalized_axis.z();

    FrameTransform T = FrameTransform::Identity();
    T(0, 0) = t*x*x + c;     T(0, 1) = t*x*y - s*z;  T(0, 2) = t*x*z + s*y;
    T(1, 0) = t*x*y + s*z;   T(1, 1) = t*y*y + c;    T(1, 2) = t*y*z - s*x;
    T(2, 0) = t*x*z - s*y;   T(2, 1) = t*y*z + s*x;  T(2, 2) = t*z*z + c;

    return T;
}

FrameTransform TransformGenerator::Compose(const FrameTransform& T1, const FrameTransform& T2) {
    return T1 * T2;
}

bool TransformGenerator::IsValidSE3(const FrameTransform& T, double tolerance) {
    // Check if bottom row is [0, 0, 0, 1]
    if (std::abs(T(3, 0)) > tolerance || std::abs(T(3, 1)) > tolerance ||
        std::abs(T(3, 2)) > tolerance || std::abs(T(3, 3) - 1.0) > tolerance) {
        return false;
    }

    // Extract rotation matrix
    Eigen::Matrix3d R = T.block<3, 3>(0, 0);

    // Check orthogonality: R * R^T = I
    Eigen::Matrix3d RRt = R * R.transpose();
    if ((RRt - Eigen::Matrix3d::Identity()).norm() > tolerance) {
        return false;
    }

    // Check determinant = 1
    if (std::abs(R.determinant() - 1.0) > tolerance) {
        return false;
    }

    return true;
}

// FCSVGenerator implementation
std::string FCSVGenerator::GenerateFCSV(
    const std::map<std::string, Pt3>& landmarks, bool use_RAS) {

    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);

    // FCSV header
    ss << "# Markups fiducial file version = 4.11\n";
    ss << "# CoordinateSystem = " << (use_RAS ? "RAS" : "LPS") << "\n";
    ss << "# columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID\n";

    int id = 0;
    for (const auto& [name, pt] : landmarks) {
        ss << id++ << "," << pt.x() << "," << pt.y() << "," << pt.z()
           << ",0,0,0,1,1,1,0," << name << ",,\n";
    }

    return ss.str();
}

std::string FCSVGenerator::Generate2DFCSV(const std::map<std::string, Pt2>& landmarks) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);

    ss << "# Markups fiducial file version = 4.11\n";
    ss << "# CoordinateSystem = RAS\n";
    ss << "# columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID\n";

    int id = 0;
    for (const auto& [name, pt] : landmarks) {
        ss << id++ << "," << pt.x() << "," << pt.y() << ",0"
           << ",0,0,0,1,1,1,0," << name << ",,\n";
    }

    return ss.str();
}

std::map<std::string, Pt3> FCSVGenerator::ParseFCSV(const std::string& content) {
    std::map<std::string, Pt3> landmarks;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream line_stream(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(line_stream, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 11) {
            std::string name = tokens[11];
            double x = std::stod(tokens[1]);
            double y = std::stod(tokens[2]);
            double z = std::stod(tokens[3]);
            landmarks[name] = Pt3(x, y, z);
        }
    }

    return landmarks;
}

// ExperimentListGenerator implementation
std::vector<std::string> ExperimentListGenerator::GenerateExperimentIDs(int count) {
    std::vector<std::string> ids;
    for (int i = 0; i < count; ++i) {
        std::stringstream ss;
        ss << "exp_" << std::setw(4) << std::setfill('0') << i;
        ids.push_back(ss.str());
    }
    return ids;
}

std::string ExperimentListGenerator::GenerateExperimentListFile(
    const std::vector<std::string>& ids) {

    std::stringstream ss;
    for (const auto& id : ids) {
        ss << id << "\n";
    }
    return ss.str();
}

// HandEyeDataGenerator implementation
std::vector<HandEyeDataGenerator::HandEyePose> HandEyeDataGenerator::GenerateCalibrationPoses(
    int num_poses, const FrameTransform& true_X, double noise_stddev) {

    std::vector<HandEyePose> poses;
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<double> noise(0.0, noise_stddev);

    // Generate random robot poses
    for (int i = 0; i < num_poses; ++i) {
        HandEyePose pose;

        // Random robot end-effector pose
        pose.robot_pose = TransformGenerator::GenerateRandomSE3(500.0, 45.0);

        // Compute corresponding device pose: B = X * A * X^-1
        // Actually: A * X = X * B, so B = X^-1 * A * X
        FrameTransform X_inv = true_X.inverse();
        pose.device_pose = X_inv * pose.robot_pose * true_X;

        // Add noise if requested
        if (noise_stddev > 0.0) {
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c) {
                    pose.device_pose(r, c) += noise(rng);
                }
            }
        }

        poses.push_back(pose);
    }

    return poses;
}

bool HandEyeDataGenerator::VerifyHandEyeSolution(
    const std::vector<HandEyePose>& poses,
    const FrameTransform& X,
    double tolerance) {

    for (const auto& pose : poses) {
        // Check: A * X = X * B
        FrameTransform AX = pose.robot_pose * X;
        FrameTransform XB = X * pose.device_pose;

        double error = (AX - XB).norm();
        if (error > tolerance) {
            return false;
        }
    }

    return true;
}

} // namespace TestData
