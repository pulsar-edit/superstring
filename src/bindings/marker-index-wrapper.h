#include "napi.h"
#include "marker-index.h"
#include "optional.h"
#include "range.h"

class MarkerIndexWrapper : public Napi::ObjectWrap<MarkerIndexWrapper> {
public:
  static void init(Napi::Env env, Napi::Object exports);
  MarkerIndexWrapper(const Napi::CallbackInfo& info);
//   static void init(v8::Local<v8::Object> exports);
  // static MarkerIndex *from_js(v8::Local<v8::Value>);
//
private:
  Napi::Value splice(const Napi::CallbackInfo &info);
//   static void generate_random_number(const Nan::FunctionCallbackInfo<v8::Value> &info);
//   static bool is_finite(v8::Local<v8::Integer> number);
  static Napi::Object marker_ids_set_to_js(Napi::Env env, const MarkerIndex::MarkerIdSet &marker_ids);
  Napi::Value has(const Napi::CallbackInfo &info);
  static optional<MarkerIndex::MarkerId> marker_id_from_js(Napi::Value value);
  Napi::Value insert(const Napi::CallbackInfo &info);
  Napi::Value compare(const Napi::CallbackInfo &info);
  Napi::Value get_start(const Napi::CallbackInfo &info);
  Napi::Value get_end(const Napi::CallbackInfo &info);
  Napi::Value get_range(const Napi::CallbackInfo &info);
  Napi::Value set_exclusive(const Napi::CallbackInfo &info);
  Napi::Value remove(const Napi::CallbackInfo &info);
  Napi::Value find_ending_at(const Napi::CallbackInfo &info);
  Napi::Value find_ending_in(const Napi::CallbackInfo &info);
  Napi::Value find_starting_at(const Napi::CallbackInfo &info);
  Napi::Value find_starting_in(const Napi::CallbackInfo &info);
  Napi::Value dump(const Napi::CallbackInfo &info);
  Napi::Value find_intersecting(const Napi::CallbackInfo &info);
  Napi::Value find_containing(const Napi::CallbackInfo &info);
//   static v8::Local<v8::Array> marker_ids_vector_to_js(const std::vector<MarkerIndex::MarkerId> &marker_ids);
//   static v8::Local<v8::Object> snapshot_to_js(const std::unordered_map<MarkerIndex::MarkerId, Range> &snapshot);
//   static void find_contained_in(const Nan::FunctionCallbackInfo<v8::Value> &info);
//   static void find_boundaries_after(const Nan::FunctionCallbackInfo<v8::Value> &info);
//   MarkerIndexWrapper(unsigned seed);
  MarkerIndex marker_index;
  static Napi::FunctionReference *constructor;
};
