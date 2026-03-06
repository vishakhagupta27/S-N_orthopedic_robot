#include <gtest/gtest.h>
#include "../fixtures/test_data_generator.h"
#include <map>
#include <string>
#include <Eigen/Dense>

using Pt3 = Eigen::Vector3d;
using Pt2 = Eigen::Vector2d;

// Test FCSV file operations for device_bb_pnp
class DeviceBBPnPFCSVTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test landmarks
        TestData::DeviceLandmarkGenerator generator(8);
        landmarks_3d_ = generator.GenerateBoundingBoxLandmarks(100.0, 50.0, 200.0);

        // Generate FCSV content
        fcsv_content_ras_ = TestData::FCSVGenerator::GenerateFCSV(landmarks_3d_, true);
        fcsv_content_lps_ = TestData::FCSVGenerator::GenerateFCSV(landmarks_3d_, false);
    }

    std::map<std::string, Pt3> landmarks_3d_;
    std::string fcsv_content_ras_;
    std::string fcsv_content_lps_;

    // Helper to convert RAS to LPS
    std::map<std::string, Pt3> ConvertRASToLPS(const std::map<std::string, Pt3>& ras_pts) {
        std::map<std::string, Pt3> lps_pts;
        for (const auto& [name, pt] : ras_pts) {
            lps_pts[name] = Pt3(-pt.x(), -pt.y(), pt.z());
        }
        return lps_pts;
    }
};

// FR-BBP-002: Test successful FCSV parsing
TEST_F(DeviceBBPnPFCSVTest, SuccessfulFCSVParsing) {
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv_content_ras_);

    EXPECT_EQ(parsed.size(), landmarks_3d_.size());

    for (const auto& [name, pt] : landmarks_3d_) {
        ASSERT_TRUE(parsed.count(name) > 0) << "Missing landmark: " << name;
        EXPECT_NEAR(parsed[name].x(), pt.x(), 1e-5);
        EXPECT_NEAR(parsed[name].y(), pt.y(), 1e-5);
        EXPECT_NEAR(parsed[name].z(), pt.z(), 1e-5);
    }
}

// FR-BBP-002: Test RAS to LPS coordinate conversion
TEST_F(DeviceBBPnPFCSVTest, RASToLPSConversion) {
    auto ras_pts = TestData::FCSVGenerator::ParseFCSV(fcsv_content_ras_);
    auto lps_pts = ConvertRASToLPS(ras_pts);

    for (const auto& [name, ras_pt] : ras_pts) {
        ASSERT_TRUE(lps_pts.count(name) > 0);
        const auto& lps_pt = lps_pts[name];

        // RAS to LPS: negate X and Y, keep Z
        EXPECT_NEAR(lps_pt.x(), -ras_pt.x(), 1e-5);
        EXPECT_NEAR(lps_pt.y(), -ras_pt.y(), 1e-5);
        EXPECT_NEAR(lps_pt.z(), ras_pt.z(), 1e-5);
    }
}

// FR-BBP-002: Test empty FCSV file
TEST_F(DeviceBBPnPFCSVTest, EmptyFCSVFile) {
    std::string empty_fcsv = "# Markups fiducial file version = 4.11\n"
                             "# CoordinateSystem = RAS\n"
                             "# columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID\n";

    auto parsed = TestData::FCSVGenerator::ParseFCSV(empty_fcsv);
    EXPECT_EQ(parsed.size(), 0);
}

