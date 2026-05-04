
// STD
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

#include "xregProgOptUtils.h"
#include "xregFCSVUtils.h"
#include "xregITKIOUtils.h"
#include "xregITKLabelUtils.h"
#include "xregLandmarkMapUtils.h"
#include "xregAnatCoordFrames.h"
#include "xregH5ProjDataIO.h"
#include "xregRayCastProgOpts.h"
#include "xregRayCastInterface.h"
#include "xregImgSimMetric2DPatchCommon.h"
#include "xregImgSimMetric2DGradImgParamInterface.h"
#include "xregImgSimMetric2DProgOpts.h"
#include "xregHUToLinAtt.h"
#include "xregProjPreProc.h"
#include "xregCIOSFusionDICOM.h"
#include "xregPnPUtils.h"
#include "xregMultiObjMultiLevel2D3DRegi.h"
#include "xregMultiObjMultiLevel2D3DRegiDebug.h"
#include "xregSE3OptVars.h"
#include "xregIntensity2D3DRegiExhaustive.h"
#include "xregIntensity2D3DRegiCMAES.h"
#include "xregIntensity2D3DRegiBOBYQA.h"
#include "xregRegi2D3DPenaltyFnSE3Mag.h"
#include "xregFoldNormDist.h"
#include "xregHipSegUtils.h"

#include "xregPAODrawBones.h"
#include "xregRigidUtils.h"

#include "bigssMath.h"

using namespace xreg;

// ============================================================================
// Constants
// ============================================================================
constexpr int kEXIT_VAL_SUCCESS = 0;
constexpr int kEXIT_VAL_BAD_USE = 1;
constexpr int kEXIT_VAL_ERROR = 2;
constexpr bool kSAVE_REGI_DEBUG = true;
constexpr int kMIN_LANDMARKS = 4;
constexpr const char* kPROG_NAME = "device_bb_pnp";
constexpr const char* kOUTPUT_FILE_PREFIX = "device_pnp_xform";
constexpr const char* kOUTPUT_FILE_EXT = ".h5";
constexpr const char* kREPROJ_BB_PREFIX = "reproj_bb";

// ============================================================================
// Type Aliases
// ============================================================================
using size_type = std::size_t;
using Pt3 = Eigen::Matrix<CoordScalar, 3, 1>;
using Pt2 = Eigen::Matrix<CoordScalar, 2, 1>;

// ============================================================================
// Configuration Structure
// ============================================================================
/**
 * @brief Configuration parameters for device BB PnP processing
 *
 * Encapsulates all command-line arguments and configuration settings.
 */
struct DeviceBBPnPConfig {
  std::string device_2d_fcsv_path;   ///< 2D Landmark root path
  std::string device_3d_fcsv_path;   ///< 3D device landmarks path
  std::string exp_list_path;         ///< Experiment list file path
  std::string dicom_path;            ///< DICOM image root path
  std::string output_path;           ///< Output directory path
  bool verbose;                      ///< Verbose logging enabled

  /**
   * @brief Validate all configuration parameters
   * @throws std::runtime_error if validation fails
   */
  void Validate() const {
    if (device_2d_fcsv_path.empty()) {
      throw std::runtime_error("device_2d_fcsv_path is empty");
    }
    if (device_3d_fcsv_path.empty()) {
      throw std::runtime_error("device_3d_fcsv_path is empty");
    }
    if (exp_list_path.empty()) {
      throw std::runtime_error("exp_list_path is empty");
    }
    if (dicom_path.empty()) {
      throw std::runtime_error("dicom_path is empty");
    }
    if (output_path.empty()) {
      throw std::runtime_error("output_path is empty");
    }
  }
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Parse command-line arguments into configuration
 * @param argc Argument count
 * @param argv Argument vector
 * @return Configuration structure
 * @throws ProgOpts::Exception if argument parsing fails
 */
DeviceBBPnPConfig ParseCommandLineArguments(int argc, char* argv[]) {
  ProgOpts po;
  xregPROG_OPTS_SET_COMPILE_DATE(po);

  po.set_help("Device pose calculation using PnP solution");
  po.set_arg_usage(
      "<Device 2D landmark annotation ROOT path> "
      "<Device 3D landmark annotation FILE path> "
      "<Image ID list txt file path> "
      "<Image DICOM ROOT path> "
      "<Output folder path>");
  po.set_min_num_pos_args(5);
  po.add_backend_flags();

  try {
    po.parse(argc, argv);
  } catch (const ProgOpts::Exception& e) {
    std::cerr << "Error parsing command line arguments: " << e.what()
              << std::endl;
    po.print_usage(std::cerr);
    throw;
  }

  if (po.help_set()) {
    po.print_usage(std::cout);
    po.print_help(std::cout);
    exit(kEXIT_VAL_SUCCESS);
  }

  DeviceBBPnPConfig config;
  config.device_2d_fcsv_path = po.pos_args()[0];
  config.device_3d_fcsv_path = po.pos_args()[1];
  config.exp_list_path = po.pos_args()[2];
  config.dicom_path = po.pos_args()[3];
  config.output_path = po.pos_args()[4];
  config.verbose = po.get("verbose");

  return config;
}

/**
 * @brief Read experiment ID list from file
 * @param path Path to experiment list file
 * @return Vector of experiment IDs
 * @throws std::runtime_error if file cannot be opened or read
 */
std::vector<std::string> ReadExperimentIDList(const std::string& path) {
  std::vector<std::string> exp_id_list;
  std::ifstream file(path);

  if (!file.is_open()) {
    throw std::runtime_error(
        fmt::format("Could not open experiment ID list file: {}", path));
  }

  std::string line;
  int line_count = 0;

  while (std::getline(file, line)) {
    if (!line.empty()) {
      // Trim whitespace
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);
      if (!line.empty()) {
        exp_id_list.push_back(line);
      }
    }
    ++line_count;
  }

