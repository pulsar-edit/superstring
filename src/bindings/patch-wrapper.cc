#include <iostream>
// #include "noop.h"
#include "patch-wrapper.h"
// #include <memory>
// #include <sstream>
// #include <vector>
#include "point-wrapper.h"
// #include "string-conversion.h"
//
// using namespace v8;
// using std::vector;
// using std::u16string;
//
// static Nan::Persistent<String> new_text_string;
// static Nan::Persistent<String> old_text_string;
// static Nan::Persistent<v8::Function> change_wrapper_constructor;
// static Nan::Persistent<v8::FunctionTemplate> patch_wrapper_constructor_template;
// static Nan::Persistent<v8::Function> patch_wrapper_constructor;
//
// static const char *InvalidSpliceMessage = "Patch does not apply";

class ChangeWrapper : public Napi::ObjectWrap<ChangeWrapper> {
 public:
  static void init(Napi::Env env) {
    // new_text_string.Reset(Nan::New("newText").ToLocalChecked());
    // old_text_string.Reset(Nan::New("oldText").ToLocalChecked());
    // static Nan::Persistent<String> old_text_string;
    //
    // Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(construct);
    // constructor_template->SetClassName(Nan::New<String>("Change").ToLocalChecked());
    // constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    // const auto &instance_template = constructor_template->InstanceTemplate();
    // Nan::SetAccessor(instance_template, Nan::New("oldStart").ToLocalChecked(), get_old_start);
    // Nan::SetAccessor(instance_template, Nan::New("newStart").ToLocalChecked(), get_new_start);
    // Nan::SetAccessor(instance_template, Nan::New("oldEnd").ToLocalChecked(), get_old_end);
    // Nan::SetAccessor(instance_template, Nan::New("newEnd").ToLocalChecked(), get_new_end);
    //
    // const auto &prototype_template = constructor_template->PrototypeTemplate();
    // Nan::SetTemplate(prototype_template, Nan::New<String>("toString").ToLocalChecked(), Nan::New<FunctionTemplate>(to_string), None);
    // change_wrapper_constructor.Reset(Nan::GetFunction(constructor_template).ToLocalChecked());
  }
//
  static Napi::Value FromChange(Napi::Env env, Patch::Change change) {
    Napi::Object result = Napi::Object::New(env);
    result.Set("oldStart", PointWrapper::from_point(env, change.old_start));
    result.Set("oldEnd", PointWrapper::from_point(env, change.old_end));
    result.Set("newStart", PointWrapper::from_point(env, change.new_start));
    result.Set("newEnd", PointWrapper::from_point(env, change.new_end));

    if(change.new_text) {
      result.Set("newText", Napi::String::From(env, change.new_text->content));
    }
    if(change.old_text) {
      result.Set("oldText", Napi::String::From(env, change.old_text->content));
    }
    return result;
    // result.Set
    // Local<Object> result;
    // if (Nan::NewInstance(Nan::New(change_wrapper_constructor)).ToLocal(&result)) {
    //   (new ChangeWrapper(change))->Wrap(result);
    //   if (change.new_text) {
    //     Nan::Set(
    //       result,
    //       Nan::New(new_text_string),
    //       string_conversion::string_to_js(change.new_text->content)
    //     );
    //   }
    //   if (change.old_text) {
    //     Nan::Set(
    //       result,
    //       Nan::New(old_text_string),
    //       string_conversion::string_to_js(change.old_text->content)
    //     );
    //   }
    //   return result;
    // } else {
    //   return Nan::Null();
    // }
  }
//
//  private:
//   ChangeWrapper(Patch::Change change) : change(change) {}
//
//   static void construct(const Nan::FunctionCallbackInfo<Value> &info) {}
//
//   static void get_old_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(PointWrapper::from_point(change.old_start));
//   }
//
//   static void get_new_start(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(PointWrapper::from_point(change.new_start));
//   }
//
//   static void get_old_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(PointWrapper::from_point(change.old_end));
//   }
//
//   static void get_new_end(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(PointWrapper::from_point(change.new_end));
//   }
//
//   static void get_preceding_old_text_length(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(Nan::New<Number>(change.preceding_old_text_size));
//   }
//
//   static void get_preceding_new_text_length(v8::Local<v8::String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     info.GetReturnValue().Set(Nan::New<Number>(change.preceding_new_text_size));
//   }
//
//   static void to_string(const Nan::FunctionCallbackInfo<Value> &info) {
//     Patch::Change &change = Nan::ObjectWrap::Unwrap<ChangeWrapper>(info.This())->change;
//     std::stringstream result;
//     result << change;
//     info.GetReturnValue().Set(Nan::New<String>(result.str()).ToLocalChecked());
//   }
//
  Patch::Change change;
};

