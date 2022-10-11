#include "marker-index-wrapper.h"
#include <unordered_map>
#include "marker-index.h"
#include "optional.h"
#include "point-wrapper.h"
#include "range.h"

// using namespace v8;
// using std::unordered_map;
//
// static Nan::Persistent<v8::FunctionTemplate> marker_index_constructor_template;
// static Nan::Persistent<String> start_string;
// static Nan::Persistent<String> end_string;
// static Nan::Persistent<String> touch_string;
// static Nan::Persistent<String> inside_string;
// static Nan::Persistent<String> overlap_string;
// static Nan::Persistent<String> surround_string;
// static Nan::Persistent<String> containing_start_string;
// static Nan::Persistent<String> boundaries_string;
// static Nan::Persistent<String> position_string;
// static Nan::Persistent<String> starting_string;
// static Nan::Persistent<String> ending_string;
//
Napi::FunctionReference *MarkerIndexWrapper::constructor;

void MarkerIndexWrapper::init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "MarkerIndex", {
    InstanceMethod<&MarkerIndexWrapper::splice>("splice"),
    InstanceMethod<&MarkerIndexWrapper::has>("has"),
    InstanceMethod<&MarkerIndexWrapper::insert>("insert"),
    InstanceMethod<&MarkerIndexWrapper::compare>("compare"),
    InstanceMethod<&MarkerIndexWrapper::get_start>("getStart"),
    InstanceMethod<&MarkerIndexWrapper::get_end>("getEnd"),
    InstanceMethod<&MarkerIndexWrapper::get_range>("getRange"),
    InstanceMethod<&MarkerIndexWrapper::set_exclusive>("setExclusive"),
    InstanceMethod<&MarkerIndexWrapper::find_starting_at>("findStartingAt"),
    InstanceMethod<&MarkerIndexWrapper::find_starting_in>("findStartingIn"),
    InstanceMethod<&MarkerIndexWrapper::find_ending_at>("findEndingAt"),
    InstanceMethod<&MarkerIndexWrapper::find_ending_in>("findEndingIn"),
    InstanceMethod<&MarkerIndexWrapper::remove>("remove"),
    InstanceMethod<&MarkerIndexWrapper::dump>("dump"),
    InstanceMethod<&MarkerIndexWrapper::find_intersecting>("findIntersecting"),
    InstanceMethod<&MarkerIndexWrapper::find_containing>("findContaining"),
  });

  constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("MarkerIndex", func);
}

#include <iostream>
MarkerIndexWrapper::MarkerIndexWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<MarkerIndexWrapper>(info) {
  // if(info[0].IsNumber()) {
  //   auto seed = info[0].ToNumber();
  //   auto index = new MarkerIndex { seed.Uint32Value() };
  //   this->marker_index = *index;
  // }
}

Napi::Value MarkerIndexWrapper::splice(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> old_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> new_extent = PointWrapper::point_from_js(info[2]);
  if (start && old_extent && new_extent) {
    MarkerIndex::SpliceResult result = marker_index.splice(
      *start, *old_extent, *new_extent
    );

    auto invalidated = Napi::Object::New(env);
    invalidated.Set("touch", marker_ids_set_to_js(env, result.touch));
    invalidated.Set("inside", marker_ids_set_to_js(env, result.inside));
    invalidated.Set("overlap", marker_ids_set_to_js(env, result.overlap));
    invalidated.Set("surround", marker_ids_set_to_js(env, result.surround));
    return invalidated;
  } else {
    return env.Undefined();
  }
}

