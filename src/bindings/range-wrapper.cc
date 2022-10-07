#include "range-wrapper.h"
#include "point-wrapper.h"
#include "napi.h"

std::optional<Range> RangeWrapper::range_from_js(Napi::Value value) {
  auto env = value.Env();

  if (!value.IsObject()) {
    Napi::TypeError::New(env, "Expected an object with 'start' and 'end' properties.").ThrowAsJavaScriptException();
    return std::optional<Range>();
  }
  Napi::Object object = value.ToObject();

  auto start = PointWrapper::point_from_js(object.Get("start"));
  auto end = PointWrapper::point_from_js(object.Get("end"));
  if (start.has_value() && end.has_value()) {
    return Range{start.value(), end.value()};
  } else {
    Napi::TypeError::New(env, "Expected an object with 'start' and 'end' properties.").ThrowAsJavaScriptException();
    return std::optional<Range>();
  }
}

void RangeWrapper::init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Range", {
    InstanceAccessor<&RangeWrapper::get_start>("start"),
    InstanceAccessor<&RangeWrapper::get_end>("end")
  });
  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("Range", func);
}

// Local<Value> RangeWrapper::from_range(Range range) {
//   Local<Object> result;
//   if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
//     (new RangeWrapper(range))->Wrap(result);
//     return result;
//   } else {
//     return Nan::Null();
//   }
// }
//
RangeWrapper::RangeWrapper(const Napi::CallbackInfo& info)
      : Napi::ObjectWrap<RangeWrapper>(info) {
  auto maybe_range = RangeWrapper::range_from_js(info[0]);
  if(maybe_range.has_value()) {
    this->range = maybe_range.value();
  }
}

Napi::Value RangeWrapper::get_start(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), PointWrapper::from_point(range.start));
}
Napi::Value RangeWrapper::get_end(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), PointWrapper::from_point(range.end));
}
