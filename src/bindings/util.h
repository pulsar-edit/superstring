#pragma once

// Copied from https://github.com/nodejs/node/blob/main/src/js_native_api_v8.h
//=== Conversion between V8 Handles and napi_value ========================

// This asserts v8::Local<> will always be implemented with a single
// pointer field so that we can pass it around as a void*.
static_assert(sizeof(v8::Local<v8::Value>) == sizeof(napi_value),
  "Cannot convert between v8::Local<v8::Value> and napi_value");

inline napi_value JsValueFromV8LocalValue(v8::Local<v8::Value> local) {
  return reinterpret_cast<napi_value>(*local);
}

inline v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v) {
  v8::Local<v8::Value> local;
  memcpy(static_cast<void*>(&local), &v, sizeof(v));
  return local;
}

// End