// MarkerIndex *MarkerIndexWrapper::from_js(Local<Value> value) {
//   auto js_marker_index = Local<Object>::Cast(value);
//   if (!Nan::New(marker_index_constructor_template)->HasInstance(js_marker_index)) {
//     return nullptr;
//   }
//   return &Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(js_marker_index)->marker_index;
// }
//
// void MarkerIndexWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
//   auto seed = Nan::To<unsigned>(info[0]);
//   MarkerIndexWrapper *marker_index = new MarkerIndexWrapper(seed.IsJust() ? seed.FromJust() : 0u);
//   marker_index->Wrap(info.This());
// }
//
// void MarkerIndexWrapper::generate_random_number(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//   int random = wrapper->marker_index.generate_random_number();
//   info.GetReturnValue().Set(Nan::New<v8::Number>(random));
// }
//
Napi::Object MarkerIndexWrapper::marker_ids_set_to_js(Napi::Env env, const MarkerIndex::MarkerIdSet &marker_ids) {
  auto js_set = env.RunScript("new Set()").ToObject();
  auto add_to_set = js_set.Get("add").As<Napi::Function>();
  for (MarkerIndex::MarkerId id : marker_ids) {
    auto params = std::initializer_list<napi_value> { Napi::Number::New(env, id) };
    add_to_set.Call(js_set, params);
  }
  return js_set;
}
//
// Local<Array> MarkerIndexWrapper::marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids) {
//   Local<Array> js_array = Nan::New<Array>(marker_ids.size());
//
//   Isolate *isolate = v8::Isolate::GetCurrent();
//   Local<Context> context = isolate->GetCurrentContext();
//   for (size_t i = 0; i < marker_ids.size(); i++) {
//     js_array->Set(context, i, Nan::New<Integer>(marker_ids[i]));
//   }
//   return js_array;
// }
//

optional<MarkerIndex::MarkerId> MarkerIndexWrapper::marker_id_from_js(Napi::Value value) {
  if(!value.IsNumber() || value.ToNumber().DoubleValue() < 0) {
    Napi::TypeError::New(value.Env(), "Expected an non-negative integer value").ThrowAsJavaScriptException();
    return optional<MarkerIndex::MarkerId>();
  } else {
    auto result = value.ToNumber();
    return optional<MarkerIndex::MarkerId>(result.Uint32Value());
  }
}