  if (exp_id_list.empty()) {
    throw std::runtime_error(
        fmt::format("No valid experiment IDs found in: {}", path));
  }

  return exp_id_list;
}

/**
 * @brief Process a single experiment: extract landmarks and compute PnP
 * @param config Configuration parameters
 * @param exp_id Experiment identifier
 * @param device_3d_fcsv 3D device landmarks
 * @throws std::runtime_error if processing fails
 */
void ProcessExperiment(const DeviceBBPnPConfig& config,
                       const std::string& exp_id,
                       const LandMap3& device_3d_fcsv) {
  // Construct file paths
  const std::string devicebb_2d_fcsv_path =
      config.device_2d_fcsv_path + "/" + kREPROJ_BB_PREFIX + exp_id + ".fcsv";
  const std::string img_path = config.dicom_path + "/" + exp_id;
  const std::string output_file_path =
      config.output_path + "/" + kOUTPUT_FILE_PREFIX + exp_id + kOUTPUT_FILE_EXT;

  if (config.verbose) {
    std::cout << "Processing experiment: " << exp_id << std::endl;
  }

  try {
    // Read 2D landmarks
    auto devicebb_2d_fcsv = ReadFCSVFileNamePtMap(devicebb_2d_fcsv_path);
    ConvertRASToLPS(&devicebb_2d_fcsv);

    // Validate landmark count
    if (devicebb_2d_fcsv.size() <= kMIN_LANDMARKS) {
      throw std::runtime_error(
          fmt::format("Insufficient landmarks for experiment {}: {} (required > {})",
                      exp_id, devicebb_2d_fcsv.size(), kMIN_LANDMARKS));
    }

    // Setup projection preprocessing
    ProjPreProc proj_pre_proc;
    proj_pre_proc.input_projs.resize(1);

    std::vector<CIOSFusionDICOMInfo> devicecios_metas(1);

    // Read DICOM and camera
    std::tie(proj_pre_proc.input_projs[0].img, devicecios_metas[0]) =
        ReadCIOSFusionDICOMFloat(img_path);
    proj_pre_proc.input_projs[0].cam =
        NaiveCamModelFromCIOSFusion(devicecios_metas[0], true);

    // Update landmarks for CIOS fusion and populate projection landmarks
    UpdateLandmarkMapForCIOSFusion(devicecios_metas[0], &devicebb_2d_fcsv);

    auto& deviceproj_lands = proj_pre_proc.input_projs[0].landmarks;
    deviceproj_lands.reserve(devicebb_2d_fcsv.size());

    for (const auto& fcsv_kv : devicebb_2d_fcsv) {
      deviceproj_lands.emplace(fcsv_kv.first,
                               Pt2{fcsv_kv.second[0], fcsv_kv.second[1]});
    }

    // Preprocess projections
    proj_pre_proc();
    auto& projs_to_regi = proj_pre_proc.output_projs;

    // Compute PnP solution
    FrameTransform pnp_cam_to_device = PnPPOSITAndReprojCMAES(
        projs_to_regi[0].cam, device_3d_fcsv, projs_to_regi[0].landmarks);

    // Write output
    WriteITKAffineTransform(output_file_path, pnp_cam_to_device);

    if (config.verbose) {
      std::cout << "Successfully processed experiment: " << exp_id << std::endl;
      std::cout << "Output written to: " << output_file_path << std::endl;
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(
        fmt::format("Error processing experiment {}: {}", exp_id, e.what()));
  }
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
  try {
    // Parse command-line arguments
    const auto config = ParseCommandLineArguments(argc, argv);

    // Validate configuration
    config.Validate();

    if (config.verbose) {
      std::cout << "Configuration validated successfully" << std::endl;
      std::cout << "2D FCSV path: " << config.device_2d_fcsv_path << std::endl;
      std::cout << "3D FCSV path: " << config.device_3d_fcsv_path << std::endl;
      std::cout << "Experiment list path: " << config.exp_list_path
                << std::endl;
      std::cout << "DICOM path: " << config.dicom_path << std::endl;
      std::cout << "Output path: " << config.output_path << std::endl;
    }

    // Read 3D device landmarks
    if (config.verbose) {
      std::cout << "Reading device BB landmarks from FCSV file..." << std::endl;
    }
    auto device_3d_fcsv = ReadFCSVFileNamePtMap(config.device_3d_fcsv_path);
    ConvertRASToLPS(&device_3d_fcsv);

    if (config.verbose) {
      std::cout << "Loaded " << device_3d_fcsv.size()
                << " 3D landmarks" << std::endl;
    }

    // Read experiment IDs
    const auto exp_id_list = ReadExperimentIDList(config.exp_list_path);

    if (config.verbose) {
      std::cout << "Processing " << exp_id_list.size()
                << " experiments" << std::endl;
    }

    // Process each experiment
    for (const auto& exp_id : exp_id_list) {
      ProcessExperiment(config, exp_id, device_3d_fcsv);
    }

    std::cout << "All experiments processed successfully" << std::endl;
    return kEXIT_VAL_SUCCESS;

  } catch (const ProgOpts::Exception& e) {
    std::cerr << "Command-line parsing error: " << e.what() << std::endl;
    return kEXIT_VAL_BAD_USE;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return kEXIT_VAL_ERROR;
  } catch (...) {
    std::cerr << "Unknown error occurred" << std::endl;
    return kEXIT_VAL_ERROR;
  }
}
