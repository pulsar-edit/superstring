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
    InstanceMethod<&PatchWrapper::splice>("splice"),
    InstanceMethod<&PatchWrapper::splice_old>("spliceOld"),
    InstanceMethod<&PatchWrapper::get_changes>("getChanges"),
    InstanceMethod<&PatchWrapper::get_change_count>("getChangeCount"),
    StaticMethod<&PatchWrapper::compose>("compose")
    // StaticMethod<&PatchWrapper::deserialize>("deserialize")
    // InstanceAccessor<&RangeWrapper::get_start>("start"),
    // InstanceAccessor<&RangeWrapper::get_end>("end")
  });
  constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("Patch", func);

  // Nan::SetTemplate(constructor_template_local, Nan::New("deserialize").ToLocalChecked(), Nan::New<FunctionTemplate>(deserialize), None);
  // Nan::SetTemplate(constructor_template_local, Nan::New("compose").ToLocalChecked(), Nan::New<FunctionTemplate>(compose), None);
  // constructor_template_local->InstanceTemplate()->SetInternalFieldCount(1);
  // const auto &prototype_template = constructor_template_local->PrototypeTemplate();
  // Nan::SetTemplate(prototype_template, Nan::New("delete").ToLocalChecked(), Nan::New<FunctionTemplate>(noop), None);
  // Nan::SetTemplate(prototype_template, Nan::New("splice").ToLocalChecked(), Nan::New<FunctionTemplate>(splice), None);
  // Nan::SetTemplate(prototype_template, Nan::New("spliceOld").ToLocalChecked(), Nan::New<FunctionTemplate>(splice_old), None);
  // Nan::SetTemplate(prototype_template, Nan::New("copy").ToLocalChecked(), Nan::New<FunctionTemplate>(copy), None);
  // Nan::SetTemplate(prototype_template, Nan::New("invert").ToLocalChecked(), Nan::New<FunctionTemplate>(invert), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getChanges").ToLocalChecked(), Nan::New<FunctionTemplate>(get_changes), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getChangesInOldRange").ToLocalChecked(),
  //                         Nan::New<FunctionTemplate>(get_changes_in_old_range), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getChangesInNewRange").ToLocalChecked(),
  //                         Nan::New<FunctionTemplate>(get_changes_in_new_range), None);
  // Nan::SetTemplate(prototype_template, Nan::New("changeForOldPosition").ToLocalChecked(),
  //                         Nan::New<FunctionTemplate>(change_for_old_position), None);
  // Nan::SetTemplate(prototype_template, Nan::New("changeForNewPosition").ToLocalChecked(),
  //                         Nan::New<FunctionTemplate>(change_for_new_position), None);
  // Nan::SetTemplate(prototype_template, Nan::New("serialize").ToLocalChecked(), Nan::New<FunctionTemplate>(serialize), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getDotGraph").ToLocalChecked(), Nan::New<FunctionTemplate>(get_dot_graph), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getJSON").ToLocalChecked(), Nan::New<FunctionTemplate>(get_json), None);
  // Nan::SetTemplate(prototype_template, Nan::New("rebalance").ToLocalChecked(), Nan::New<FunctionTemplate>(rebalance), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getChangeCount").ToLocalChecked(), Nan::New<FunctionTemplate>(get_change_count), None);
  // Nan::SetTemplate(prototype_template, Nan::New("getBounds").ToLocalChecked(), Nan::New<FunctionTemplate>(get_bounds), None);
  // patch_wrapper_constructor_template.Reset(constructor_template_local);
  // patch_wrapper_constructor.Reset(Nan::GetFunction(constructor_template_local).ToLocalChecked());
  // Nan::Set(exports, Nan::New("Patch").ToLocalChecked(), Nan::New(patch_wrapper_constructor));
}
//
PatchWrapper::PatchWrapper(const Napi::CallbackInfo &info) : Napi::ObjectWrap<PatchWrapper>(info) {
  if(info[0].IsExternal()) {
  } else if(info[0].IsObject()) {
    auto param = info[0].ToObject();
    if(param.Has("mergeAdjacentChanges")) {
      bool merge_adjacent = param.Get("mergeAdjacentChanges").ToBoolean();
      this->patch = Patch{ merge_adjacent };
    }
  }
}

