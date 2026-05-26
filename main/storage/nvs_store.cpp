#include "storage/nvs_store.h"
#include "nvs_flash.h"

namespace pulse {
namespace storage {

esp_err_t NvsStore::Initialize() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    err = nvs_flash_erase();
    if (err != ESP_OK) return err;
    err = nvs_flash_init();
  }
  if (err == ESP_OK) initialized_ = true;
  return err;
}

esp_err_t NvsStore::ReadU16(const char* nameSpace, const char* key, uint16_t* value) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READONLY, &handle);
  if (err != ESP_OK) return err;
  err = nvs_get_u16(handle, key, value);
  nvs_close(handle);
  return err;
}

esp_err_t NvsStore::ReadI16(const char* nameSpace, const char* key, int16_t* value) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READONLY, &handle);
  if (err != ESP_OK) return err;
  err = nvs_get_i16(handle, key, value);
  nvs_close(handle);
  return err;
}

esp_err_t NvsStore::WriteU16(const char* nameSpace, const char* key, uint16_t value) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_u16(handle, key, value);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t NvsStore::WriteI16(const char* nameSpace, const char* key, int16_t value) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_i16(handle, key, value);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t NvsStore::ReadBlob(const char* nameSpace, const char* key, void* value, size_t valueSize) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READONLY, &handle);
  if (err != ESP_OK) return err;
  size_t actual = valueSize;
  err = nvs_get_blob(handle, key, value, &actual);
  nvs_close(handle);
  if (err == ESP_OK && actual != valueSize) return ESP_ERR_INVALID_SIZE;
  return err;
}

esp_err_t NvsStore::WriteBlob(const char* nameSpace, const char* key, const void* value, size_t valueSize) const {
  nvs_handle_t handle = 0;
  esp_err_t err = Open(nameSpace, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_blob(handle, key, value, valueSize);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t NvsStore::EraseAll() {
  return nvs_flash_erase();
}

esp_err_t NvsStore::Open(const char* nameSpace, nvs_open_mode_t mode, nvs_handle_t* handle) const {
  return nvs_open(nameSpace, mode, handle);
}

}  // namespace storage
}  // namespace pulse
