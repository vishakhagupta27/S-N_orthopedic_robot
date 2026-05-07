// STD
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>
#include <limits>
#include <filesystem>
#include <algorithm>

#include <fmt/format.h>

#include "xregProgOptUtils.h"
#include "xregFCSVUtils.h"
#include "xregITKIOUtils.h"
#include "xregITKLabelUtils.h"
#include "xregLandmarkMapUtils.h"
#include "xregAnatCoordFrames.h"
#include "xregHDF5.h"
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
namespace fs = std::filesystem;

constexpr int kEXIT_VAL_SUCCESS = 0;
constexpr int kEXIT_VAL_BAD_USE = 1;

using size_type = std::size_t;

using Pt3         = Eigen::Matrix<CoordScalar,3,1>;
using Pt2         = Eigen::Matrix<CoordScalar,2,1>;

constexpr size_type kMIN_NUM_FRAMES = 6;

struct ReadinessCheckResult
{
  bool ready = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
};

inline bool PathExists(const std::string& path)
{
  std::error_code ec;
  return fs::exists(fs::path(path), ec);
}

inline bool IsDirectoryPath(const std::string& path)
{
  std::error_code ec;
  return fs::is_directory(fs::path(path), ec);
}

std::vector<std::string> ReadExperimentIDs(const std::string& exp_list_path)
{
  std::vector<std::string> exp_ID_list;

  std::ifstream expIDFile(exp_list_path);
  if (!expIDFile.is_open())
  {
    throw std::runtime_error("Could not open exp ID file: " + exp_list_path);
  }

  std::string line;
  while (std::getline(expIDFile, line))
  {
    if (!line.empty())
    {
      exp_ID_list.push_back(line);
    }
  }

  return exp_ID_list;
}

std::string ResolvePnPPath(const std::string& root_pnp_path, const std::string& exp_ID)
{
  const std::string path_without_underscore = root_pnp_path + "/devicepnp_xform" + exp_ID + ".h5";
  if (PathExists(path_without_underscore))
  {
    return path_without_underscore;
  }

  const std::string path_with_underscore = root_pnp_path + "/device_pnp_xform" + exp_ID + ".h5";
  if (PathExists(path_with_underscore))
  {
    return path_with_underscore;
  }

  return "";
}

ReadinessCheckResult RunReadinessCheck(const std::string& root_debug_path,
                                       const std::string& root_slicer_path,
                                       const std::string& root_pnp_path,
                                       const std::string& deviceref_fcsv_path,
                                       const std::vector<std::string>& exp_ID_list)
{
  ReadinessCheckResult result;

  if (!PathExists(root_debug_path) || !IsDirectoryPath(root_debug_path))
  {
    result.errors.emplace_back("Debug output directory missing: " + root_debug_path);
  }

  if (!PathExists(root_slicer_path) || !IsDirectoryPath(root_slicer_path))
  {
    result.errors.emplace_back("Slicer root directory missing: " + root_slicer_path);
  }

  if (!PathExists(root_pnp_path) || !IsDirectoryPath(root_pnp_path))
  {
    result.errors.emplace_back("PnP root directory missing: " + root_pnp_path);
  }

  if (!PathExists(deviceref_fcsv_path))
  {
    result.errors.emplace_back("Device reference FCSV missing: " + deviceref_fcsv_path);
  }

  if (exp_ID_list.empty())
  {
    result.errors.emplace_back("Experiment ID list is empty.");
  }

  std::set<std::string> unique_ids;
  for (const auto& exp_id : exp_ID_list)
  {
    if (!unique_ids.insert(exp_id).second)
    {
      result.warnings.emplace_back("Duplicate experiment ID found: " + exp_id);
    }

    const std::string src_ureef_path = root_slicer_path + "/" + exp_id + "/ur_eef.h5";
    if (!PathExists(src_ureef_path))
    {
      result.errors.emplace_back("Missing robot pose file: " + src_ureef_path);
    }

    const std::string pnp_path = ResolvePnPPath(root_pnp_path, exp_id);
    if (pnp_path.empty())
    {
      result.errors.emplace_back("Missing PnP transform file for exp " + exp_id +
                                 " (checked devicepnp_xform*.h5 and device_pnp_xform*.h5)");
    }
  }

  result.ready = result.errors.empty();
  return result;
}

double ComputeQualityScore(double mean_residual, double max_residual)
{
  // Lower residuals produce higher scores. Coefficients chosen for intuitive 0-100 scaling.
  const double raw_score = 100.0 - (25.0 * mean_residual) - (10.0 * max_residual);
  return std::clamp(raw_score, 0.0, 100.0);
}

