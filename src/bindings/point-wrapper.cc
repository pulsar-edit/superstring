#include "point-wrapper.h"
#include <cmath>
#include "napi.h"

// using namespace v8;

// static Nan::Persistent<String> row_string;
// static Nan::Persistent<String> column_string;
// static Nan::Persistent<v8::Function> constructor;
//
// static uint32_t number_from_js(Napi::Value js_number) {
//   double number;
//   if(js_number.IsNumber()) {
//     number = js_number.ToNumber().DoubleValue();
//   } else {
//     number = 0;
//   }
//   if (number > 0 && !std::isfinite(number)) {
//     return UINT32_MAX;
//   } else {
//     return std::max(0.0, number);
//   }
// }

std::optional<Point> PointWrapper::point_from_js(Napi::Value value) {
  Napi::Env env = value.Env();
  if (!value.IsObject()) {
    Napi::TypeError::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return std::optional<Point>();
  }
  Napi::Object object = value.ToObject();

  auto js_row = object.Get("row");
  auto js_column = object.Get("column");;
  if (!js_row.IsNumber() || !js_column.IsNumber()) {
    Napi::TypeError::New(env, "Expected an object with 'row' and 'column' properties.").ThrowAsJavaScriptException();
    return std::optional<Point>();
  }

  return Point(js_row.ToNumber().DoubleValue(), js_column.ToNumber().DoubleValue());
}

void PointWrapper::init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Point", {
    InstanceAccessor<&PointWrapper::get_row>("row"),
    InstanceAccessor<&PointWrapper::get_column>("column")
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  exports.Set("Point", func);

}

// Local<Value> PointWrapper::from_point(Point point) {
//   Local<Object> result;
//   if (Nan::New(constructor)->NewInstance(Nan::GetCurrentContext()).ToLocal(&result)) {
//     (new PointWrapper(point))->Wrap(result);
//     return result;
//   } else {
//     return Nan::Null();
//   }
// }
//
PointWrapper::PointWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<PointWrapper>(info) {
  auto maybe_point = PointWrapper::point_from_js(info[0]);
  if(maybe_point.has_value()) {
    this->point = maybe_point.value();
  }
}

// PointWrapper::PointWrapper(Point point) : point(point) {}
//
// void PointWrapper::construct(const Nan::FunctionCallbackInfo<Value> &info) {}
//
Napi::Value PointWrapper::get_row(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), point.row);
}
Napi::Value PointWrapper::get_column(const Napi::CallbackInfo &info) {
  return Napi::Value::From(info.Env(), point.column);
}
