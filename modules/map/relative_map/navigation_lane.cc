/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/map/relative_map/navigation_lane.h"

#include "modules/common/log.h"
#include "modules/common/math/math_utils.h"
#include "modules/common/vehicle_state/vehicle_state_provider.h"

namespace apollo {
namespace relative_map {

using apollo::perception::PerceptionObstacles;
using apollo::common::VehicleStateProvider;

bool NavigationLane::Update(const PerceptionObstacles& perception_obstacles) {
  // udpate perception_obstacles_
  perception_obstacles_ = perception_obstacles;
  if (!perception_obstacles.has_lane_marker()) {
    AERROR << "No lane marker in perception_obstacles.";
    return false;
  }

  // update adc_state_ from VehicleStateProvider
  adc_state_ = VehicleStateProvider::instance()->vehicle_state();

  // TODO(All): lane_marker --> navigation path

  navigation_path_.Clear();
  auto* path = navigation_path_.mutable_path();
  ConvertLaneMarkerToPath(perception_obstacles_.lane_marker(), path);
  return true;
}

double NavigationLane::EvaluateCubicPolynomial(const double c0, const double c1,
                                               const double c2, const double c3,
                                               const double z) const {
  return c3 * std::pow(z, 3) + c2 * std::pow(z, 2) + c1 * z + c0;
}

void NavigationLane::ConvertLaneMarkerToPath(
    const perception::LaneMarkers& lane_marker, common::Path* path) {
  const auto& left_lane = lane_marker.left_lane_marker();
  const auto& right_lane = lane_marker.right_lane_marker();

  const double unit_z = 1.0;
  double accumulated_s = 0.0;
  for (double z = 0;
       z <= std::fmin(left_lane.view_range(), right_lane.view_range());
       z += unit_z) {
    const double x_l = EvaluateCubicPolynomial(
        left_lane.c0_position(), left_lane.c1_heading_angle(),
        left_lane.c2_curvature(), left_lane.c3_curvature_derivative(), z);
    const double x_r = EvaluateCubicPolynomial(
        right_lane.c0_position(), right_lane.c1_heading_angle(),
        right_lane.c2_curvature(), right_lane.c3_curvature_derivative(), z);

    double x1 = 0.0;
    double y1 = 0.0;
    // rotate from vehicle axis to x-y axis
    common::math::RotateAxis(-adc_state_.heading(), z, (x_l + x_r) / 2.0, &x1,
                             &y1);

    // shift to get point on x-y axis
    x1 += adc_state_.x();
    y1 += adc_state_.y();

    auto* point = path->add_path_point();
    point->set_x(x1);
    point->set_y(y1);
    point->set_s(accumulated_s);
    accumulated_s += std::hypot(x1, y1);
  }
}

}  // namespace relative_map
}  // namespace apollo