Napi::FunctionReference *PatchWrapper::constructor;
void PatchWrapper::init(Napi::Env env, Napi::Object exports) {
  ChangeWrapper::init(env);

  auto func = DefineClass(env, "Patch", {
    StaticMethod<&PatchWrapper::deserialize>("deserialize"),
    InstanceMethod<&PatchWrapper::serialize>("serialize"),
    InstanceMethod<&PatchWrapper::splice>("splice"),
    InstanceMethod<&PatchWrapper::splice_old>("spliceOld"),
    InstanceMethod<&PatchWrapper::change_for_old_position>("changeForOldPosition"),
    InstanceMethod<&PatchWrapper::change_for_new_position>("changeForNewPosition"),
    InstanceMethod<&PatchWrapper::get_changes>("getChanges"),
    InstanceMethod<&PatchWrapper::get_change_count>("getChangeCount"),
    InstanceMethod<&PatchWrapper::get_changes_in_old_range>("getChangesInOldRange"),
    InstanceMethod<&PatchWrapper::get_changes_in_new_range>("getChangesInNewRange"),
    InstanceMethod<&PatchWrapper::invert>("invert"),
    InstanceMethod<&PatchWrapper::copy>("copy"),
    InstanceMethod<&PatchWrapper::rebalance>("rebalance"),
    InstanceMethod<&PatchWrapper::get_bounds>("getBounds"),
    StaticMethod<&PatchWrapper::compose>("compose")
    // InstanceAccessor<&RangeWrapper::get_start>("start"),
    // InstanceAccessor<&RangeWrapper::get_end>("end")
  });
  constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("Patch", func);
}

PatchWrapper::PatchWrapper(const Napi::CallbackInfo &info) : Napi::ObjectWrap<PatchWrapper>(info) {
  if(info[0].IsExternal()) {
    auto patch = info[0].As<Napi::External<Patch>>();
    this->patch = std::move(*patch.Data());
  } else if(info[0].IsObject()) {
    auto param = info[0].ToObject();
    if(param.Has("mergeAdjacentChanges")) {
      bool merge_adjacent = param.Get("mergeAdjacentChanges").ToBoolean();
      this->patch = Patch{ merge_adjacent };
    }
  }
}

Napi::Value PatchWrapper::splice(const Napi::CallbackInfo &info) {
  auto start = PointWrapper::point_from_js(info[0]);
  auto deletion_extent = PointWrapper::point_from_js(info[1]);
  auto insertion_extent = PointWrapper::point_from_js(info[2]);
  auto env = info.Env();

  if (start && deletion_extent && insertion_extent) {
    optional<Text> deleted_text;
    optional<Text> inserted_text;
    if (info.Length() >= 4) {
      if(info[3].IsString()) {
        deleted_text = optional<Text>(Text{info[3].ToString().Utf16Value()});
      } else {
        return env.Undefined();
      }
    }

    if (info.Length() >= 5) {
      if(info[4].IsString()) {
        inserted_text = optional<Text>(Text{info[4].ToString().Utf16Value()});
      } else {
        return env.Undefined();
      }
    }

    if (!this->patch.splice(
      *start,
      *deletion_extent,
      *insertion_extent,
      std::move(deleted_text),
      std::move(inserted_text)
    )) {
      Napi::Error::New(env, "Patch does not apply").ThrowAsJavaScriptException();
    }
  }
  return info.Env().Undefined();
}

Napi::Value PatchWrapper::splice_old(const Napi::CallbackInfo &info) {
  auto start = PointWrapper::point_from_js(info[0]);
  auto deletion_extent = PointWrapper::point_from_js(info[1]);
  auto insertion_extent = PointWrapper::point_from_js(info[2]);
  auto env = info.Env();

  if (start && deletion_extent && insertion_extent) {
    patch.splice_old(*start, *deletion_extent, *insertion_extent);
  }
  return env.Undefined();
}

Napi::Value PatchWrapper::copy(const Napi::CallbackInfo &info) {
  auto copied = patch.copy();
  auto wrapped = Napi::External<Patch>::New(info.Env(), &copied);
  auto result = PatchWrapper::constructor->New({ wrapped });
  return result;
}

Napi::Value PatchWrapper::invert(const Napi::CallbackInfo &info) {
  auto inverted = patch.invert();
  auto wrapped = Napi::External<Patch>::New(info.Env(), &inverted);
  auto result = PatchWrapper::constructor->New({ wrapped });
  return result;
}

Napi::Value PatchWrapper::get_changes(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto t = Napi::ObjectWrap<PatchWrapper>::Unwrap(info.This().ToObject());
  auto &patch = t->patch;
  auto js_result = Napi::Array::New(env);

  size_t i = 0;
  for (auto change : patch.get_changes()) {
    js_result.Set(i++, ChangeWrapper::FromChange(env, change));
  }
  return js_result;
}

