#pragma once

#include "napi.h"
#include "marker-index.h"
#include "optional.h"
#include "range.h"

class MarkerIndexWrapper : public Napi::ObjectWrap<MarkerIndexWrapper> {
public:
  static void init(Napi::Env env, Napi::Object exports);
  static MarkerIndex *from_js(Napi::Value);

  explicit MarkerIndexWrapper(const Napi::CallbackInfo &info);

private:
  Napi::Value generate_random_number(const Napi::CallbackInfo &info);
  bool is_finite(Napi::Number number);
  Napi::Value marker_ids_set_to_js(const MarkerIndex::MarkerIdSet &marker_ids);
  Napi::Array marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids);
  Napi::Object snapshot_to_js(const std::unordered_map<MarkerIndex::MarkerId, Range> &snapshot);
  optional<MarkerIndex::MarkerId> marker_id_from_js(Napi::Value value);
  optional<unsigned> unsigned_from_js(Napi::Value value);
  optional<bool> bool_from_js(Napi::Value value);
  void insert(const Napi::CallbackInfo &info);
  void set_exclusive(const Napi::CallbackInfo &info);
  void remove(const Napi::CallbackInfo &info);
  Napi::Value has(const Napi::CallbackInfo &info);
  Napi::Value splice(const Napi::CallbackInfo &info);
  Napi::Value get_start(const Napi::CallbackInfo &info);
  Napi::Value get_end(const Napi::CallbackInfo &info);
  Napi::Value get_range(const Napi::CallbackInfo &info);
  Napi::Value compare(const Napi::CallbackInfo &info);
  Napi::Value find_intersecting(const Napi::CallbackInfo &info);
  Napi::Value find_containing(const Napi::CallbackInfo &info);
  Napi::Value find_contained_in(const Napi::CallbackInfo &info);
  Napi::Value find_starting_in(const Napi::CallbackInfo &info);
  Napi::Value find_starting_at(const Napi::CallbackInfo &info);
  Napi::Value find_ending_in(const Napi::CallbackInfo &info);
  Napi::Value find_ending_at(const Napi::CallbackInfo &info);
  Napi::Value find_boundaries_after(const Napi::CallbackInfo &info);
  Napi::Value dump(const Napi::CallbackInfo &info);

  std::unique_ptr<MarkerIndex> marker_index;
};
