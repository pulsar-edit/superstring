#ifndef SUPERSTRING_POINT_WRAPPER_H
#define SUPERSTRING_POINT_WRAPPER_H

#include "napi.h"
#include "point.h"

class PointWrapper : public Napi::ObjectWrap<PointWrapper> {
public:
  static void init(Napi::Env env, Napi::Object exports);

  static Napi::Value from_point(Napi::Env env, Point point);
  static std::optional<Point> point_from_js(Napi::Value);
  PointWrapper(const Napi::CallbackInfo& info);

private:
  static Napi::FunctionReference *constructor;
  Napi::Value get_row(const Napi::CallbackInfo &info);
  Napi::Value get_column(const Napi::CallbackInfo &info);
  Napi::Value to_json(const Napi::CallbackInfo &info);
  Point point;
};

#endif // SUPERSTRING_POINT_WRAPPER_H