Napi::Value PatchWrapper::get_changes_in_old_range(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto start = PointWrapper::point_from_js(info[0]);
  auto end = PointWrapper::point_from_js(info[1]);
  if (start && end) {
    auto changes = patch.grab_changes_in_old_range(*start, *start);
    auto js_result = Napi::Array::New(env, changes.size());
    size_t i = 0;
    for (auto change : changes) {
      js_result.Set(i++, ChangeWrapper::FromChange(env, change));
    }
    return js_result;
  } else {
    return env.Undefined();
  }
}

Napi::Value PatchWrapper::get_changes_in_new_range(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  auto start = PointWrapper::point_from_js(info[0]);
  auto end = PointWrapper::point_from_js(info[1]);
  if (start && end) {
    auto changes = patch.grab_changes_in_new_range(*start, *start);
    auto js_result = Napi::Array::New(env, changes.size());
    size_t i = 0;
    for (auto change : changes) {
      js_result.Set(i++, ChangeWrapper::FromChange(env, change));
    }
    return js_result;
  } else {
    return env.Undefined();
  }
}

Napi::Value PatchWrapper::change_for_old_position(const Napi::CallbackInfo &info) {
  auto start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_old_position(*start);
    if (change) {
      return ChangeWrapper::FromChange(info.Env(), *change);
    }
  }
  return info.Env().Undefined();
}

Napi::Value PatchWrapper::change_for_new_position(const Napi::CallbackInfo &info) {
  auto start = PointWrapper::point_from_js(info[0]);
  if (start) {
    auto change = patch.grab_change_starting_before_new_position(*start);
    if (change) {
      return ChangeWrapper::FromChange(info.Env(), *change);
    }
  }
  return info.Env().Undefined();
}

Napi::Value PatchWrapper::serialize(const Napi::CallbackInfo &info) {
  static std::vector<uint8_t> output;
  output.clear();
  Serializer serializer(output);
  patch.serialize(serializer);
  auto result = Napi::Buffer<uint8_t>::Copy(info.Env(), output.data(), output.size());
  return result;
}


Napi::Value PatchWrapper::deserialize(const Napi::CallbackInfo &info) {
  if(info[0].IsBuffer()) {
    auto buffer = info[0].As<Napi::Buffer<uint8_t>>();
    auto *data = buffer.Data();
    static std::vector<uint8_t> input;
    input.assign(data, data + buffer.Length());
    Deserializer deserializer(input);
    auto new_patch = Patch{deserializer};
    auto wrapped = Napi::External<Patch>::New(info.Env(), &new_patch);
    return PatchWrapper::constructor->New({ wrapped });
  } else {
    return info.Env().Undefined();
  }
}

Napi::Value PatchWrapper::compose(const Napi::CallbackInfo &info) {
  auto env = info.Env();
  if(!info[0].IsArray()) {
    Napi::Error::New(env, "Compose requires an array of patches").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Array js_patches = info[0].As<Napi::Array>();


  auto values = std::initializer_list<napi_value> { };
  auto return_value = PatchWrapper::constructor->New(values);
  auto combination = &PatchWrapper::Unwrap(return_value)->patch;
  bool left_to_right = true;
  for (uint32_t i = 0, n = js_patches.Length(); i < n; i++) {
    Patch patch;
    auto obj = js_patches.Get(i).ToObject();
    if(!obj.Has("getChangeCount")) {
    // } catch (Napi::Error _) {
      Napi::Error::New(env, "Patch.compose must be called with an array of patches").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    patch = std::move(PatchWrapper::Unwrap(obj)->patch);

    if (!combination->combine(patch, left_to_right)) {
      Napi::Error::New(env, "Patch does not apply").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    left_to_right = !left_to_right;
  }

  return return_value;
}
//
// void PatchWrapper::get_dot_graph(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   std::string graph = patch.get_dot_graph();
//   info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
// }
//
// void PatchWrapper::get_json(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   std::string graph = patch.get_json();
//   info.GetReturnValue().Set(Nan::New<String>(graph).ToLocalChecked());
// }
//
Napi::Value PatchWrapper::get_change_count(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), this->patch.get_change_count());
}
//
Napi::Value PatchWrapper::get_bounds(const Napi::CallbackInfo &info) {
  auto bounds = patch.get_bounds();
  if (bounds) {
    return ChangeWrapper::FromChange(info.Env(), *bounds);
  }
  return info.Env().Undefined();
}

Napi::Value PatchWrapper::rebalance(const Napi::CallbackInfo &info) {
  patch.rebalance();
  return info.Env().Undefined();
}
