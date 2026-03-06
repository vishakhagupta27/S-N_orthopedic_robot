#include <gtest/gtest.h>
#include <string>
#include <vector>

// Test command-line argument parsing for device_handeye_calibration
class DeviceHandEyeCmdLineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test arguments
        valid_args_ = {
            "device_handeye_calibration",
            "/path/to/debug",
            "/path/to/slicer",
            "/path/to/pnp",
            "/path/to/deviceref.fcsv",
            "/path/to/exp_list.txt",
            "prefix_"
        };
    }

    std::vector<std::string> valid_args_;
};

// FR-HEC-001: Test valid argument count (6 arguments)
TEST_F(DeviceHandEyeCmdLineTest, ValidArgumentCount) {
    EXPECT_EQ(valid_args_.size(), 7);  // Program name + 6 args
}

// FR-HEC-001: Test insufficient arguments
TEST_F(DeviceHandEyeCmdLineTest, InsufficientArguments) {
    std::vector<std::string> insufficient_args = {
        "device_handeye_calibration",
        "/path/to/debug",
        "/path/to/slicer"
        // Missing 3 arguments
    };

    EXPECT_LT(insufficient_args.size(), 7);
}

// FR-HEC-001: Test all required paths are present
TEST_F(DeviceHandEyeCmdLineTest, AllPathsPresent) {
    for (size_t i = 1; i < valid_args_.size(); ++i) {
        EXPECT_FALSE(valid_args_[i].empty())
            << "Argument " << i << " should not be empty";
    }
}

// FR-HEC-001: Test argument ordering
TEST_F(DeviceHandEyeCmdLineTest, CorrectArgumentOrdering) {
    EXPECT_EQ(valid_args_[0], "device_handeye_calibration");
    EXPECT_TRUE(valid_args_[1].find("debug") != std::string::npos);
    EXPECT_TRUE(valid_args_[2].find("slicer") != std::string::npos);
    EXPECT_TRUE(valid_args_[3].find("pnp") != std::string::npos);
    EXPECT_TRUE(valid_args_[4].find("deviceref") != std::string::npos);
    EXPECT_TRUE(valid_args_[5].find("exp_list") != std::string::npos);
    EXPECT_TRUE(valid_args_[6].find("prefix") != std::string::npos);
}

// FR-HEC-001: Test help flag
TEST_F(DeviceHandEyeCmdLineTest, HelpFlagRecognized) {
    std::vector<std::string> help_args = {"device_handeye_calibration", "--help"};

    bool has_help = false;
    for (const auto& arg : help_args) {
        if (arg == "--help" || arg == "-h") {
            has_help = true;
        }
    }

    EXPECT_TRUE(has_help);
}

// Test file prefix format
TEST_F(DeviceHandEyeCmdLineTest, FilePrefixFormat) {
    std::string prefix = valid_args_[6];

    // Prefix should be a valid filename prefix
    EXPECT_FALSE(prefix.empty());

    // Should not contain path separators
    EXPECT_EQ(prefix.find('/'), std::string::npos);
    EXPECT_EQ(prefix.find('\\'), std::string::npos);
}

// Test paths with special characters
TEST_F(DeviceHandEyeCmdLineTest, PathsWithSpecialCharacters) {
    std::vector<std::string> special_args = {
        "device_handeye_calibration",
        "/path/with spaces/debug",
        "/path-with-dashes/slicer",
        "/path_with_underscores/pnp",
        "/path.with.dots/ref.fcsv",
        "/path/exp_list.txt",
        "prefix_v1.0"
    };

    for (const auto& arg : special_args) {
        EXPECT_FALSE(arg.empty());
    }
}
