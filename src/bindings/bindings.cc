#include <napi.h>
#include "marker-index-wrapper.h"
#include "patch-wrapper.h"
#include "range-wrapper.h"
#include "point-wrapper.h"
// #include "text-writer.h"
// #include "text-reader.h"
// #include "text-buffer-wrapper.h"
// #include "text-buffer-snapshot-wrapper.h"

// FIXME: Remove NAN
// using namespace v8;

// void Init(Local<Object> exports) {
//   PointWrapper::init();
//   RangeWrapper::init();
//   PatchWrapper::init(exports);
//   MarkerIndexWrapper::init(exports);
//   TextBufferWrapper::init(exports);
//   TextWriter::init(exports);
//   TextReader::init(exports);
//   TextBufferSnapshotWrapper::init();
// }
//
// NAN_MODULE_WORKER_ENABLED(superstring, Init)

// using namespace Napi;
// static Object NAPI_Init(Env env, Object exports) {
//    return exports;
// }

// NODE_MODULE_INITIALIZER(NODE_GYP_MODULE_NAME, exports);

Napi::Object InitNapi(Napi::Env env, Napi::Object exports) {
  PointWrapper::init(env, exports);
  RangeWrapper::init(env, exports);
  PatchWrapper::init(env, exports);
  MarkerIndexWrapper::init(env, exports);
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitNapi);
