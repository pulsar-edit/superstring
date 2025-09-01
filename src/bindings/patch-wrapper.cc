#include <memory>
#include <sstream>
#include <vector>

#include "addon-data.h"
#include "patch-wrapper.h"
#include "point-wrapper.h"
#include "string-conversion.h"

using namespace Napi;
using std::move;
using std::vector;
using std::u16string;

static const char *InvalidSpliceMessage = "Patch does not apply";

class ChangeWrapper : public ObjectWrap<ChangeWrapper> {
 public:
  static void init(Napi::Env env) {
    auto *data = env.GetInstanceData<AddonData>();
    Function func = DefineClass(env, "Change", {
      InstanceMethod<&ChangeWrapper::to_string>("toString"),
    });

    data->change_wrapper_constructor = Napi::Persistent(func);
  }

  static Napi::Value FromChange(Napi::Env env, Patch::Change change) {
    auto *data = env.GetInstanceData<AddonData>();
    auto wrapper = External<Patch::Change>::New(env, &change);
    Napi::Object js_change_wrapper = data->change_wrapper_constructor.New({wrapper});
    return js_change_wrapper;
  }

  ChangeWrapper(const CallbackInfo &info): ObjectWrap<ChangeWrapper>(info) {
    auto env = info.Env();
    if (info[0].IsExternal()) {
      auto js_wrapper = info[0].As<External<Patch::Change>>();
      change = *js_wrapper.Data();
      Object js_change = info.This().As<Object>();
      js_change.Set("oldStart", PointWrapper::from_point(env, change.old_start));
      js_change.Set("newStart", PointWrapper::from_point(env, change.new_start));
      js_change.Set("oldEnd", PointWrapper::from_point(env, change.old_end));
      js_change.Set("newEnd", PointWrapper::from_point(env, change.new_end));

      if (change.new_text) {
        js_change.Set("newText", string_conversion::string_to_js(env, change.new_text->content));
        js_change.Set("oldText", string_conversion::string_to_js(env, change.old_text->content));
      }
    }
  }

 private:
  Napi::Value to_string(const CallbackInfo &info) {
    std::stringstream result;
    result << this->change;
    return String::New(Env(), result.str());
  }

  Patch::Change change;
};

void PatchWrapper::init(Napi::Env env, Object exports) {
  auto *data = env.GetInstanceData<AddonData>();
  ChangeWrapper::init(env);


  Function func = DefineClass(env, "Patch", {
    StaticMethod<&PatchWrapper::deserialize>("deserialize"),
    StaticMethod<&PatchWrapper::compose>("compose"),
    InstanceMethod<&PatchWrapper::splice>("splice"),
    InstanceMethod<&PatchWrapper::splice_old>("spliceOld"),
    InstanceMethod<&PatchWrapper::copy>("copy"),
    InstanceMethod<&PatchWrapper::invert>("invert"),
    InstanceMethod<&PatchWrapper::get_changes>("getChanges"),
    InstanceMethod<&PatchWrapper::get_changes_in_old_range>("getChangesInOldRange"),
    InstanceMethod<&PatchWrapper::get_changes_in_new_range>("getChangesInNewRange"),
    InstanceMethod<&PatchWrapper::change_for_old_position>("changeForOldPosition"),
    InstanceMethod<&PatchWrapper::change_for_new_position>("changeForNewPosition"),
    InstanceMethod<&PatchWrapper::serialize>("serialize"),
    InstanceMethod<&PatchWrapper::get_dot_graph>("getDotGraph"),
    InstanceMethod<&PatchWrapper::get_json>("getJSON"),
    InstanceMethod<&PatchWrapper::rebalance>("rebalance"),
    InstanceMethod<&PatchWrapper::get_change_count>("getChangeCount"),
    InstanceMethod<&PatchWrapper::get_bounds>("getBounds"),
  });

  data->patch_wrapper_constructor = Napi::Persistent(func);

  exports.Set("Patch", func);
}

Napi::Value PatchWrapper::from_patch(Napi::Env env, Patch &&patch) {
  auto *data = env.GetInstanceData<AddonData>();
  auto wrapper = External<Patch>::New(env, &patch);
  Napi::Object js_patch = data->patch_wrapper_constructor.New({wrapper});

  return js_patch;
}

PatchWrapper::PatchWrapper(const CallbackInfo &info): ObjectWrap<PatchWrapper>(info) {
  if (info[0].IsExternal()) {
    auto patch = info[0].As<Napi::External<Patch>>();
    this->patch = move(*patch.Data());
    return;
  }

  bool merges_adjacent_changes = true;

  if (info[0].IsObject()) {
    Object options = info[0].As<Object>();
    if (options.Has("mergeAdjacentChanges")) {
      Napi::Value js_merge_adjacent_changes = options.Get("mergeAdjacentChanges");
      if (js_merge_adjacent_changes.IsBoolean()) {
        merges_adjacent_changes = js_merge_adjacent_changes.As<Boolean>();
      }
    }
  }

  patch = Patch{merges_adjacent_changes};
}

void PatchWrapper::splice(const CallbackInfo &info) {
  Patch &patch = this->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    optional<Text> deleted_text;
    optional<Text> inserted_text;

    if (info.Length() >= 4) {
      auto deleted_string = string_conversion::string_from_js(info[3]);
      if (!deleted_string) return;
      deleted_text = Text{move(*deleted_string)};
    }

    if (info.Length() >= 5) {
      auto inserted_string = string_conversion::string_from_js(info[4]);
      if (!inserted_string) return;
      inserted_text = Text{move(*inserted_string)};
    }

    if (!patch.splice(
      *start,
      *deletion_extent,
      *insertion_extent,
      move(deleted_text),
      move(inserted_text)
    )) {
      Error::New(Env(), InvalidSpliceMessage).ThrowAsJavaScriptException();
    }
  }
}

