#include "addon-data.h"
#include "marker-index-wrapper.h"
#include "patch-wrapper.h"
#include "range-wrapper.h"
#include "text-writer.h"
#include "text-reader.h"
#include "text-buffer-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"

using namespace Napi;

Object Init(Env env, Object exports) {
  auto* data = new AddonData(env);
  env.SetInstanceData(data);

  PatchWrapper::init(env, exports);
  MarkerIndexWrapper::init(env, exports);
  TextBufferWrapper::init(env, exports);
  TextWriter::init(env, exports);
  TextReader::init(env, exports);
  TextBufferSnapshotWrapper::init(env);
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