std::string ComputeQualityGrade(const double quality_score)
{
  if (quality_score >= 90.0)
  {
    return "A";
  }
  if (quality_score >= 75.0)
  {
    return "B";
  }
  if (quality_score >= 60.0)
  {
    return "C";
  }
  return "D";
}

FrameTransform ConvertSlicerToITK(std::vector<float> slicer_vec){
  FrameTransform RAS2LPS;
  RAS2LPS(0, 0) = -1; RAS2LPS(0, 1) = 0; RAS2LPS(0, 2) = 0; RAS2LPS(0, 3) = 0;
  RAS2LPS(1, 0) = 0;  RAS2LPS(1, 1) = -1;RAS2LPS(1, 2) = 0; RAS2LPS(1, 3) = 0;
  RAS2LPS(2, 0) = 0;  RAS2LPS(2, 1) = 0; RAS2LPS(2, 2) = 1; RAS2LPS(2, 3) = 0;
  RAS2LPS(3, 0) = 0;  RAS2LPS(3, 1) = 0; RAS2LPS(3, 2) = 0; RAS2LPS(3, 3) = 1;

  FrameTransform ITK_xform;
  for(size_type idx=0; idx<4; ++idx)
  {
    for(size_type idy=0; idy<3; ++idy)
    {
      ITK_xform(idy, idx) = slicer_vec[idx*3+idy];
    }
    ITK_xform(3, idx) = 0.0;
  }
  ITK_xform(3, 3) = 1.0;

  ITK_xform= RAS2LPS * ITK_xform * RAS2LPS;

  float tmp1, tmp2, tmp3;
  tmp1 = ITK_xform(0,0)*ITK_xform(0,3) + ITK_xform(0,1)*ITK_xform(1,3) + ITK_xform(0,2)*ITK_xform(2,3);
  tmp2 = ITK_xform(1,0)*ITK_xform(0,3) + ITK_xform(1,1)*ITK_xform(1,3) + ITK_xform(1,2)*ITK_xform(2,3);
  tmp3 = ITK_xform(2,0)*ITK_xform(0,3) + ITK_xform(2,1)*ITK_xform(1,3) + ITK_xform(2,2)*ITK_xform(2,3);

  ITK_xform(0,3) = -tmp1;
  ITK_xform(1,3) = -tmp2;
  ITK_xform(2,3) = -tmp3;

  return ITK_xform;
}

