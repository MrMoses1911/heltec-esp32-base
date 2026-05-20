#include "unity.h"
#include "storage/nvs_store.h"
#include "storage/config_repository.h"
#include <cstring>

void test_config_save_load_roundtrip(void) {
    pulse::storage::NvsStore store;
    TEST_ASSERT_EQUAL(ESP_OK, store.Initialize());

    pulse::storage::ConfigRepository repo(&store);
    pulse::storage::SafeDockConfig cfg;
    cfg.freqMinHz = 111;
    cfg.freqMaxHz = 222;
    cfg.distanceMinMm = 333;
    cfg.distanceMaxMm = 444;
    cfg.sensorGpio = 38;
    cfg.webServerAlwaysOn = 1;
    cfg.webServerTimeoutMinutes = 30;
    std::strncpy(cfg.webUsername, "test_user", sizeof(cfg.webUsername) - 1);
    std::strncpy(cfg.webPassword, "test_pass", sizeof(cfg.webPassword) - 1);

    TEST_ASSERT_EQUAL(ESP_OK, repo.Save(cfg));

    pulse::storage::SafeDockConfig loaded;
    TEST_ASSERT_EQUAL(ESP_OK, repo.Load(&loaded));
    TEST_ASSERT_EQUAL_UINT16(111, loaded.freqMinHz);
    TEST_ASSERT_EQUAL_UINT16(222, loaded.freqMaxHz);
    TEST_ASSERT_EQUAL_UINT16(333, loaded.distanceMinMm);
    TEST_ASSERT_EQUAL_UINT16(444, loaded.distanceMaxMm);
    TEST_ASSERT_EQUAL_UINT16(38, loaded.sensorGpio);
    TEST_ASSERT_EQUAL_UINT16(1, loaded.webServerAlwaysOn);
    TEST_ASSERT_EQUAL_UINT16(30, loaded.webServerTimeoutMinutes);
    TEST_ASSERT_EQUAL_STRING("test_user", loaded.webUsername);
    TEST_ASSERT_EQUAL_STRING("test_pass", loaded.webPassword);
}

void test_config_defaults_on_empty(void) {
    pulse::storage::NvsStore store;
    TEST_ASSERT_EQUAL(ESP_OK, store.Initialize());
    pulse::storage::ConfigRepository repo(&store);
    repo.FactoryReset();

    pulse::storage::SafeDockConfig cfg;
    cfg.freqMinHz = 999;
    TEST_ASSERT_EQUAL(ESP_OK, repo.Load(&cfg));
    TEST_ASSERT_EQUAL_UINT16(100, cfg.freqMinHz);
    TEST_ASSERT_EQUAL_UINT16(600, cfg.freqMaxHz);
    TEST_ASSERT_EQUAL_UINT16(50, cfg.distanceMinMm);
    TEST_ASSERT_EQUAL_UINT16(5000, cfg.distanceMaxMm);
}

void test_config_ssid_code_save_load(void) {
    pulse::storage::NvsStore store;
    TEST_ASSERT_EQUAL(ESP_OK, store.Initialize());
    pulse::storage::ConfigRepository repo(&store);
    TEST_ASSERT_EQUAL(ESP_OK, repo.SaveSsidCode(0xABCD));
    uint16_t code = 0;
    TEST_ASSERT_EQUAL(ESP_OK, repo.LoadSsidCode(&code));
    TEST_ASSERT_EQUAL_UINT16(0xABCD, code);
}

void test_config_ssid_code_defaults(void) {
    pulse::storage::NvsStore store;
    TEST_ASSERT_EQUAL(ESP_OK, store.Initialize());
    pulse::storage::ConfigRepository repo(&store);
    repo.FactoryReset();
    uint16_t code = 0xFFFF;
    TEST_ASSERT_EQUAL(ESP_OK, repo.LoadSsidCode(&code));
    TEST_ASSERT_EQUAL_UINT16(0, code);
}
