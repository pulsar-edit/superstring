#include "napi.h"
#include "patch.h"

class PatchWrapper : public Napi::ObjectWrap<PatchWrapper> {
 public:
  static void init(Napi::Env env, Napi::Object exports);
  static Napi::Value from_patch(Napi::Env, Patch &&);

  explicit PatchWrapper(const Napi::CallbackInfo &info);

 private:
  static Napi::Value deserialize(const Napi::CallbackInfo &info);
  static Napi::Value compose(const Napi::CallbackInfo &info);

  void splice(const Napi::CallbackInfo &info);
  void splice_old(const Napi::CallbackInfo &info);
  Napi::Value copy(const Napi::CallbackInfo &info);
  Napi::Value invert(const Napi::CallbackInfo &info);
  Napi::Value get_changes(const Napi::CallbackInfo &info);
  Napi::Value get_changes_in_old_range(const Napi::CallbackInfo &info);
  Napi::Value get_changes_in_new_range(const Napi::CallbackInfo &info);
  Napi::Value change_for_old_position(const Napi::CallbackInfo &info);
  Napi::Value change_for_new_position(const Napi::CallbackInfo &info);
  Napi::Value serialize(const Napi::CallbackInfo &info);
  Napi::Value get_dot_graph(const Napi::CallbackInfo &info);
  Napi::Value get_json(const Napi::CallbackInfo &info);
  Napi::Value get_change_count(const Napi::CallbackInfo &info);
  Napi::Value get_bounds(const Napi::CallbackInfo &info);
  void rebalance(const Napi::CallbackInfo &info);

  Patch patch;
};
