#include "addon-data.h"
#include "text-buffer.h"
#include "text-buffer-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"

using namespace Napi;

void TextBufferSnapshotWrapper::init(Napi::Env env) {
  auto *data = env.GetInstanceData<AddonData>();

  Napi::Function func = DefineClass(env, "Snapshot", {
    InstanceMethod<&TextBufferSnapshotWrapper::destroy>("destroy"),
  });

  data->text_buffer_snapshot_wrapper_constructor = Napi::Persistent(func);
}

TextBufferSnapshotWrapper::TextBufferSnapshotWrapper(const CallbackInfo &info) : ObjectWrap<TextBufferSnapshotWrapper>(info) {
  if (info[0].IsObject() && info[1].IsExternal()) {
    auto js_buffer = info[0].As<Object>();
    auto js_wrapper = info[1].As<External<TextBuffer::Snapshot>>();

    js_text_buffer.Reset(js_buffer, 1);
    snapshot = js_wrapper.Data();
    slices_ = snapshot->primitive_chunks();
  }
}

TextBufferSnapshotWrapper::~TextBufferSnapshotWrapper() {
  if (snapshot) {
    delete snapshot;
  }
}

Value TextBufferSnapshotWrapper::new_instance(Napi::Env env, Object js_buffer, TextBuffer::Snapshot *snapshot) {
  auto *data = env.GetInstanceData<AddonData>();
  auto wrapper = External<TextBuffer::Snapshot>::New(env, snapshot);
  return data->text_buffer_snapshot_wrapper_constructor.New({js_buffer, wrapper});
}

void TextBufferSnapshotWrapper::destroy(const CallbackInfo &info) {
  if (this->snapshot) {
    delete this->snapshot;
    this->snapshot = nullptr;
  }
}
