#ifndef SUPERSTRING_RANGE_WRAPPER_H
#define SUPERSTRING_RANGE_WRAPPER_H

#include "napi.h"
#include "optional.h"
#include "point.h"
#include "range.h"

class RangeWrapper : public Napi::ObjectWrap<RangeWrapper> {
public:
  static Napi::Value from_range(Napi::Env, Range);
  static optional<Range> range_from_js(Napi::Value);
};

#endif // SUPERSTRING_RANGE_WRAPPER_H