int main(int argc, char* argv[])
{
  ProgOpts po;

  xregPROG_OPTS_SET_COMPILE_DATE(po);

  po.set_help("Compute 3D Polaris Position of Snake Tip Jig");
  po.set_arg_usage("<root regi debug H5 file source path> <root slicer H5 file source path> <root pnp H5 path> <device ref fcsv path> <exp ID file> <output file prefix>");
  po.set_min_num_pos_args(6);

  po.add_backend_flags();

  try
  {
    po.parse(argc, argv);
  }
  catch (const ProgOpts::Exception& e)
  {
    std::cerr << "Error parsing command line arguments: " << e.what() << std::endl;
    po.print_usage(std::cerr);
    return kEXIT_VAL_BAD_USE;
  }

  if (po.help_set())
  {
    po.print_usage(std::cout);
    po.print_help(std::cout);
    return kEXIT_VAL_SUCCESS;
  }

  const bool verbose = po.get("verbose");
  std::ostream& vout = po.vout();

  if (po.pos_args().size() != 6)
  {
    std::cerr << "Expected exactly 6 positional arguments, got " << po.pos_args().size() << std::endl;
    po.print_usage(std::cerr);
    return kEXIT_VAL_BAD_USE;
  }

  const std::string root_debug_path      = po.pos_args()[0]; // Debug root path to save pnp handeye result
  const std::string root_slicer_path     = po.pos_args()[1]; // Slicer root path
  const std::string root_pnp_path        = po.pos_args()[2]; // Pnp xform root path
  const std::string deviceref_fcsv_path   = po.pos_args()[3];  // 3D device rotation center landmark path
  const std::string exp_list_path        = po.pos_args()[4]; // Source exp list file
  const std::string file_prefix          = po.pos_args()[5]; // Handeye result save prefix

  std::vector<std::string> exp_ID_list = ReadExperimentIDs(exp_list_path);

  auto readiness = RunReadinessCheck(root_debug_path, root_slicer_path, root_pnp_path,
                                     deviceref_fcsv_path, exp_ID_list);

  std::cout << "==================== READINESS CHECK ====================" << std::endl;
  std::cout << "Status: " << (readiness.ready ? "READY" : "NOT READY") << std::endl;
  std::cout << "Experiments found: " << exp_ID_list.size() << std::endl;
  if (!readiness.warnings.empty())
  {
    std::cout << "Warnings:" << std::endl;
    for (const auto& warning : readiness.warnings)
    {
      std::cout << "  - " << warning << std::endl;
    }
  }
  if (!readiness.errors.empty())
  {
    std::cout << "Errors:" << std::endl;
    for (const auto& error : readiness.errors)
    {
      std::cout << "  - " << error << std::endl;
    }
  }
  std::cout << "=========================================================" << std::endl;

  if (!readiness.ready)
  {
    return kEXIT_VAL_BAD_USE;
  }

  std::cout << "reading device rotation center ref landmark from FCSV file..." << std::endl;
  auto deviceref_3dfcsv = ReadFCSVFileNamePtMap(deviceref_fcsv_path);
  vout << "  RAS --> LPS..." << std::endl;
  ConvertRASToLPS(&deviceref_3dfcsv);

  FrameTransform device_rotcen_ref = FrameTransform::Identity();
  {
    auto deviceref_fcsv = deviceref_3dfcsv.find("HandeyeRef");
    Pt3 device_rotcen_pt;

    if (deviceref_fcsv != deviceref_3dfcsv.end()){
      device_rotcen_pt = deviceref_fcsv->second;
    }
    else{
      std::cout << "ERROR: NOT FOUND device REF PT" << std::endl;
    }

    device_rotcen_ref.matrix()(0,3) = -device_rotcen_pt[0];
    device_rotcen_ref.matrix()(1,3) = -device_rotcen_pt[1];
    device_rotcen_ref.matrix()(2,3) = -device_rotcen_pt[2];
  }

  const auto default_cam = NaiveCamModelFromCIOSFusion(
                                  MakeNaiveCIOSFusionMetaDR(), true);

  std::vector <vctFrm4x4> A_frames;    //< Transformation of A frames
  std::vector <vctFrm4x4> B_frames;    //< Transformation of B frames

  std::vector<FrameTransform> A_frames_eigen;
  std::vector<FrameTransform> B_frames_eigen;

  for(size_type idx=0; idx<exp_ID_list.size(); ++idx)
  {
    const std::string exp_ID                  = exp_ID_list[idx];

    // Read Robot End Effector transformation from h5_slicer file
    const std::string src_ureef_path          = root_slicer_path + "/" + exp_ID + "/ur_eef.h5";
    H5::H5File h5_ureef(src_ureef_path, H5F_ACC_RDWR);
    H5::Group ureef_transform_group           = h5_ureef.openGroup("TransformGroup");
    H5::Group ureef_group0                    = ureef_transform_group.openGroup("0");
    std::vector<float> UReef_tracker          = ReadVectorH5Float("TranformParameters", ureef_group0);

    FrameTransform UReef_xform                = ConvertSlicerToITK(UReef_tracker);

    const std::string src_pnp_path            = ResolvePnPPath(root_pnp_path, exp_ID);
    if (src_pnp_path.empty())
    {
      throw std::runtime_error("PnP transform file missing after readiness check for experiment: " + exp_ID);
    }
    FrameTransform pnp_xform = ReadITKAffineTransformFromFile(src_pnp_path);

    FrameTransform device_cam_to_ref = device_rotcen_ref * pnp_xform;
    FrameTransform device_ref_to_cam = device_cam_to_ref.inverse(); //actuation jig relative to C-arm

    vctFrm4x4 A_frame;
    vctFrm4x4 B_frame;

    for(size_type idx=0; idx<4; ++idx)
    {
      for(size_type idy=0; idy<4; ++idy)
      {
        A_frame[idx][idy] = UReef_xform(idx, idy);
        B_frame[idx][idy] = device_ref_to_cam(idx, idy);
      }
    }


    std::cout << exp_ID << std::endl;
    std::cout << "A frame:\n" << A_frame << std::endl;
    std::cout << "B frame:\n" << B_frame << std::endl;
    std::cout << " ***************************************** " << std::endl;

    A_frames.push_back(A_frame);
    B_frames.push_back(B_frame);
    A_frames_eigen.push_back(UReef_xform);
    B_frames_eigen.push_back(device_ref_to_cam);
  }

  if(A_frames.size() < kMIN_NUM_FRAMES){
    std::cerr << "At least " << kMIN_NUM_FRAMES << " frames are required for hand-eye calibration" << std::endl;
    return kEXIT_VAL_BAD_USE;
  }

  vctDoubleMat AX, BX, AY, BY;
  AX.SetSize(4*A_frames.size(), 4);
  BX.SetSize(4*B_frames.size(), 4);
  AY.SetSize(4*A_frames.size(), 4);
  BY.SetSize(4*A_frames.size(), 4);

  // identity for first A, B matrices
  AX.Ref(4, 4, 0, 0).Assign(vctDoubleMat(vctFrm4x4()));
  BX.Ref(4, 4, 0, 0).Assign(vctDoubleMat(vctFrm4x4()));
  AY.Ref(4, 4, 0, 0).Assign(vctDoubleMat(vctFrm4x4()));
  BY.Ref(4, 4, 0, 0).Assign(vctDoubleMat(vctFrm4x4()));

  /*
  // precalculate inverse to solve for X
  vctFrm4x4 AInv = A_frames[0].Inverse();
  vctFrm4x4 BInv = B_frames[0].Inverse();

  for (unsigned int i=1; i<A_frames.size(); i++) {
    AX.Ref(4, 4, 4*i, 0).Assign ( vctDoubleMat (AInv * A_frames[i]));
    BX.Ref(4, 4, 4*i, 0).Assign ( vctDoubleMat (BInv * B_frames[i]));
    AY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(A_frames[0]*A_frames[i].Inverse()));
    BY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(B_frames[0]*B_frames[i].Inverse()));
  }
   */

  // precalculate inverse to solve for X
  vctFrm4x4 AInv = A_frames[0].Inverse();
  vctFrm4x4 BInv = B_frames[0].Inverse();

  for (unsigned int i=1; i<A_frames.size(); i++) {
    AInv = A_frames[i-1].Inverse();
    BInv = B_frames[i-1].Inverse();
    AX.Ref(4, 4, 4*i, 0).Assign ( vctDoubleMat (AInv * A_frames[i]));
    BX.Ref(4, 4, 4*i, 0).Assign ( vctDoubleMat (BInv * B_frames[i]));
    AY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(A_frames[i-1]*A_frames[i].Inverse()));
    BY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(B_frames[i-1]*B_frames[i].Inverse()));
  }

  vctFrm4x4 X, Y;
  BIGSS::ax_xb(AX, BX, X);
  BIGSS::ax_xb(AY, BY, Y);

  // print out results
  std::cout << "X = " << std::endl << X << std::endl;
  std::cout << "Y = " << std::endl << Y << std::endl;

  // Save hand-eye result to file
  FrameTransform pnphandeye_X = FrameTransform::Identity();
  FrameTransform pnphandeye_Y = FrameTransform::Identity();
  for(size_type idx=0; idx<4; ++idx)
  {
    for(size_type idy=0; idy<4; ++idy)
    {
      pnphandeye_X(idx, idy) = X[idx][idy];
      pnphandeye_Y(idx, idy) = Y[idx][idy];
    }
  }

  const std::string pnphandeye_X_file = root_debug_path + "/" + file_prefix + "handeye_pnp_X.h5";
  const std::string pnphandeye_Y_file = root_debug_path + "/" + file_prefix + "handeye_pnp_Y.h5";
  WriteITKAffineTransform(pnphandeye_X_file, pnphandeye_X);
  WriteITKAffineTransform(pnphandeye_Y_file, pnphandeye_Y);

  // Quality score based on calibration residuals: ||A_i X - X B_i||
  double residual_sum = 0.0;
  double residual_max = 0.0;
  for (size_type i = 0; i < A_frames_eigen.size(); ++i)
  {
    const FrameTransform AX = A_frames_eigen[i] * pnphandeye_X;
    const FrameTransform XB = pnphandeye_X * B_frames_eigen[i];
    const double residual = (AX - XB).norm();
    residual_sum += residual;
    residual_max = std::max(residual_max, residual);
  }

  const double mean_residual = residual_sum / static_cast<double>(A_frames_eigen.size());
  const double quality_score = ComputeQualityScore(mean_residual, residual_max);
  const std::string quality_grade = ComputeQualityGrade(quality_score);
  const bool quality_pass = quality_score >= 70.0;

  std::cout << "==================== QUALITY SCORE ======================" << std::endl;
  std::cout << "Mean residual: " << mean_residual << std::endl;
  std::cout << "Max residual : " << residual_max << std::endl;
  std::cout << "Score        : " << quality_score << "/100" << std::endl;
  std::cout << "Grade        : " << quality_grade << std::endl;
  std::cout << "Status       : " << (quality_pass ? "PASS" : "FAIL") << std::endl;
  std::cout << "=========================================================" << std::endl;

  const std::string quality_report_path = root_debug_path + "/" + file_prefix + "handeye_quality_report.txt";
  std::ofstream quality_report(quality_report_path);
  if (quality_report.is_open())
  {
    quality_report << "mean_residual=" << mean_residual << "\n";
    quality_report << "max_residual=" << residual_max << "\n";
    quality_report << "quality_score=" << quality_score << "\n";
    quality_report << "quality_grade=" << quality_grade << "\n";
    quality_report << "quality_status=" << (quality_pass ? "PASS" : "FAIL") << "\n";
  }
  else
  {
    std::cerr << "Warning: Unable to write quality report: " << quality_report_path << std::endl;
  }

  return kEXIT_VAL_SUCCESS;
}
