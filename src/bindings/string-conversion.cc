#include "string-conversion.h"
#include "text.h"

using namespace Napi;
using std::u16string;

optional<u16string> string_conversion::string_from_js(Value value) {
  Env env = value.Env();
  if (!value.IsString()) {
    Error::New(env, "Expected a string.").ThrowAsJavaScriptException();
    return optional<u16string>{};
  }

  String string = value.As<String>();
  return optional<u16string>{string};
}

String string_conversion::string_to_js(const Env env, const u16string &text, const char *failure_message) {
  return String::New(env, text);
}

String string_conversion::char_to_js(const Env env, const uint16_t c, const char *failure_message) {
  return String::New(env, u16string{c});
}
