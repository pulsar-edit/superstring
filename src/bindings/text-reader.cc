#include "addon-data.h"
#include "text-slice.h"
#include "text-reader.h"
#include "encoding-conversion.h"
#include "text-buffer-wrapper.h"

using std::move;
using std::string;
using namespace Napi;

void TextReader::init(Napi::Env env, Object exports) {
  auto *data = env.GetInstanceData<AddonData>();

  Napi::Function func = DefineClass(env, "TextReader", {
    InstanceMethod<&TextReader::read>("read"),
    InstanceMethod<&TextReader::end>("end"),
    InstanceMethod<&TextReader::destroy>("destroy"),
  });

  data->text_reader_constructor = Napi::Persistent(func);
  exports.Set("TextReader", func);
}

TextReader::~TextReader() {
  if (snapshot) delete snapshot;
}

TextReader::TextReader(const CallbackInfo &info):
  ObjectWrap<TextReader>(info),
  slice_index{0} {
  Object js_buffer = info[0].As<Object>();
  auto &text_buffer = TextBufferWrapper::Unwrap(js_buffer)->text_buffer;
  snapshot = text_buffer.create_snapshot();
  slices = snapshot->chunks();
  text_offset=slices[0].start_offset();

  String js_encoding_name = info[1].As<String>();
  std::string encoding_name = js_encoding_name.Utf8Value();
  auto _conversion = transcoding_to(encoding_name.c_str());
  if (!_conversion) {
    Error::New(Env(), (string("Invalid encoding name: ") + encoding_name).c_str()).ThrowAsJavaScriptException();
    return;
  }
  conversion.reset(new EncodingConversion(move(*_conversion)));

  js_text_buffer.Reset(js_buffer, 1);
}

Napi::Value TextReader::read(const CallbackInfo &info) {
  auto env = info.Env();
  TextReader *reader = this;

  if (!info[0].IsTypedArray()) {
    Error::New(env, "Expected a buffer").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto js_buffer_array = info[0].As<Uint8Array>();
  char *buffer = reinterpret_cast<char *>(js_buffer_array.Data());
  size_t buffer_length = js_buffer_array.ByteLength();
  size_t total_bytes_written = 0;

  for (;;) {
    if (reader->slice_index == reader->slices.size()) break;
    TextSlice &slice = reader->slices[reader->slice_index];
    size_t end_offset = slice.end_offset();
    size_t bytes_written = reader->conversion->encode(
      slice.text->content,
      &reader->text_offset,
      end_offset,
      buffer + total_bytes_written,
      buffer_length - total_bytes_written
    );
    if (bytes_written == 0) break;
    total_bytes_written += bytes_written;
    if (reader->text_offset == end_offset) {
      reader->slice_index++;
      if (reader->slice_index == reader->slices.size()) break;
      reader->text_offset = reader->slices[reader->slice_index].start_offset();
    }
  }

  return Number::New(env, total_bytes_written);
}

void TextReader::end(const CallbackInfo &info) {
  TextReader *reader = this;
  if (reader->snapshot) {
    reader->snapshot->flush_preceding_changes();
    delete reader->snapshot;
    reader->snapshot = nullptr;
  }
}

void TextReader::destroy(const CallbackInfo &info) {
  TextReader *reader = this;
  if (reader->snapshot) {
    delete reader->snapshot;
    reader->snapshot = nullptr;
  }
}
