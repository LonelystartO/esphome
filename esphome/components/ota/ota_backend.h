#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "ota_component.h"

// Extended OTAState enum to include additional states needed by backends
// but not exposed in the main component interface
#ifndef OTA_ABORT
#define OTA_ABORT 3
#define OTA_ERROR 4
#endif

#ifdef USE_OTA_STATE_CALLBACK
#include "esphome/core/automation.h"
#endif

namespace esphome {
namespace ota {

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(size_t image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
  virtual bool supports_compression() = 0;
};

#ifdef USE_OTA_STATE_CALLBACK
class OTAGlobalCallback {
 public:
  void register_ota(OTAComponent *ota_caller) {
    ota_caller->add_on_state_callback([this, ota_caller](OTAState state, float progress, uint8_t error) {
      this->state_callback_.call(state, progress, error, ota_caller);
    });
  }
  void add_on_state_callback(std::function<void(OTAState, float, uint8_t, OTAComponent *)> &&callback) {
    this->state_callback_.add(std::move(callback));
  }

 protected:
  CallbackManager<void(OTAState, float, uint8_t, OTAComponent *)> state_callback_{};
};

OTAGlobalCallback *get_global_ota_callback();
void register_ota_platform(OTAComponent *ota_caller);
#endif
// This function is defined in ota_component.cpp
std::unique_ptr<OTABackend> make_ota_backend();

}  // namespace ota
}  // namespace esphome
