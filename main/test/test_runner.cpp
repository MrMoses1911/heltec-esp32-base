#include "unity.h"

extern void test_config_save_load_roundtrip(void);
extern void test_config_defaults_on_empty(void);
extern void test_config_ssid_code_save_load(void);
extern void test_config_ssid_code_defaults(void);
extern void test_menu_navigation_wraps(void);
extern void test_menu_select_laser(void);
extern void test_menu_select_webserver_confirm(void);
extern void test_menu_webserver_sim(void);
extern void test_menu_webserver_nao(void);
extern void test_menu_factory_reset(void);
extern void test_menu_distance_back(void);
extern void test_menu_confirm_wraps(void);
extern void test_menu_reset(void);
extern void test_sensor_constants(void);
extern void test_sensor_formula(void);
extern void test_sensor_period_limits(void);
extern void test_network_ssid_code(void);
extern void test_network_ssid_code_zero(void);

void run_all_tests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_config_save_load_roundtrip);
    RUN_TEST(test_config_defaults_on_empty);
    RUN_TEST(test_config_ssid_code_save_load);
    RUN_TEST(test_config_ssid_code_defaults);

    RUN_TEST(test_menu_navigation_wraps);
    RUN_TEST(test_menu_select_laser);
    RUN_TEST(test_menu_select_webserver_confirm);
    RUN_TEST(test_menu_webserver_sim);
    RUN_TEST(test_menu_webserver_nao);
    RUN_TEST(test_menu_factory_reset);
    RUN_TEST(test_menu_distance_back);
    RUN_TEST(test_menu_confirm_wraps);
    RUN_TEST(test_menu_reset);

    RUN_TEST(test_sensor_constants);
    RUN_TEST(test_sensor_formula);
    RUN_TEST(test_sensor_period_limits);

    RUN_TEST(test_network_ssid_code);
    RUN_TEST(test_network_ssid_code_zero);

    UNITY_END();
}
