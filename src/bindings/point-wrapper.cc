#include "point-wrapper.h"
#include "napi.h"
#include <climits>
#include <iostream>

// #include <iostream>
auto Inf = std::numeric_limits<float>::infinity();
optional<Point> PointWrapper::point_from_js(Napi::Value value) {
  Napi::Env env = value.Env();
  if (!value.IsObject()) {
    Napi::TypeError::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return optional<Point>();
  }
  Napi::Object object = value.ToObject();

  auto js_row = object.Get("row");
  auto js_column = object.Get("column");;
  if (!js_row.IsNumber() || !js_column.IsNumber()) {
    Napi::TypeError::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return optional<Point>();
  }

  auto row = js_row.ToNumber().DoubleValue();
  if(row == Inf) row = UINT_MAX;
  auto col = js_column.ToNumber().DoubleValue();
  if(col == Inf) col = UINT_MAX;
  return Point(row, col);
}

Napi::FunctionReference *PointWrapper::constructor;
void PointWrapper::init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Point", {
    InstanceAccessor<&PointWrapper::get_row>("row", napi_default_jsproperty),
    InstanceAccessor<&PointWrapper::get_column>("column", napi_default_jsproperty),
    InstanceMethod<&PointWrapper::inspect>("inspect"),
    InstanceMethod<&PointWrapper::inspect>("toString"),
    InstanceMethod<&PointWrapper::to_json>("toJSON"),
  });

  constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("Point", func);
}

Napi::Value PointWrapper::from_point(Napi::Env env, Point point) {
  auto wrapped = Napi::External<Point>::New(env, &point);
  auto values = std::initializer_list<napi_value> { wrapped };
  return PointWrapper::constructor->New(values);
}

PointWrapper::PointWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<PointWrapper>(info) {
  if(info[0].IsExternal()) {
    auto point = info[0].As<Napi::External<Point>>();
    this->point = *point.Data();
  } else {
    auto maybe_point = PointWrapper::point_from_js(info[0]);
    if(maybe_point) {
      this->point = *maybe_point;
    }
  }
}

Napi::Value PointWrapper::get_row(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), point.row);
}
Napi::Value PointWrapper::get_column(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), point.column);
}
Napi::Value PointWrapper::to_json(const Napi::CallbackInfo &info) {
  auto json = Napi::Object::New(info.Env());
  json.Set("row", this->point.row);
  json.Set("column", this->point.column);
  return json;
}
Napi::Value PointWrapper::inspect(const Napi::CallbackInfo &info) {
  auto obj = this->to_json(info);
  // auto json = Napi::Object::New(info.Env());
  // json.Set("row", this->point.row);
  // json.Set("column", this->point.column);
  return obj.ToString();
}
