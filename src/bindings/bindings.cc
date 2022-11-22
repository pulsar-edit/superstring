#include "marker-index-wrapper.h"
#include "patch-wrapper.h"
#include "range-wrapper.h"
#include "text-writer.h"
#include "text-reader.h"
#include "text-buffer-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"

using namespace Napi;

Object Init(Env env, Object exports) {
  PatchWrapper::init(exports);
  MarkerIndexWrapper::init(exports);
  TextBufferWrapper::init(exports);
  TextWriter::init(exports);
  TextReader::init(exports);
  TextBufferSnapshotWrapper::init(env);
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
