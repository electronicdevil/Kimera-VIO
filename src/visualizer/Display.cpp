/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   Display.cpp
 * @brief  Class to display visualizer output
 * @author Antoni Rosinol
 */

#include "kimera-vio/visualizer/Display.h"

#include <string>

#include <glog/logging.h>

#include <opencv2/opencv.hpp>

#include "kimera-vio/utils/FilesystemUtils.h"

namespace VIO {

OpenCv3dDisplay::OpenCv3dDisplay(
    const ShutdownPipelineCallback& shutdown_pipeline_cb)
    : DisplayBase(),
      window_data_(),
      shutdown_pipeline_cb_(shutdown_pipeline_cb) {
  if (VLOG_IS_ON(2)) {
    window_data_.window_.setGlobalWarnings(true);
  } else {
    window_data_.window_.setGlobalWarnings(false);
  }
  window_data_.window_.registerKeyboardCallback(keyboardCallback,
                                                &window_data_);
  window_data_.window_.setBackgroundColor(window_data_.background_color_);
  window_data_.window_.showWidget("Coordinate Widget",
                                  cv::viz::WCoordinateSystem());

  // TODO(Toni): not sure if we need this...
  // See page 211:
  // Learning OpenCV 3: Computer Vision in C++ with the OpenCV Library
  //LOG_IF(WARNING, cv::startWindowThread() == 0)
  //    << "Could not start OpenCV window thread.";

  // TODO(Toni): perhaps we want to use these in the future
  // cv::createButton(nameb2, callbackButton,NULL,QT_CHECKBOX,0);
  // cv::createTrackbar( "track2", NULL, &value2, 255, NULL);
  // cv::setMouseCallback( "main2",on_mouse,NULL );
}

void OpenCv3dDisplay::spinOnce(VisualizerOutput::UniquePtr&& viz_output) {
  CHECK(viz_output);
  // Display 2D images.
  spin2dWindow(*viz_output);
  // Display 3D window.
  spin3dWindow(std::move(viz_output));
}

// Adds 3D widgets to the window, and displays it.
void OpenCv3dDisplay::spin3dWindow(VisualizerOutput::UniquePtr&& viz_output) {
  CHECK(viz_output);
  if (viz_output->visualization_type_ != VisualizationType::kNone) {
    if (window_data_.window_.wasStopped()) {
      // Shutdown the pipeline!
      shutdown_pipeline_cb_();
    }
    // viz_output.window_->spinOnce(1, true);
    setMeshProperties(&viz_output->widgets_);
    for (const auto& widget : viz_output->widgets_) {
      CHECK(widget.second);
      window_data_.window_.showWidget(widget.first, *(widget.second));
    }
    setFrustumPose(viz_output->frustum_pose_);
    window_data_.window_.spinOnce(1, true);
  }
}

void OpenCv3dDisplay::spin2dWindow(const VisualizerOutput& viz_output) {
  for (const ImageToDisplay& img_to_display : viz_output.images_to_display_) {
    cv::namedWindow(img_to_display.name_);
    cv::imshow(img_to_display.name_, img_to_display.image_);
  }
  VLOG(10) << "Spin Visualize 2D output.";
  cv::waitKey(1);  // Not needed because we are using startWindowThread()
}

void OpenCv3dDisplay::setFrustumPose(const cv::Affine3d& frustum_pose) {
  static const std::string kFrustum = "Camera Pose with Frustum";
  try {
    window_data_.window_.setWidgetPose(kFrustum, frustum_pose);
  } catch (...) {
    LOG(ERROR) << "Setting widget pose for " << kFrustum << " failed.";
  }
}

void OpenCv3dDisplay::setMeshProperties(WidgetsMap* widgets) {
  CHECK_NOTNULL(widgets);
  static const std::string kMesh = "Mesh";
  auto mesh_iterator = widgets->find(kMesh);
  if (mesh_iterator == widgets->end()) {
    LOG_EVERY_N(WARNING, 100) << "Missing Mesh in visualization's 3D widgets.";
    return;
  }
  WidgetPtr& mesh_widget = mesh_iterator->second;
  // Decide mesh shading style.
  switch (window_data_.mesh_shading_) {
    case 0: {
      mesh_widget->setRenderingProperty(cv::viz::SHADING,
                                        cv::viz::SHADING_FLAT);
      break;
    }
    case 1: {
      mesh_widget->setRenderingProperty(cv::viz::SHADING,
                                        cv::viz::SHADING_GOURAUD);
      break;
    }
    case 2: {
      mesh_widget->setRenderingProperty(cv::viz::SHADING,
                                        cv::viz::SHADING_PHONG);
      break;
    }
    default: { break; }
  }

  // Decide mesh representation style.
  switch (window_data_.mesh_representation_) {
    case 0: {
      mesh_widget->setRenderingProperty(cv::viz::REPRESENTATION,
                                        cv::viz::REPRESENTATION_POINTS);
      mesh_widget->setRenderingProperty(cv::viz::POINT_SIZE, 8);
      break;
    }
    case 1: {
      mesh_widget->setRenderingProperty(cv::viz::REPRESENTATION,
                                        cv::viz::REPRESENTATION_SURFACE);
      break;
    }
    case 2: {
      mesh_widget->setRenderingProperty(cv::viz::REPRESENTATION,
                                        cv::viz::REPRESENTATION_WIREFRAME);
      break;
    }
    default: { break; }
  }
  mesh_widget->setRenderingProperty(cv::viz::AMBIENT,
                                    window_data_.mesh_ambient_);
  mesh_widget->setRenderingProperty(cv::viz::LIGHTING,
                                    window_data_.mesh_lighting_);
}

void OpenCv3dDisplay::keyboardCallback(const cv::viz::KeyboardEvent& event,
                                       void* t) {
  WindowData* window_data = static_cast<WindowData*>(t);
  if (event.action == cv::viz::KeyboardEvent::Action::KEY_DOWN) {
    toggleFreezeScreenKeyboardCallback(event.code, window_data);
    setMeshRepresentation(event.code, window_data);
    setMeshShadingCallback(event.code, window_data);
    setMeshAmbientCallback(event.code, window_data);
    setMeshLightingCallback(event.code, window_data);
    getViewerPoseKeyboardCallback(event.code, window_data);
    getCurrentWindowSizeKeyboardCallback(event.code, window_data);
    getScreenshotCallback(event.code, window_data);
  }
}

// Keyboard callback to toggle freezing screen.
void OpenCv3dDisplay::toggleFreezeScreenKeyboardCallback(
    const uchar& code,
    WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 't') {
    LOG(WARNING) << "Pressing " << code << " toggles freezing screen.";
    static bool freeze = false;
    freeze = !freeze;  // Toggle.
    window_data->window_.spinOnce(1, true);
    while (!window_data->window_.wasStopped()) {
      if (freeze) {
        window_data->window_.spinOnce(1, true);
      } else {
        break;
      }
    }
  }
}

