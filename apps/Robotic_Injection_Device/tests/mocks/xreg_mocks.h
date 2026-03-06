#ifndef XREG_MOCKS_H
#define XREG_MOCKS_H

#include <gmock/gmock.h>
#include <map>
#include <string>
#include <vector>
#include <Eigen/Dense>

// Forward declarations for xReg types
namespace xreg {
    using CoordScalar = double;
    using Pt2 = Eigen::Matrix<CoordScalar, 2, 1>;
    using Pt3 = Eigen::Matrix<CoordScalar, 3, 1>;
    using FrameTransform = Eigen::Matrix<CoordScalar, 4, 4>;

    template<typename T>
    using LandmarkMap = std::map<std::string, T>;

    using LandMap2 = LandmarkMap<Pt2>;
    using LandMap3 = LandmarkMap<Pt3>;

    struct CIOSFusionDICOMInfo {
        double pixel_spacing_x;
        double pixel_spacing_y;
        int rows;
        int cols;
        double focal_length;
        double principal_point_x;
        double principal_point_y;
    };

    struct CameraModel {
        FrameTransform extrinsic;
        Eigen::Matrix3d intrinsic;
        int img_width;
        int img_height;
    };

    struct Projection {
        std::vector<float> img;
        CameraModel cam;
        LandMap2 landmarks;
    };

    struct ProjPreProc {
        std::vector<Projection> input_projs;
        std::vector<Projection> output_projs;

        void operator()() {
            output_projs = input_projs;
        }
    };
}

// Mock class for xReg file I/O operations
class MockXRegFileIO {
public:
    MOCK_METHOD(xreg::LandMap3, ReadFCSVFileNamePtMap, (const std::string& path));
    MOCK_METHOD(void, ConvertRASToLPS, (xreg::LandMap3* landmarks));
    MOCK_METHOD((std::pair<std::vector<float>, xreg::CIOSFusionDICOMInfo>),
                ReadCIOSFusionDICOMFloat, (const std::string& path));
    MOCK_METHOD(void, WriteITKAffineTransform,
                (const std::string& path, const xreg::FrameTransform& xform));
    MOCK_METHOD(xreg::FrameTransform, ReadITKAffineTransformFromFile,
                (const std::string& path));
};

// Mock class for PnP solver
class MockPnPSolver {
public:
    MOCK_METHOD(xreg::FrameTransform, PnPPOSITAndReprojCMAES,
                (const xreg::CameraModel& cam,
                 const xreg::LandMap3& pts_3d,
                 const xreg::LandMap2& pts_2d));
};

// Mock class for camera model creation
class MockCameraModel {
public:
    MOCK_METHOD(xreg::CameraModel, NaiveCamModelFromCIOSFusion,
                (const xreg::CIOSFusionDICOMInfo& meta, bool is_default));
    MOCK_METHOD(xreg::CIOSFusionDICOMInfo, MakeNaiveCIOSFusionMetaDR, ());
};

// Mock class for landmark utilities
class MockLandmarkUtils {
public:
    MOCK_METHOD(void, UpdateLandmarkMapForCIOSFusion,
                (const xreg::CIOSFusionDICOMInfo& meta, xreg::LandMap3* landmarks));
};

// Mock class for BIGSS hand-eye solver
class MockHandEyeSolver {
public:
    MOCK_METHOD(void, SolveHandEye,
                (const std::vector<xreg::FrameTransform>& A,
                 const std::vector<xreg::FrameTransform>& B,
                 xreg::FrameTransform& X));
};

// Singleton to access mocks
class XRegMocks {
public:
    static XRegMocks& Instance() {
        static XRegMocks instance;
        return instance;
    }

    MockXRegFileIO file_io;
    MockPnPSolver pnp_solver;
    MockCameraModel camera_model;
    MockLandmarkUtils landmark_utils;
    MockHandEyeSolver handeye_solver;

private:
    XRegMocks() = default;
};

#endif // XREG_MOCKS_H
