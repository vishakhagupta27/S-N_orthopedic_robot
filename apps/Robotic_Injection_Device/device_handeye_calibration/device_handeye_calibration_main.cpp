// STD
#include <iostream>
#include <vector>
#include <chrono>
#include <omp.h>

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

constexpr int kEXIT_VAL_SUCCESS = 0;
constexpr int kEXIT_VAL_BAD_USE = 1;

using size_type = std::size_t;

using Pt3         = Eigen::Matrix<CoordScalar,3,1>;
using Pt2         = Eigen::Matrix<CoordScalar,2,1>;

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
  po.set_arg_usage("<root regi debug H5 file source path> <root slicer H5 file source path> <exp ID file>");
  po.set_min_num_pos_args(3);

  po.add_backend_flags();
  po.add("parallel,p", ProgOpts::kSTORE_TRUE, po.get("parallel"), 
         "Enable parallel processing of experiments (default: false)");
  po.add("num-threads,t", ProgOpts::kSTORE_INT, 1,
         "Number of threads for parallel processing (default: auto)");
  po.add("show-progress", ProgOpts::kSTORE_TRUE, po.get("show_progress"),
         "Show progress bar during processing (default: false)");

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

  // Performance options
  const bool enable_parallel = po.get("parallel");
  int num_threads = po.get("num-threads");
  const bool show_progress = po.get("show_progress");
  
  // Set up OpenMP
  if (enable_parallel) {
    if (num_threads <= 0) {
      num_threads = omp_get_num_procs();
    }
    omp_set_num_threads(num_threads);
    vout << fmt::format("Parallel processing enabled with {} threads\n", num_threads);
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  const std::string root_debug_path      = po.pos_args()[0]; // Debug root path to save pnp handeye result
  const std::string root_slicer_path     = po.pos_args()[1]; // Slicer root path
  const std::string root_pnp_path        = po.pos_args()[2]; // Pnp xform root path
  const std::string deviceref_fcsv_path   = po.pos_args()[3];  // 3D device rotation center landmark path
  const std::string exp_list_path        = po.pos_args()[4]; // Source exp list file
  const std::string file_prefix          = po.pos_args()[5]; // Handeye result save prefix

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

  std::vector<std::string> exp_ID_list;
  int lineNumber = 0;
  /* Read exp ID list from file */
  {
    std::ifstream expIDFile(exp_list_path);
    // Make sure the file is open
    if(!expIDFile.is_open()) throw std::runtime_error("Could not open exp ID file");

    std::string line, csvItem;

    while(std::getline(expIDFile, line)){
      std::istringstream myline(line);
      while(getline(myline, csvItem)){
          exp_ID_list.push_back(csvItem);
      }
      lineNumber++;
    }
  }

  if(lineNumber!=exp_ID_list.size()) throw std::runtime_error("Exp ID list size mismatch!!!");

  vout << fmt::format("Processing {} experiments...\n", lineNumber);
  auto exp_start = std::chrono::high_resolution_clock::now();

  // Use vectors that thread-safe for frame collection
  std::vector<vctFrm4x4> A_frames(lineNumber);
  std::vector<vctFrm4x4> B_frames(lineNumber);
  std::vector<std::string> processed_exp_ids(lineNumber);

  // Process experiments in parallel or sequential
  #pragma omp parallel for schedule(dynamic) if(enable_parallel) collapse(1)
  for(int idx=0; idx<lineNumber; ++idx)
  {
    // Progress bar (thread-safe with critical section)
    if (show_progress && idx % std::max(1, lineNumber/10) == 0) {
      #pragma omp critical
      {
        int progress = (idx * 100) / lineNumber;
        vout << fmt::format("[Progress] {}/{}  ({}%)\n", idx, lineNumber, progress);
      }
    }

    const std::string exp_ID = exp_ID_list[idx];
    processed_exp_ids[idx] = exp_ID;

    // Read Robot End Effector transformation from h5_slicer file
    const std::string src_ureef_path = root_slicer_path + "/" + exp_ID + "/ur_eef.h5";
    H5::H5File h5_ureef(src_ureef_path, H5F_ACC_RDWR);
    H5::Group ureef_transform_group = h5_ureef.openGroup("TransformGroup");
    H5::Group ureef_group0 = ureef_transform_group.openGroup("0");
    std::vector<float> UReef_tracker = ReadVectorH5Float("TranformParameters", ureef_group0);

    FrameTransform UReef_xform = ConvertSlicerToITK(UReef_tracker);

    const std::string src_pnp_path = root_pnp_path + "/devicepnp_xform" + exp_ID + ".h5";
    FrameTransform pnp_xform = ReadITKAffineTransformFromFile(src_pnp_path);

    FrameTransform device_cam_to_ref = device_rotcen_ref * pnp_xform;
    FrameTransform device_ref_to_cam = device_cam_to_ref.inverse(); //actuation jig relative to C-arm

    vctFrm4x4 A_frame;
    vctFrm4x4 B_frame;

    for(size_type idx_col=0; idx_col<4; ++idx_col)
    {
      for(size_type idy_col=0; idy_col<4; ++idy_col)
      {
        A_frame[idx_col][idy_col] = UReef_xform(idx_col, idy_col);
        B_frame[idx_col][idy_col] = device_ref_to_cam(idx_col, idy_col);
      }
    }

    A_frames[idx] = A_frame;
    B_frames[idx] = B_frame;

    if (verbose) {
      #pragma omp critical
      {
        std::cout << fmt::format("Processed: {}\n", exp_ID);
        std::cout << fmt::format("  A frame determinant: {:.6f}\n", A_frame.det());
        std::cout << fmt::format("  B frame determinant: {:.6f}\n", B_frame.det());
      }
    }
  }

  auto exp_end = std::chrono::high_resolution_clock::now();
  auto exp_duration = std::chrono::duration_cast<std::chrono::milliseconds>(exp_end - exp_start);
  vout << fmt::format("Experiment processing completed in {:.2f}s\n", 
                      exp_duration.count() / 1000.0);

  if(A_frames.size() <= 5){
    std::cerr << "At least 5 frames are required for hand-eye calibration" << std::endl;
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

  // Cache inverse matrices to avoid redundant computation
  auto mat_start = std::chrono::high_resolution_clock::now();
  std::vector<vctFrm4x4> A_inverses(A_frames.size());
  std::vector<vctFrm4x4> B_inverses(B_frames.size());
  
  vout << "Pre-computing matrix inverses for caching...\n";
  #pragma omp parallel for if(enable_parallel) schedule(static)
  for (unsigned int i=0; i<A_frames.size(); i++) {
    A_inverses[i] = A_frames[i].Inverse();
    B_inverses[i] = B_frames[i].Inverse();
  }
  
  auto mat_end = std::chrono::high_resolution_clock::now();
  auto mat_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mat_end - mat_start);
  vout << fmt::format("Matrix inversion cache computed in {:.2f}ms\n", mat_duration.count());

  // Use cached inverses to populate matrices (faster than recomputing)
  #pragma omp parallel for if(enable_parallel) schedule(static)
  for (unsigned int i=1; i<A_frames.size(); i++) {
    AX.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(A_inverses[i-1] * A_frames[i]));
    BX.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(B_inverses[i-1] * B_frames[i]));
    AY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(A_frames[i-1] * A_inverses[i]));
    BY.Ref(4, 4, 4*i, 0).Assign(vctDoubleMat(B_frames[i-1] * B_inverses[i]));
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

  auto end_time = std::chrono::high_resolution_clock::now();
  auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  vout << fmt::format("\n{:-^60}\n", " Execution Summary ");
  vout << fmt::format("Total experiments processed: {}\n", lineNumber);
  vout << fmt::format("Parallel processing: {}\n", enable_parallel ? "ENABLED" : "DISABLED");
  if (enable_parallel) {
    vout << fmt::format("Number of threads: {}\n", omp_get_max_threads());
  }
  vout << fmt::format("Total execution time: {:.2f}s\n", total_duration.count() / 1000.0);
  vout << fmt::format("Average time per experiment: {:.2f}ms\n", 
                      static_cast<double>(total_duration.count()) / lineNumber);
  vout << fmt::format("{:-^60}\n", "");

  return kEXIT_VAL_SUCCESS;
}