//
// Local<Value> PatchWrapper::from_patch(Patch &&patch) {
//   Local<Object> result;
//   if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
//     (new PatchWrapper(move(patch)))->Wrap(result);
//     return result;
//   } else {
//     return Nan::Null();
//   }
// }
//
// void PatchWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {
//   bool merges_adjacent_changes = true;
//   Local<Object> options;
//
//   if (info.Length() > 0 && Nan::To<Object>(info[0]).ToLocal(&options)) {
//     Local<Value> js_merge_adjacent_changes;
//     if (Nan::Get(options, Nan::New("mergeAdjacentChanges").ToLocalChecked()).ToLocal(&js_merge_adjacent_changes)) {
//       merges_adjacent_changes = Nan::To<bool>(js_merge_adjacent_changes).FromMaybe(false);
//     }
//   }
//   PatchWrapper *patch = new PatchWrapper(Patch{merges_adjacent_changes});
//   patch->Wrap(info.This());
// }
//
// Patch from_js(
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
//
// void PatchWrapper::copy(const Nan::FunctionCallbackInfo<Value> &info) {
//   Local<Object> result;
//   if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
//     Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//     auto wrapper = new PatchWrapper{patch.copy()};
//     wrapper->Wrap(result);
//     info.GetReturnValue().Set(result);
//   }
// }
//
// void PatchWrapper::invert(const Nan::FunctionCallbackInfo<Value> &info) {
//   Local<Object> result;
//   if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
//     Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//     auto wrapper = new PatchWrapper{patch.invert()};
//     wrapper->Wrap(result);
//     info.GetReturnValue().Set(result);
//   }
// }
//
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
//
// void PatchWrapper::get_changes_in_old_range(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     Local<Array> js_result = Nan::New<Array>();
//
//     size_t i = 0;
//     for (auto change : patch.grab_changes_in_old_range(*start, *end)) {
//       Nan::Set(js_result, i++, ChangeWrapper::FromChange(change));
//     }
//
//     info.GetReturnValue().Set(js_result);
//   }
// }
//
// void PatchWrapper::get_changes_in_new_range(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   optional<Point> end = PointWrapper::point_from_js(info[1]);
//
//   if (start && end) {
//     Local<Array> js_result = Nan::New<Array>();
//
//     size_t i = 0;
//     for (auto change : patch.grab_changes_in_new_range(*start, *end)) {
//       Nan::Set(js_result, i++, ChangeWrapper::FromChange(change));
//     }
//
//     info.GetReturnValue().Set(js_result);
//   }
// }
//
// void PatchWrapper::change_for_old_position(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   if (start) {
//     auto change = patch.grab_change_starting_before_old_position(*start);
//     if (change) {
//       info.GetReturnValue().Set(ChangeWrapper::FromChange(*change));
//     } else {
//       info.GetReturnValue().Set(Nan::Undefined());
//     }
//   }
// }
//
// void PatchWrapper::change_for_new_position(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   optional<Point> start = PointWrapper::point_from_js(info[0]);
//   if (start) {
//     auto change = patch.grab_change_starting_before_new_position(*start);
//     if (change) {
//       info.GetReturnValue().Set(ChangeWrapper::FromChange(*change));
//     } else {
//       info.GetReturnValue().Set(Nan::Undefined());
//     }
//   }
// }
//
// void PatchWrapper::serialize(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//
//   static vector<uint8_t> output;
//   output.clear();
//   Serializer serializer(output);
//   patch.serialize(serializer);
//   Local<Object> result;
//   auto maybe_result =
//       Nan::CopyBuffer(reinterpret_cast<char *>(output.data()), output.size());
//   if (maybe_result.ToLocal(&result)) {
//     info.GetReturnValue().Set(result);
//   }
// }
//

// Napi::Value PatchWrapper::deserialize(const Napi::CallbackInfo &info) {
//   Napi::Object result;
//   // result.S
//   // // Local<Object> result;
//   // if (Nan::NewInstance(Nan::New(patch_wrapper_constructor)).ToLocal(&result)) {
//   //   if (info[0]->IsUint8Array()) {
//   //     auto *data = node::Buffer::Data(info[0]);
//   //
//   //     static vector<uint8_t> input;
//   //     input.assign(data, data + node::Buffer::Length(info[0]));
//   //     Deserializer deserializer(input);
//   //     PatchWrapper *wrapper = new PatchWrapper(Patch{deserializer});
//   //     wrapper->Wrap(result);
//   //     info.GetReturnValue().Set(result);
//   //   }
//   // }
//   return info.Env().Undefined();
// }
//

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
  // auto wrapped = Napi::External<Patch>::New(env, &combination);
  // auto values = std::initializer_list<napi_value> { wrapped };
  // return PatchWrapper::constructor->New(values);

  // std::cout << "SAT " << combination.get_change_count() << "\n";
  // PatchWrapper::Unwrap(w)->patch = std::move(combination);
  // std::cout << "SAT " << PatchWrapper::Unwrap(w)->patch.get_change_count() << "\n";
  // auto w = Napi::External<Patch>::New(env, &combination);
  // return new PatchWrapper { std::move(combination) };
  // return Napi::External<Patch>::New(env, &combination);

    // (new PatchWrapper{move(combination)})->Wrap(result);
    // info.GetReturnValue().Set(result);
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
  // std::cout << "PTR? " << (this->patch == nullptr) << "\n";
  std::cout << "ChangeCount " << this->patch.get_change_count() << "\n";
  return Napi::Value::From(info.Env(), this->patch.get_change_count());
}
//
// void PatchWrapper::get_bounds(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   auto bounds = patch.get_bounds();
//   if (bounds) {
//     info.GetReturnValue().Set(ChangeWrapper::FromChange(*bounds));
//   }
// }
//
// void PatchWrapper::rebalance(const Nan::FunctionCallbackInfo<Value> &info) {
//   Patch &patch = Nan::ObjectWrap::Unwrap<PatchWrapper>(info.This())->patch;
//   patch.rebalance();
// }
