#ifndef SUPERSTRING_POINT_WRAPPER_H
#define SUPERSTRING_POINT_WRAPPER_H

#include "napi.h"
#include "optional.h"
#include "point.h"

class PointWrapper {
public:
  static Napi::Value from_point(Napi::Env env, Point point);
  static optional<Point> point_from_js(Napi::Value);
};

#endif // SUPERSTRING_POINT_WRAPPER_H
