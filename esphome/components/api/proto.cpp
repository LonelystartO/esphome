#include "proto.h"
#include <cinttypes>
#include <chrono>  // for high_resolution_clock
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.proto";

void ProtoService::log_protobuf_encode_timing(const ProtoMessage &msg) {
  // Benchmark without reserve
  auto start_without_reserve = std::chrono::high_resolution_clock::now();
  std::vector<uint8_t> buffer_without_reserve;
  ProtoWriteBuffer write_buffer_without_reserve(&buffer_without_reserve);
  msg.encode(write_buffer_without_reserve);
  auto end_without_reserve = std::chrono::high_resolution_clock::now();
  auto time_without_reserve =
      std::chrono::duration_cast<std::chrono::microseconds>(end_without_reserve - start_without_reserve).count();

  // Benchmark with reserve (including size calculation)
  auto start_with_reserve = std::chrono::high_resolution_clock::now();
  uint32_t msg_size = 0;
  msg.calculate_size(msg_size);
  std::vector<uint8_t> buffer_with_reserve;
  buffer_with_reserve.reserve(msg_size);
  ProtoWriteBuffer write_buffer_with_reserve(&buffer_with_reserve);
  msg.encode(write_buffer_with_reserve);
  auto end_with_reserve = std::chrono::high_resolution_clock::now();
  auto time_with_reserve =
      std::chrono::duration_cast<std::chrono::microseconds>(end_with_reserve - start_with_reserve).count();

  // Calculate improvement percentage
  uint32_t improvement_percent = 0;
  if (time_without_reserve > 0) {
    improvement_percent = ((time_without_reserve - time_with_reserve) * 100) / time_without_reserve;
  }

  // Log results
  ESP_LOGW(TAG,
           "Protobuf encoding benchmark: without reserve: %" PRIu64 "us, with reserve (including size calc): %" PRIu64
           "us, improvement: "
           "%" PRIu32 "%%, size: %" PRIu32 " bytes",
           (uint64_t) time_without_reserve, (uint64_t) time_with_reserve, improvement_percent, msg_size);
}

void ProtoMessage::decode(const uint8_t *buffer, size_t length) {
  uint32_t i = 0;
  bool error = false;
  while (i < length) {
    uint32_t consumed;
    auto res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
    if (!res.has_value()) {
      ESP_LOGV(TAG, "Invalid field start at %" PRIu32, i);
      break;
    }

    uint32_t field_type = (res->as_uint32()) & 0b111;
    uint32_t field_id = (res->as_uint32()) >> 3;
    i += consumed;

    switch (field_type) {
      case 0: {  // VarInt
        res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid VarInt at %" PRIu32, i);
          error = true;
          break;
        }
        if (!this->decode_varint(field_id, *res)) {
          ESP_LOGV(TAG, "Cannot decode VarInt field %" PRIu32 " with value %" PRIu32 "!", field_id, res->as_uint32());
        }
        i += consumed;
        break;
      }
      case 2: {  // Length-delimited
        res = ProtoVarInt::parse(&buffer[i], length - i, &consumed);
        if (!res.has_value()) {
          ESP_LOGV(TAG, "Invalid Length Delimited at %" PRIu32, i);
          error = true;
          break;
        }
        uint32_t field_length = res->as_uint32();
        i += consumed;
        if (field_length > length - i) {
          ESP_LOGV(TAG, "Out-of-bounds Length Delimited at %" PRIu32, i);
          error = true;
          break;
        }
        if (!this->decode_length(field_id, ProtoLengthDelimited(&buffer[i], field_length))) {
          ESP_LOGV(TAG, "Cannot decode Length Delimited field %" PRIu32 "!", field_id);
        }
        i += field_length;
        break;
      }
      case 5: {  // 32-bit
        if (length - i < 4) {
          ESP_LOGV(TAG, "Out-of-bounds Fixed32-bit at %" PRIu32, i);
          error = true;
          break;
        }
        uint32_t val = encode_uint32(buffer[i + 3], buffer[i + 2], buffer[i + 1], buffer[i]);
        if (!this->decode_32bit(field_id, Proto32Bit(val))) {
          ESP_LOGV(TAG, "Cannot decode 32-bit field %" PRIu32 " with value %" PRIu32 "!", field_id, val);
        }
        i += 4;
        break;
      }
      default:
        ESP_LOGV(TAG, "Invalid field type at %" PRIu32, i);
        error = true;
        break;
    }
    if (error) {
      break;
    }
  }
}

#ifdef HAS_PROTO_MESSAGE_DUMP
std::string ProtoMessage::dump() const {
  std::string out;
  this->dump_to(out);
  return out;
}
#endif

}  // namespace api
}  // namespace esphome
