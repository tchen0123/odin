#include <iostream>
#include <stdexcept>

#include <valhalla/midgard/util.h>
#include <valhalla/midgard/logging.h>

#include "odin/maneuversbuilder.h"

using namespace valhalla::midgard;

namespace valhalla {
namespace odin {

ManeuversBuilder::ManeuversBuilder(EnhancedTripPath* etp)
    : trip_path_(etp) {
}

std::list<Maneuver> ManeuversBuilder::Build() {
  // Create the maneuvers
  std::list<Maneuver> maneuvers = Produce();

  // Combine maneuvers
  Combine(maneuvers);

  return maneuvers;
}

std::list<Maneuver> ManeuversBuilder::Produce() {
  std::list<Maneuver> maneuvers;

  // Validate trip path node list
  if (trip_path_->node_size() < 1) {
    throw std::runtime_error("Trip path does not have any nodes");
  }

  // Process the Destination maneuver
  maneuvers.emplace_front();
  CreateDestinationManeuver(maneuvers.front());

  // TODO - handle no edges

  // Initialize maneuver prior to loop
  maneuvers.emplace_front();
  InitializeManeuver(maneuvers.front(), trip_path_->GetLastNodeIndex());

  // Step through nodes in reverse order to produce maneuvers
  // excluding the last and first nodes
  for (int i = (trip_path_->GetLastNodeIndex() - 1); i > 0; --i) {

#ifdef LOGGING_LEVEL_TRACE
    auto* prevEdge = trip_path_->GetPrevEdge(i);
    auto* currEdge = trip_path_->GetCurrEdge(i);
    auto* nextEdge = trip_path_->GetNextEdge(i);
    LOG_TRACE(std::to_string(i) + ":  ");
    LOG_TRACE(std::string("  prevEdge=") + (prevEdge ? (prevEdge->name_size() > 0 ? prevEdge->name(0) : "unnamed") : "NONE"));
    LOG_TRACE(std::string("  currEdge=") + (currEdge ? (currEdge->name_size() > 0 ? currEdge->name(0) : "unnamed") : "NONE"));
    LOG_TRACE(std::string("  nextEdge=") + (nextEdge ? (nextEdge->name_size() > 0 ? nextEdge->name(0) : "unnamed") : "NONE"));
    LOG_TRACE("---------------------------------------------");
#endif


    if (CanManeuverIncludePrevEdge(maneuvers.front(), i)) {
      UpdateManeuver(maneuvers.front(), i);
    } else {
      // Finalize current maneuver
      FinalizeManeuver(maneuvers.front(), i);

      // Initialize new maneuver
      maneuvers.emplace_front(Maneuver());
      InitializeManeuver(maneuvers.front(), i);
    }
  }

  // Process the Start maneuver
  CreateStartManeuver(maneuvers.front());

  return maneuvers;
}

void ManeuversBuilder::Combine(std::list<Maneuver>& maneuvers) {
  // TODO - implement
}

void ManeuversBuilder::CreateDestinationManeuver(Maneuver& maneuver) {
  int nodeIndex = trip_path_->GetLastNodeIndex();

  // Set the destination maneuver type
  // TODO - side of street
  maneuver.set_type(TripDirections_Maneuver_Type_kDestination);

  // Set the begin and end node index
  maneuver.set_begin_node_index(nodeIndex);
  maneuver.set_end_node_index(nodeIndex);

  // Set the begin and end shape index
  auto* prevEdge = trip_path_->GetPrevEdge(nodeIndex);
  maneuver.set_begin_shape_index(prevEdge->end_shape_index());
  maneuver.set_end_shape_index(prevEdge->end_shape_index());

}

void ManeuversBuilder::CreateStartManeuver(Maneuver& maneuver) {
  int nodeIndex = 0;

  // Set the start maneuver type
  // TODO - side of street
  maneuver.set_type(TripDirections_Maneuver_Type_kStart);

  FinalizeManeuver(maneuver, nodeIndex);
}

void ManeuversBuilder::InitializeManeuver(Maneuver& maneuver, int nodeIndex) {

  auto* prevEdge = trip_path_->GetPrevEdge(nodeIndex);

  // Set the end heading
  maneuver.set_end_heading(prevEdge->end_heading());

  // Set the end node index
  maneuver.set_end_node_index(nodeIndex);

  // Set the end shape index
  maneuver.set_end_shape_index(prevEdge->end_shape_index());

  // TODO - what about street names; maybe check name flag
  UpdateManeuver(maneuver, nodeIndex);
}

void ManeuversBuilder::UpdateManeuver(Maneuver& maneuver, int nodeIndex) {

  // Set the begin and end shape index
  auto* prevEdge = trip_path_->GetPrevEdge(nodeIndex);

  // Street names
  // TODO - improve
  if (maneuver.street_names().empty()) {
    auto* names = maneuver.mutable_street_names();
    for (const auto& name : prevEdge->name()) {
      names->emplace_back(name);
    }
  } else {
    // TODO
  }

  // Distance
  maneuver.set_distance(maneuver.distance() + prevEdge->length());

  // Time
  maneuver.set_time(
      maneuver.time() + GetTime(prevEdge->length(), prevEdge->speed()));

  // Ramp
  if (prevEdge->ramp()) {
    maneuver.set_ramp(true);
  }

  // Portions Toll
  if (prevEdge->toll()) {
    maneuver.set_portions_toll(true);
  }

  // Portions unpaved
  if (prevEdge->unpaved()) {
    maneuver.set_portions_unpaved(true);
  }

}

void ManeuversBuilder::FinalizeManeuver(Maneuver& maneuver, int nodeIndex) {
  auto* currEdge = trip_path_->GetCurrEdge(nodeIndex);

  // if possible, set the turn degree
  auto* prevEdge = trip_path_->GetPrevEdge(nodeIndex);
  if (prevEdge) {
    maneuver.set_turn_degree(
        GetTurnDegree(prevEdge->end_heading(), currEdge->begin_heading()));
  }

  // Set the maneuver type
  SetManeuverType(maneuver, nodeIndex);

  // Begin street names
  // TODO

  // Set begin cardinal direction
  maneuver.set_begin_cardinal_direction(
      DetermineCardinalDirection(currEdge->begin_heading()));

  // Set the begin heading
  maneuver.set_begin_heading(currEdge->begin_heading());

  // Set the begin node index
  maneuver.set_begin_node_index(nodeIndex);

  // Set the begin shape index
  maneuver.set_begin_shape_index(currEdge->begin_shape_index());

}

void ManeuversBuilder::SetManeuverType(Maneuver& maneuver, int nodeIndex) {
  // If the type is already set then just return
  if (maneuver.type() != TripDirections_Maneuver_Type_kNone) {
    return;
  }

  // TODO - iterate and expand

  SetSimpleDirectionalManeuverType(maneuver);

}

void ManeuversBuilder::SetSimpleDirectionalManeuverType(Maneuver& maneuver) {
  uint32_t turn_degree = maneuver.turn_degree();
  if ((turn_degree > 349) || (turn_degree < 11)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kContinue);
  } else if ((turn_degree > 10) && (turn_degree < 45)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kSlightRight);
  } else if ((turn_degree > 44) && (turn_degree < 136)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kRight);
  } else if ((turn_degree > 135) && (turn_degree < 181)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kSharpRight);
  } else if ((turn_degree > 180) && (turn_degree < 225)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kSharpLeft);
  } else if ((turn_degree > 224) && (turn_degree < 316)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kLeft);
  } else if ((turn_degree > 315) && (turn_degree < 350)) {
    maneuver.set_type(TripDirections_Maneuver_Type_kSlightLeft);
  }
}

