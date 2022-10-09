#include <napi.h>
#include "patch.h"

class PatchWrapper : public Napi::ObjectWrap<PatchWrapper> {
 public:
  static void init(Napi::Env env, Napi::Object exports);
  // static v8::Local<v8::Value> from_patch(Patch &&);
  PatchWrapper(const Napi::CallbackInfo &info);

 private:
  static Napi::FunctionReference *constructor;
  // static void construct(const Nan::FunctionCallbackInfo<v8::Value> &info);
  Napi::Value splice(const Napi::CallbackInfo &info);
  Napi::Value splice_old(const Napi::CallbackInfo &info);
  // static void copy(const Nan::FunctionCallbackInfo<v8::Value> &info);
  Napi::Value copy(const Napi::CallbackInfo &info);
  Napi::Value invert(const Napi::CallbackInfo &info);
  Napi::Value get_changes(const Napi::CallbackInfo &info);
  Napi::Value get_changes_in_old_range(const Napi::CallbackInfo &info);
  Napi::Value get_changes_in_new_range(const Napi::CallbackInfo &info);
  Napi::Value change_for_old_position(const Napi::CallbackInfo &info);
  Napi::Value change_for_new_position(const Napi::CallbackInfo &info);
  // static void change_for_new_position(const Nan::FunctionCallbackInfo<v8::Value> &info);
  static Napi::Value deserialize(const Napi::CallbackInfo &info);
  Napi::Value serialize(const Napi::CallbackInfo &info);
  static Napi::Value compose(const Napi::CallbackInfo &info);
  // static void get_dot_graph(const Nan::FunctionCallbackInfo<v8::Value> &info);
  // static void get_json(const Nan::FunctionCallbackInfo<v8::Value> &info);
  Napi::Value get_change_count(const Napi::CallbackInfo &info);
  Napi::Value get_bounds(const Napi::CallbackInfo &info);
  Napi::Value rebalance(const Napi::CallbackInfo &info);

  Patch patch;
};