Napi::Value MarkerIndexWrapper::insert(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  optional<Point> start = PointWrapper::point_from_js(info[1]);
  optional<Point> end = PointWrapper::point_from_js(info[2]);

  if (id && start && end) {
    marker_index.insert(*id, *start, *end);
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::set_exclusive(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);

  if (id && info[1].IsBoolean()) {
    marker_index.set_exclusive(*id, info[1].ToBoolean().Value());
  }
  return env.Undefined();
}

Napi::Value MarkerIndexWrapper::remove(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    marker_index.remove(*id);
  }
  return env.Undefined();
}
//
Napi::Value MarkerIndexWrapper::has(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto id = marker_id_from_js(info[0]);
  if (id) {
    bool result = marker_index.has(*id);
    return Napi::Boolean::New(env, result);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::get_start(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Range range = marker_index.get_range(*id);
    return PointWrapper::from_point(env, range.start);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::get_end(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Range range = marker_index.get_range(*id);
    return PointWrapper::from_point(env, range.end);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::get_range(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id = marker_id_from_js(info[0]);
  if (id) {
    Range range = marker_index.get_range(*id);
    auto result = Napi::Object::New(env);
    result.Set("start", PointWrapper::from_point(env, range.start));
    result.Set("end", PointWrapper::from_point(env, range.end));
    return result;
  } else {
    return env.Undefined();
  }
}
//
Napi::Value MarkerIndexWrapper::compare(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<MarkerIndex::MarkerId> id1 = marker_id_from_js(info[0]);
  optional<MarkerIndex::MarkerId> id2 = marker_id_from_js(info[1]);
  if (id1 && id2) {
    auto r = marker_index.compare(*id1, *id2);
    return Napi::Number::New(env, r);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::find_intersecting(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = marker_index.find_intersecting(*start, *end);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::find_containing(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = marker_index.find_containing(*start, *end);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}
//
// void MarkerIndexWrapper::find_containing(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_containing(*start, *end);
//     info.GetReturnValue().Set(marker_ids_set_to_js(result));
//   }
// }
//
// void MarkerIndexWrapper::find_contained_in(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_contained_in(*start, *end);
//     info.GetReturnValue().Set(marker_ids_set_to_js(result));
//   }
// }
//
// void MarkerIndexWrapper::find_starting_in(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_starting_in(*start, *end);
//     info.GetReturnValue().Set(marker_ids_set_to_js(result));
//   }
// }
//
// void MarkerIndexWrapper::find_starting_at(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> position = PointWrapper::point_from_js(info[0]);
//
//   if (position) {
//     MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_starting_at(*position);
//     info.GetReturnValue().Set(marker_ids_set_to_js(result));
//   }
// }
//
// void MarkerIndexWrapper::find_ending_in(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     MarkerIndex::MarkerIdSet result = wrapper->marker_index.find_ending_in(*start, *end);
//     info.GetReturnValue().Set(marker_ids_set_to_js(result));
//   }
// }
//

Napi::Value MarkerIndexWrapper::find_ending_at(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = marker_index.find_ending_at(*position);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::find_ending_in(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = marker_index.find_ending_in(*start, *end);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::find_starting_at(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> position = PointWrapper::point_from_js(info[0]);

  if (position) {
    MarkerIndex::MarkerIdSet result = marker_index.find_starting_at(*position);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}

Napi::Value MarkerIndexWrapper::find_starting_in(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    MarkerIndex::MarkerIdSet result = marker_index.find_starting_in(*start, *end);
    return marker_ids_set_to_js(env, result);
  } else {
    return env.Undefined();
  }
}

// void MarkerIndexWrapper::find_boundaries_after(const Nan::FunctionCallbackInfo<Value> &info) {
//   MarkerIndexWrapper *wrapper = Nan::ObjectWrap::Unwrap<MarkerIndexWrapper>(info.This());
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<size_t> max_count;
//   Local<Integer> js_max_count;
//   if (Nan::To<Integer>(info[1]).ToLocal(&js_max_count)) {
//     max_count = Nan::To<uint32_t>(js_max_count).FromMaybe(0);
//   }
//
//   if (start && max_count) {
//     MarkerIndex::BoundaryQueryResult result = wrapper->marker_index.find_boundaries_after(*start, *max_count);
//     Local<Object> js_result = Nan::New<Object>();
//     Nan::Set(js_result, Nan::New(containing_start_string), marker_ids_vector_to_js(result.containing_start));
//
//     Local<Array> js_boundaries = Nan::New<Array>(result.boundaries.size());
//     for (size_t i = 0; i < result.boundaries.size(); i++) {
//       MarkerIndex::Boundary boundary = result.boundaries[i];
//       Local<Object> js_boundary = Nan::New<Object>();
//       Nan::Set(js_boundary, Nan::New(position_string), PointWrapper::from_point(boundary.position));
//       Nan::Set(js_boundary, Nan::New(starting_string), marker_ids_set_to_js(boundary.starting));
//       Nan::Set(js_boundary, Nan::New(ending_string), marker_ids_set_to_js(boundary.ending));
//       Nan::Set(js_boundaries, i, js_boundary);
//     }
//     Nan::Set(js_result, Nan::New(boundaries_string), js_boundaries);
//
//     info.GetReturnValue().Set(js_result);
//   }
// }
//
Napi::Value MarkerIndexWrapper::dump(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto snapshot = marker_index.dump();
  auto result_object = Napi::Object::New(env);
  for (auto pair : snapshot) {
    auto range = Napi::Object::New(env);
    range.Set("start", PointWrapper::from_point(env, pair.second.start));
    range.Set("end", PointWrapper::from_point(env, pair.second.end));
    result_object.Set(pair.first, range);
  }
  return result_object;
}
