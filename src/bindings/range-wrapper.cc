#include "range-wrapper.h"
#include "point-wrapper.h"


using namespace Napi;

optional<Range> RangeWrapper::range_from_js(Napi::Value value) {
  Napi::Env env = value.Env();
  if (!value.IsObject()) {
    Error::New(env, "Expected an object with 'start' and 'end' properties.").ThrowAsJavaScriptException();
    return optional<Range>{};
  }

  Object object = value.As<Object>();

  auto start = PointWrapper::point_from_js(object.Get("start"));
  auto end = PointWrapper::point_from_js(object.Get("end"));
  if (start && end) {
    return Range{*start, *end};
  } else {
    Error::New(env, "Expected an object with 'start' and 'end' properties.").ThrowAsJavaScriptException();
    return optional<Range>{};
  }
}

Value RangeWrapper::from_range(Napi::Env env, Range range) {
  Object js_range_wrapper = Object::New(env);
  js_range_wrapper.Set("start", PointWrapper::from_point(env, range.start));
  js_range_wrapper.Set("end", PointWrapper::from_point(env, range.end));
  return js_range_wrapper;
}
