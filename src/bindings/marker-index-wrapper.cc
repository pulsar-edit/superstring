#include <unordered_map>

#include "v8.h"
#include "napi.h"
#include "marker-index-wrapper.h"
#include "marker-index.h"
#include "optional.h"
#include "point-wrapper.h"
#include "range.h"
#include "util.h"

using namespace Napi;
using std::unordered_map;

FunctionReference MarkerIndexWrapper::constructor;

void MarkerIndexWrapper::init(Object exports) {
  Napi::Env env = exports.Env();
  Napi::Function func = DefineClass(env, "MarkerIndex", {
    InstanceMethod("generateRandomNumber", &MarkerIndexWrapper::generate_random_number),
    InstanceMethod("insert", &MarkerIndexWrapper::insert),
    InstanceMethod("setExclusive", &MarkerIndexWrapper::set_exclusive),
    InstanceMethod("remove", &MarkerIndexWrapper::remove),
    InstanceMethod("has", &MarkerIndexWrapper::has),
    InstanceMethod("splice", &MarkerIndexWrapper::splice),
    InstanceMethod("getStart", &MarkerIndexWrapper::get_start),
    InstanceMethod("getEnd", &MarkerIndexWrapper::get_end),
    InstanceMethod("getRange", &MarkerIndexWrapper::get_range),
    InstanceMethod("compare", &MarkerIndexWrapper::compare),
    InstanceMethod("findIntersecting", &MarkerIndexWrapper::find_intersecting),
    InstanceMethod("findContaining", &MarkerIndexWrapper::find_containing),
    InstanceMethod("findContainedIn", &MarkerIndexWrapper::find_contained_in),
    InstanceMethod("findStartingIn", &MarkerIndexWrapper::find_starting_in),
    InstanceMethod("findStartingAt", &MarkerIndexWrapper::find_starting_at),
    InstanceMethod("findEndingIn", &MarkerIndexWrapper::find_ending_in),
    InstanceMethod("findEndingAt", &MarkerIndexWrapper::find_ending_at),
    InstanceMethod("findBoundariesAfter", &MarkerIndexWrapper::find_boundaries_after),
    InstanceMethod("dump", &MarkerIndexWrapper::dump),
  });

  constructor.Reset(func, 1);
  exports.Set("MarkerIndex", func);
}

MarkerIndex *MarkerIndexWrapper::from_js(Napi::Value value) {
  if (!value.IsObject()) {
    return nullptr;
  }

  return Unwrap(value.As<Object>())->marker_index.get();
}

MarkerIndexWrapper::MarkerIndexWrapper(const CallbackInfo &info): ObjectWrap<MarkerIndexWrapper>(info) {
  unsigned seed = (info.Length() > 0 && info[0].IsNumber()) ? info[0].As<Number>().Uint32Value() : 0u;
  marker_index.reset(new MarkerIndex(seed));
}

Napi::Value MarkerIndexWrapper::generate_random_number(const CallbackInfo &info) {
  auto env = info.Env();
  return Number::New(env, marker_index->generate_random_number());
}

Napi::Value MarkerIndexWrapper::marker_ids_set_to_js(const MarkerIndex::MarkerIdSet &marker_ids) {
  v8::Isolate *isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Set> js_set = v8::Set::New(isolate);

  for (MarkerIndex::MarkerId id : marker_ids) {
    (void)js_set->Add(context, v8::Integer::New(isolate, id));
  }

  return Napi::Value(Env(), JsValueFromV8LocalValue(js_set));
}

Array MarkerIndexWrapper::marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids) {
  Array js_array = Array::New(Env(), marker_ids.size());

  for (size_t i = 0; i < marker_ids.size(); i++) {
    js_array[i] = marker_ids[i];
  }
  return js_array;
}

Object MarkerIndexWrapper::snapshot_to_js(const unordered_map<MarkerIndex::MarkerId, Range> &snapshot) {
  auto env = Env();
  Object result_object = Object::New(env);
  for (auto &pair : snapshot) {
    Object range = Object::New(Env());
    range.Set("start", PointWrapper::from_point(env, pair.second.start));
    range.Set("end", PointWrapper::from_point(env, pair.second.end));
    result_object.Set(pair.first, range);
  }
  return result_object;
}

optional<MarkerIndex::MarkerId> MarkerIndexWrapper::marker_id_from_js(Napi::Value value) {
  auto result = unsigned_from_js(value);
  if (result) {
    return *result;
  } else {
    return optional<MarkerIndex::MarkerId>{};
  }
}

