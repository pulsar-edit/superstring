#ifndef SUPERSTRING_RANGE_WRAPPER_H
#define SUPERSTRING_RANGE_WRAPPER_H

#include "napi.h"
#include "point.h"
#include "range.h"

class RangeWrapper : public Napi::ObjectWrap<RangeWrapper> {
public:
  static void init(Napi::Env env, Napi::Object exports);
  static Napi::Value from_range(Range);
  static std::optional<Range> range_from_js(Napi::Value);
  RangeWrapper(const Napi::CallbackInfo& info);

private:
  // RangeWrapper(Range);

  Napi::Value get_start(const Napi::CallbackInfo &info);
  Napi::Value get_end(const Napi::CallbackInfo &info);
  Range range;
};

#endif // SUPERSTRING_RANGE_WRAPPER_H
