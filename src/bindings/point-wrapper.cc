#include <cmath>

#include "point-wrapper.h"

using namespace Napi;

static uint32_t number_from_js(Number js_number) {
  double number = js_number.DoubleValue();
  if (number > 0 && !std::isfinite(number)) {
    return UINT32_MAX;
  } else {
    return std::max(0.0, number);
  }
}

optional<Point> PointWrapper::point_from_js(Napi::Value value) {
  Napi::Env env = value.Env();
  if (!value.IsObject()) {
    Error::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return optional<Point>{};
  }

  Object object = value.As<Object>();
  Napi::Value maybe_row = object.Get("row");
  if (!maybe_row.IsNumber()) {
    Error::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return optional<Point>{};
  }
  Number js_row = maybe_row.As<Number>();

  Napi::Value maybe_column = object.Get("column");
  if (!maybe_column.IsNumber()) {
    Error::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return optional<Point>{};
  }
  Number js_column = maybe_column.As<Number>();
  return Point(number_from_js(js_row), number_from_js(js_column));
}

Value PointWrapper::from_point(Napi::Env env, Point point) {
  Object js_point_wrapper = Object::New(env);
  js_point_wrapper.Set("row", Number::New(env, point.row));
  js_point_wrapper.Set("column", Number::New(env, point.column));

  return js_point_wrapper;
}