TripDirections_Maneuver_CardinalDirection ManeuversBuilder::DetermineCardinalDirection(
    uint32_t heading) {
  if ((heading > 336) || (heading < 24)) {
    return TripDirections_Maneuver_CardinalDirection_kNorth;
  } else if ((heading > 23) && (heading < 67)) {
    return TripDirections_Maneuver_CardinalDirection_kNorthEast;
  } else if ((heading > 66) && (heading < 114)) {
    return TripDirections_Maneuver_CardinalDirection_kEast;
  } else if ((heading > 113) && (heading < 157)) {
    return TripDirections_Maneuver_CardinalDirection_kSouthEast;
  } else if ((heading > 156) && (heading < 204)) {
    return TripDirections_Maneuver_CardinalDirection_kSouth;
  } else if ((heading > 203) && (heading < 247)) {
    return TripDirections_Maneuver_CardinalDirection_kSouthWest;
  } else if ((heading > 246) && (heading < 294)) {
    return TripDirections_Maneuver_CardinalDirection_kWest;
  } else if ((heading > 293) && (heading < 337)) {
    return TripDirections_Maneuver_CardinalDirection_kNorthWest;
  }
}

bool ManeuversBuilder::CanManeuverIncludePrevEdge(Maneuver& maneuver,
                                                  int nodeIndex) {
  // TODO - fix it
  auto* prevEdge = trip_path_->GetPrevEdge(nodeIndex);

  // TODO - add to streetnames
  StreetNames new_names;
  for (const auto& name : prevEdge->name()) {
    new_names.push_back(StreetName(name));
  }

  if (new_names == maneuver.street_names()) {
    return true;
  }

  return false;

}

}
}