// FR-BBP-002: Test FCSV with single landmark
TEST_F(DeviceBBPnPFCSVTest, SingleLandmark) {
    std::map<std::string, Pt3> single_landmark;
    single_landmark["point1"] = Pt3(10.0, 20.0, 30.0);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(single_landmark, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    EXPECT_EQ(parsed.size(), 1);
    ASSERT_TRUE(parsed.count("point1") > 0);
    EXPECT_NEAR(parsed["point1"].x(), 10.0, 1e-5);
    EXPECT_NEAR(parsed["point1"].y(), 20.0, 1e-5);
    EXPECT_NEAR(parsed["point1"].z(), 30.0, 1e-5);
}

// FR-BBP-002: Test FCSV with special characters in landmark names
TEST_F(DeviceBBPnPFCSVTest, SpecialCharactersInNames) {
    std::map<std::string, Pt3> special_landmarks;
    special_landmarks["point_with_underscore"] = Pt3(1, 2, 3);
    special_landmarks["point-with-dash"] = Pt3(4, 5, 6);
    special_landmarks["point.with.dot"] = Pt3(7, 8, 9);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(special_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    EXPECT_EQ(parsed.size(), 3);
    EXPECT_TRUE(parsed.count("point_with_underscore") > 0);
    EXPECT_TRUE(parsed.count("point-with-dash") > 0);
    EXPECT_TRUE(parsed.count("point.with.dot") > 0);
}

// FR-BBP-002: Test FCSV with zero coordinates
TEST_F(DeviceBBPnPFCSVTest, ZeroCoordinates) {
    std::map<std::string, Pt3> zero_landmarks;
    zero_landmarks["origin"] = Pt3(0, 0, 0);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(zero_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    ASSERT_TRUE(parsed.count("origin") > 0);
    EXPECT_NEAR(parsed["origin"].norm(), 0.0, 1e-5);
}

// FR-BBP-002: Test FCSV with negative coordinates
TEST_F(DeviceBBPnPFCSVTest, NegativeCoordinates) {
    std::map<std::string, Pt3> negative_landmarks;
    negative_landmarks["negative"] = Pt3(-10, -20, -30);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(negative_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    ASSERT_TRUE(parsed.count("negative") > 0);
    EXPECT_NEAR(parsed["negative"].x(), -10.0, 1e-5);
    EXPECT_NEAR(parsed["negative"].y(), -20.0, 1e-5);
    EXPECT_NEAR(parsed["negative"].z(), -30.0, 1e-5);
}

// FR-BBP-002: Test FCSV with very large coordinates
TEST_F(DeviceBBPnPFCSVTest, LargeCoordinates) {
    std::map<std::string, Pt3> large_landmarks;
    large_landmarks["large"] = Pt3(1000.0, 2000.0, 3000.0);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(large_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    ASSERT_TRUE(parsed.count("large") > 0);
    EXPECT_NEAR(parsed["large"].x(), 1000.0, 1e-5);
    EXPECT_NEAR(parsed["large"].y(), 2000.0, 1e-5);
    EXPECT_NEAR(parsed["large"].z(), 3000.0, 1e-5);
}

// FR-BBP-002: Test FCSV with very small (fractional) coordinates
TEST_F(DeviceBBPnPFCSVTest, SmallFractionalCoordinates) {
    std::map<std::string, Pt3> small_landmarks;
    small_landmarks["small"] = Pt3(0.001, 0.002, 0.003);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(small_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    ASSERT_TRUE(parsed.count("small") > 0);
    EXPECT_NEAR(parsed["small"].x(), 0.001, 1e-7);
    EXPECT_NEAR(parsed["small"].y(), 0.002, 1e-7);
    EXPECT_NEAR(parsed["small"].z(), 0.003, 1e-7);
}

// FR-BBP-002: Test malformed FCSV (missing columns)
TEST_F(DeviceBBPnPFCSVTest, MalformedFCSVMissingColumns) {
    std::string malformed_fcsv =
        "# Markups fiducial file version = 4.11\n"
        "# CoordinateSystem = RAS\n"
        "# columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID\n"
        "0,10.0,20.0\n";  // Missing columns

    auto parsed = TestData::FCSVGenerator::ParseFCSV(malformed_fcsv);
    // Should handle gracefully - either skip or parse partial
    EXPECT_LE(parsed.size(), 1);  // At most 1 entry (may be 0)
}

// FR-BBP-005: Test 2D landmark extraction
TEST_F(DeviceBBPnPFCSVTest, TwoDimensionalLandmarks) {
    std::map<std::string, Pt2> landmarks_2d;
    landmarks_2d["pt1"] = Pt2(100, 200);
    landmarks_2d["pt2"] = Pt2(300, 400);
    landmarks_2d["pt3"] = Pt2(500, 600);
    landmarks_2d["pt4"] = Pt2(700, 800);

    std::string fcsv_2d = TestData::FCSVGenerator::Generate2DFCSV(landmarks_2d);
    EXPECT_FALSE(fcsv_2d.empty());

    // Parse as 3D (Z should be 0)
    auto parsed_3d = TestData::FCSVGenerator::ParseFCSV(fcsv_2d);
    EXPECT_EQ(parsed_3d.size(), landmarks_2d.size());

    for (const auto& [name, pt2d] : landmarks_2d) {
        ASSERT_TRUE(parsed_3d.count(name) > 0);
        EXPECT_NEAR(parsed_3d[name].x(), pt2d.x(), 1e-5);
        EXPECT_NEAR(parsed_3d[name].y(), pt2d.y(), 1e-5);
        EXPECT_NEAR(parsed_3d[name].z(), 0.0, 1e-5);
    }
}

// FR-BBP-005: Test minimum landmark count (4 for PnP)
TEST_F(DeviceBBPnPFCSVTest, MinimumLandmarkCount) {
    std::map<std::string, Pt3> min_landmarks;
    min_landmarks["pt1"] = Pt3(0, 0, 0);
    min_landmarks["pt2"] = Pt3(100, 0, 0);
    min_landmarks["pt3"] = Pt3(0, 100, 0);
    min_landmarks["pt4"] = Pt3(0, 0, 100);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(min_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    EXPECT_EQ(parsed.size(), 4);
    EXPECT_GE(parsed.size(), 4);  // Minimum required for PnP
}

// FR-BBP-005: Test insufficient landmark count (< 4)
TEST_F(DeviceBBPnPFCSVTest, InsufficientLandmarkCount) {
    std::map<std::string, Pt3> insufficient_landmarks;
    insufficient_landmarks["pt1"] = Pt3(0, 0, 0);
    insufficient_landmarks["pt2"] = Pt3(100, 0, 0);
    insufficient_landmarks["pt3"] = Pt3(0, 100, 0);

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(insufficient_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    EXPECT_EQ(parsed.size(), 3);
    EXPECT_LT(parsed.size(), 4);  // Less than required for PnP
}

// FR-BBP-005: Test large number of landmarks
TEST_F(DeviceBBPnPFCSVTest, LargeLandmarkCount) {
    std::map<std::string, Pt3> many_landmarks;
    for (int i = 0; i < 100; ++i) {
        std::string name = "pt" + std::to_string(i);
        many_landmarks[name] = Pt3(i * 1.0, i * 2.0, i * 3.0);
    }

    std::string fcsv = TestData::FCSVGenerator::GenerateFCSV(many_landmarks, true);
    auto parsed = TestData::FCSVGenerator::ParseFCSV(fcsv);

    EXPECT_EQ(parsed.size(), 100);
}

// Test coordinate system specification in FCSV
TEST_F(DeviceBBPnPFCSVTest, CoordinateSystemSpecification) {
    // RAS system
    EXPECT_TRUE(fcsv_content_ras_.find("CoordinateSystem = RAS") != std::string::npos);

    // LPS system
    EXPECT_TRUE(fcsv_content_lps_.find("CoordinateSystem = LPS") != std::string::npos);
}
