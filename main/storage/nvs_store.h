#ifndef PULSE_SAFEDOCK_STORAGE_NVS_STORE_H
#define PULSE_SAFEDOCK_STORAGE_NVS_STORE_H

#include <cstddef>
#include "esp_err.h"
#include "nvs.h"

namespace pulse {
namespace storage {

class NvsStore {
 public:
  esp_err_t Initialize();
  esp_err_t ReadU16(const char* nameSpace, const char* key, uint16_t* value) const;
  esp_err_t WriteU16(const char* nameSpace, const char* key, uint16_t value) const;
  esp_err_t ReadBlob(const char* nameSpace, const char* key, void* value, size_t valueSize) const;
  esp_err_t WriteBlob(const char* nameSpace, const char* key, const void* value, size_t valueSize) const;
  esp_err_t EraseAll();

 private:
  esp_err_t Open(const char* nameSpace, nvs_open_mode_t mode, nvs_handle_t* handle) const;
  bool initialized_ = false;
};

}  // namespace storage
}  // namespace pulse

#endif
