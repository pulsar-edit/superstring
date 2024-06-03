#ifndef SUPERSTRING_NUMBER_CONVERSION_H
#define SUPERSTRING_NUMBER_CONVERSION_H

#include "napi.h"
#include "optional.h"

namespace number_conversion {
  template<typename T>
  optional<T> number_from_js(Napi::Value js_value) {
    if (js_value.IsNumber()) {
      Napi::Number js_number = js_value.As<Napi::Number>();
      return static_cast<T>(js_number);
    }
    return optional<T>{};
  }
}

#endif // SUPERSTRING_NUMBER_CONVERSION_H