void PatchWrapper::splice_old(const CallbackInfo &info) {
  Patch &patch = this->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> deletion_extent = PointWrapper::point_from_js(info[1]);
  optional<Point> insertion_extent = PointWrapper::point_from_js(info[2]);

  if (start && deletion_extent && insertion_extent) {
    patch.splice_old(*start, *deletion_extent, *insertion_extent);
  }
}

Napi::Value PatchWrapper::copy(const CallbackInfo &info) {
  return from_patch(info.Env(), patch.copy());
}

Napi::Value PatchWrapper::invert(const CallbackInfo &info) {
  return from_patch(info.Env(), patch.invert());
}

Napi::Value PatchWrapper::get_changes(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  Patch &patch = this->patch;

  Array js_result = Array::New(env);

  size_t i = 0;
  for (auto change : patch.get_changes()) {
    js_result[i++] = ChangeWrapper::FromChange(env, change);
  }

  return js_result;
}

Napi::Value PatchWrapper::get_changes_in_old_range(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  Patch &patch = this->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Array js_result = Array::New(env);

    size_t i = 0;
    for (auto change : patch.grab_changes_in_old_range(*start, *end)) {
      js_result[i++] = ChangeWrapper::FromChange(env, change);
    }

    return js_result;
  }

  return env.Undefined();
}

Napi::Value PatchWrapper::get_changes_in_new_range(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  Patch &patch = this->patch;

  optional<Point> start = PointWrapper::point_from_js(info[0]);
  optional<Point> end = PointWrapper::point_from_js(info[1]);

  if (start && end) {
    Array js_result = Array::New(env);

    size_t i = 0;
    for (auto change : patch.grab_changes_in_new_range(*start, *end)) {
      js_result[i++] = ChangeWrapper::FromChange(env, change);
    }

    return js_result;
  }

  return env.Undefined();
}

Napi::Value PatchWrapper::change_for_old_position(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  Patch &patch = this->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_old_position(*start);
    if (change) {
      return ChangeWrapper::FromChange(env, *change);
    }
  }

  return env.Undefined();
}

Napi::Value PatchWrapper::change_for_new_position(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  Patch &patch = this->patch;
  optional<Point> start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_new_position(*start);
    if (change) {
      return ChangeWrapper::FromChange(env, *change);
    }
  }
  return env.Undefined();
}

Napi::Value PatchWrapper::serialize(const CallbackInfo &info) {
  Patch &patch = this->patch;

  static vector<uint8_t> output;
  output.clear();
  Serializer serializer(output);
  patch.serialize(serializer);
  return Buffer<uint8_t>::Copy(Env(), output.data(), output.size());
}

Napi::Value PatchWrapper::deserialize(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() > 0 && info[0].IsTypedArray()) {
    Uint8Array js_array = info[0].As<Uint8Array>();
    static vector<uint8_t> input;
    input.assign(js_array.Data(), js_array.Data() + js_array.ByteLength());
    Deserializer deserializer(input);
    return from_patch(env, Patch{deserializer});
  }

  return env.Undefined();
}

Napi::Value PatchWrapper::compose(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto *data = env.GetInstanceData<AddonData>();

  if (!info[0].IsArray()) {
    Error::New(env, "Compose requires an array of patches").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Array js_patches = info[0].As<Array>();
  Patch combination;
  bool left_to_right = true;
  for (uint32_t i = 0, n = js_patches.Length(); i < n; i++) {
    Napi::Value js_patch_v = js_patches[i];
    if (!js_patch_v.IsObject()) {
      Error::New(env, "Patch.compose must be called with an array of patches").ThrowAsJavaScriptException();
      return env.Undefined();
    }

    Object js_patch = js_patch_v.As<Object>();
    if (!js_patch.InstanceOf(data->patch_wrapper_constructor.Value())) {
      Error::New(env, "Patch.compose must be called with an array of patches").ThrowAsJavaScriptException();
      return env.Undefined();
    }

    Patch &patch = Unwrap(js_patch)->patch;
    if (!combination.combine(patch, left_to_right)) {
      TypeError::New(env, InvalidSpliceMessage).ThrowAsJavaScriptException();;
      return env.Undefined();
    }
    left_to_right = !left_to_right;
  }

  return from_patch(env, move(combination));
}

Napi::Value PatchWrapper::get_dot_graph(const CallbackInfo &info) {
  Patch &patch = this->patch;
  std::string graph = patch.get_dot_graph();
  return String::New(info.Env(), graph);
}

Napi::Value PatchWrapper::get_json(const CallbackInfo &info) {
  Patch &patch = this->patch;
  std::string graph = patch.get_json();
  return String::New(info.Env(), graph);
}

Napi::Value PatchWrapper::get_change_count(const CallbackInfo &info) {
  Patch &patch = this->patch;
  uint32_t change_count = patch.get_change_count();
  return Number::New(Env(), change_count);
}

Napi::Value PatchWrapper::get_bounds(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  Patch &patch = this->patch;
  auto bounds = patch.get_bounds();
  if (bounds) {
    return ChangeWrapper::FromChange(env, *bounds);
  }
  return env.Undefined();
}

void PatchWrapper::rebalance(const CallbackInfo &info) {
  Patch &patch = this->patch;
  patch.rebalance();
}