optional<unsigned> MarkerIndexWrapper::unsigned_from_js(Napi::Value value) {
  if (!value.IsNumber()) {
    Error::New(Env(), "Expected an non-negative integer value.").ThrowAsJavaScriptException();
    return optional<unsigned>{};
  }

  return value.As<Number>().Uint32Value();
}

optional<bool> MarkerIndexWrapper::bool_from_js(Napi::Value value) {
  if (!value.IsBoolean()) {
    Error::New(Env(), "Expected an boolean.").ThrowAsJavaScriptException();
    return optional<bool>{};
  }

  return value.As<Boolean>().Value();
}

void MarkerIndexWrapper::insert(const CallbackInfo &info) {
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  optional<Point> start = PointWrapper::point_from_js(info[1]);
  optional<Point> end = PointWrapper::point_from_js(info[2]);

  if (id && start && end) {
    this->marker_index->insert(*id, *start, *end);
  }
}

void MarkerIndexWrapper::set_exclusive(const CallbackInfo &info) {
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  optional<bool> exclusive = bool_from_js(info[1]);

  if (id && exclusive) {
    this->marker_index->set_exclusive(*id, *exclusive);
  }
}

void MarkerIndexWrapper::remove(const CallbackInfo &info) {
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    this->marker_index->remove(*id);
  }
}

Napi::Value MarkerIndexWrapper::has(const CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    bool result = this->marker_index->has(*id);
    return Boolean::New(env, result);
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::splice(const CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> old_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> new_extent = PointWrapper::point_from_js(info[2]);
  if (start && old_extent && new_extent) {
    MarkerIndex::SpliceResult result = this->marker_index->splice(*start, *old_extent, *new_extent);

    Object invalidated = Object::New(env);
    invalidated.Set("touch", marker_ids_set_to_js(result.touch));
    invalidated.Set("inside", marker_ids_set_to_js(result.inside));
    invalidated.Set("inside", marker_ids_set_to_js(result.inside));
    invalidated.Set("overlap", marker_ids_set_to_js(result.overlap));
    invalidated.Set("surround", marker_ids_set_to_js(result.surround));
    return invalidated;
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::get_start(const CallbackInfo &info) {
  auto env = Env();

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Point result = this->marker_index->get_start(*id);
    return PointWrapper::from_point(env, result);
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::get_end(const CallbackInfo &info) {
  auto env = Env();

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Point result = this->marker_index->get_end(*id);
    return PointWrapper::from_point(env, result);
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::get_range(const CallbackInfo &info) {
  auto env = Env();

  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Range range = this->marker_index->get_range(*id);
    auto result = Object::New(env);
    result.Set("start", PointWrapper::from_point(env, range.start));
    result.Set("end", PointWrapper::from_point(env, range.end));
    return result;
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::compare(const CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id1 = marker_id_from_js(info[0]);
  optional<MarkerIndex::MarkerId> id2 = marker_id_from_js(info[1]);
  if (id1 && id2) {
    return Number::New(env, this->marker_index->compare(*id1, *id2));
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_intersecting(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_intersecting(*start, *end);
    return marker_ids_set_to_js(result);
  }

  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_containing(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_containing(*start, *end);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_contained_in(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_contained_in(*start, *end);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_starting_in(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_starting_in(*start, *end);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_starting_at(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_starting_at(*position);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_ending_in(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_ending_in(*start, *end);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_ending_at(const CallbackInfo &info) {
  auto env = info.Env();

  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = this->marker_index->find_ending_at(*position);
    return marker_ids_set_to_js(result);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::find_boundaries_after(const CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<size_t> max_count;
  if (info[1].IsNumber()) {
    max_count = info[1].As<Number>().Uint32Value();
  }

  if (start && max_count) {
    MarkerIndex::BoundaryQueryResult result = this->marker_index->find_boundaries_after(*start, *max_count);
    Object js_result = Object::New(env);
    js_result.Set("containingStart", marker_ids_vector_to_js(result.containing_start));

    Array js_boundaries = Array::New(env, result.boundaries.size());
    for (size_t i = 0; i < result.boundaries.size(); i++) {
      MarkerIndex::Boundary boundary = result.boundaries[i];
      Object js_boundary = Object::New(env);
      js_boundary.Set("position", PointWrapper::from_point(env, boundary.position));
      js_boundary.Set("starting", marker_ids_set_to_js(boundary.starting));
      js_boundary.Set("ending", marker_ids_set_to_js(boundary.ending));
      js_boundaries[i] = js_boundary;
    }
    js_result.Set("boundaries", js_boundaries);

    return js_result;
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::dump(const CallbackInfo &info) {
  unordered_map<MarkerIndex::MarkerId, Range> snapshot = this->marker_index->dump();
  return snapshot_to_js(snapshot);
}
