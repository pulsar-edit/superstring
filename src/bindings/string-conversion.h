#ifndef SUPERSTRING_STRING_CONVERSION_H
#define SUPERSTRING_STRING_CONVERSION_H

#include <string>

#include "napi.h"
#include "optional.h"
#include "text.h"

namespace string_conversion {
  Napi::String string_to_js(
    const Napi::Env env,
    const std::u16string &,
    const char *failure_message = nullptr
  );
  Napi::String char_to_js(
    const Napi::Env env,
    const std::uint16_t,
    const char *failure_message = nullptr
  );
  optional<std::u16string> string_from_js(Napi::Value);
};

#endif // SUPERSTRING_STRING_CONVERSION_H