// Keyboard callback to set mesh representation.
void OpenCv3dDisplay::setMeshRepresentation(const uchar& code,
                                            WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == '0') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh representation to "
                    "a point cloud.";
    window_data->mesh_representation_ = 0u;
  } else if (code == '1') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh representation to "
                    "a mesh.";
    window_data->mesh_representation_ = 1u;
  } else if (code == '2') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh representation to "
                    "a wireframe.";
    window_data->mesh_representation_ = 2u;
  }
}

// Keyboard callback to set mesh shading.
void OpenCv3dDisplay::setMeshShadingCallback(const uchar& code,
                                             WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == '4') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh shading to "
                    "flat.";
    window_data->mesh_shading_ = 0u;
  } else if (code == '5') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh shading to "
                    "Gouraud.";
    window_data->mesh_shading_ = 1u;
  } else if (code == '6') {
    LOG(WARNING) << "Pressing " << code
                 << " sets mesh shading to "
                    "Phong.";
    window_data->mesh_shading_ = 2u;
  }
}

// Keyboard callback to set mesh ambient.
void OpenCv3dDisplay::setMeshAmbientCallback(const uchar& code,
                                             WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 'a') {
    window_data->mesh_ambient_ = !window_data->mesh_ambient_;
    LOG(WARNING) << "Pressing " << code << " toggles mesh ambient."
                 << " Now set to " << window_data->mesh_ambient_;
  }
}

// Keyboard callback to set mesh lighting.
void OpenCv3dDisplay::setMeshLightingCallback(const uchar& code,
                                              WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 'l') {
    window_data->mesh_lighting_ = !window_data->mesh_lighting_;
    LOG(WARNING) << "Pressing " << code << " toggles mesh lighting."
                 << " Now set to " << window_data->mesh_lighting_;
  }
}

// Keyboard callback to get current viewer pose.
void OpenCv3dDisplay::getViewerPoseKeyboardCallback(const uchar& code,
                                                    WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 'v') {
    LOG(INFO) << "Current viewer pose:\n"
              << "\tRodriguez vector: "
              << window_data->window_.getViewerPose().rvec()
              << "\n\tAffine matrix: "
              << window_data->window_.getViewerPose().matrix;
  }
}

// Keyboard callback to get current screen size.
void OpenCv3dDisplay::getCurrentWindowSizeKeyboardCallback(
    const uchar& code,
    WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 'w') {
    LOG(WARNING) << "Pressing " << code << " displays current window size:\n"
                 << "\theight: " << window_data->window_.getWindowSize().height
                 << "\twidth: " << window_data->window_.getWindowSize().width;
  }
}

// Keyboard callback to get screenshot of current windodw.
void OpenCv3dDisplay::getScreenshotCallback(const uchar& code,
                                            WindowData* window_data) {
  CHECK_NOTNULL(window_data);
  if (code == 's') {
    int i = 0;
    std::string filename = "screenshot_3d_window" + std::to_string(i);
    LOG(WARNING) << "Pressing " << code
                 << " takes a screenshot of the "
                    "window, saved in: " +
                        filename;
    window_data->window_.saveScreenshot(filename);
  }
}

// Record video sequence at a hardcoded directory relative to executable.
void OpenCv3dDisplay::recordVideo() {
  int i = 0u;
  const std::string dir_path = ".";
  const std::string dir_name = "3d_viz_video";
  const std::string dir_full_path = common::pathAppend(dir_path, dir_name);
  if (i == 0u) CHECK(common::createDirectory(dir_path, dir_name));
  std::string screenshot_path =
      common::pathAppend(dir_full_path, std::to_string(i));
  i++;
  LOG(WARNING) << "Recording video sequence for 3d Viz, "
               << "current frame saved in: " + screenshot_path;
  // window_data_.window_.saveScreenshot(screenshot_path);
  LOG(ERROR) << "WTF";
}

void OpenCv3dDisplay::setOffScreenRendering() {
  window_data_.window_.setOffScreenRendering();
}

}  // namespace VIO
