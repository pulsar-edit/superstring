#include "addon-data.h"
#include "text-writer.h"

using std::string;
using std::move;
using std::u16string;
using namespace Napi;

void TextWriter::init(Napi::Env env, Object exports) {
  auto *data = env.GetInstanceData<AddonData>();

  Napi::Function func = DefineClass(env, "TextWriter", {
    InstanceMethod<&TextWriter::write>("write"),
    InstanceMethod<&TextWriter::end>("end"),
  });

  data->text_writer_constructor = Napi::Persistent(func);
  exports.Set("TextWriter", func);
}

TextWriter::TextWriter(const CallbackInfo &info):ObjectWrap<TextWriter>(info) {
  String js_encoding_name = info[0].As<String>();
  auto encoding_name = js_encoding_name.Utf8Value();
  auto _conversion = transcoding_from(encoding_name.c_str());
  if (!_conversion) {
    Error::New(Env(), (string("Invalid encoding name: ") + encoding_name).c_str()).ThrowAsJavaScriptException();
    return;
  }
  conversion.reset(new EncodingConversion(move(*_conversion)));
}

void TextWriter::write(const CallbackInfo &info) {
  auto writer = this;

  if (info[0].IsString()) {
    String js_chunk = info[0].As<String>();
    writer->content.assign(js_chunk.Utf16Value());
  } else if (info[0].IsTypedArray()) {
    auto js_buffer = info[0].As<Uint8Array>();
    char* data = reinterpret_cast<char *>(js_buffer.Data());
    size_t length = js_buffer.ByteLength();
    if (!writer->leftover_bytes.empty()) {
      writer->leftover_bytes.insert(
        writer->leftover_bytes.end(),
        data,
        data + length
      );
      data = writer->leftover_bytes.data();
      length = writer->leftover_bytes.size();
    }
    size_t bytes_written = writer->conversion->decode(
      writer->content,
      data,
      length
    );
    if (bytes_written < length) {
      writer->leftover_bytes.assign(data + bytes_written, data + length);
    } else {
      writer->leftover_bytes.clear();
    }
  }
}

void TextWriter::end(const CallbackInfo &info) {
  auto writer = this;
  if (!writer->leftover_bytes.empty()) {
    writer->conversion->decode(
      writer->content,
      writer->leftover_bytes.data(),
      writer->leftover_bytes.size(),
      true
    );
  }
}

u16string TextWriter::get_text() {
  return move(content);
}
