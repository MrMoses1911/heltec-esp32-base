#include "unity.h"
#include "ui/menu_manager.h"

void test_menu_navigation_wraps(void) {
    pulse::ui::MenuManager m;
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kMainMenu, m.CurrentScreen());
    TEST_ASSERT_EQUAL(0, m.CurrentSelection());

    m.HandleButtonClick(); // 0 -> 1
    TEST_ASSERT_EQUAL(1, m.CurrentSelection());
    m.HandleButtonClick(); // 1 -> 2
    TEST_ASSERT_EQUAL(2, m.CurrentSelection());
    m.HandleButtonClick(); // 2 -> 0
    TEST_ASSERT_EQUAL(0, m.CurrentSelection());
}

void test_menu_select_laser(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonLongPress();
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kDistanceView, m.CurrentScreen());
}

void test_menu_select_webserver_confirm(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick(); // 0 -> 1
    m.HandleButtonLongPress();
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kWebServerConfirm, m.CurrentScreen());
}

void test_menu_webserver_sim(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick(); // 0 -> 1
    m.HandleButtonLongPress(); // enter confirm
    m.HandleButtonLongPress(); // confirm "Sim"
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kMainMenu, m.CurrentScreen());
    TEST_ASSERT(m.IsWebServerRequested());
}

void test_menu_webserver_nao(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick(); // 0 -> 1
    m.HandleButtonLongPress(); // enter confirm
    m.HandleButtonClick(); // 0 -> 1 = "Nao"
    m.HandleButtonLongPress();
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kMainMenu, m.CurrentScreen());
    TEST_ASSERT_FALSE(m.IsWebServerRequested());
}

void test_menu_factory_reset(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick(); // 0 -> 1
    m.HandleButtonClick(); // 1 -> 2
    m.HandleButtonLongPress(); // enter confirm
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kFactoryResetConfirm, m.CurrentScreen());
    m.HandleButtonLongPress(); // confirm
    TEST_ASSERT(m.IsFactoryResetConfirmed());
}

void test_menu_distance_back(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonLongPress(); // enter distance
    m.HandleButtonLongPress(); // back
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kMainMenu, m.CurrentScreen());
}

void test_menu_confirm_wraps(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick(); // 0 -> 1
    m.HandleButtonLongPress(); // enter confirm
    m.HandleButtonClick(); // 0 -> 1
    TEST_ASSERT_EQUAL(1, m.CurrentSelection());
    m.HandleButtonClick(); // 1 -> 0
    TEST_ASSERT_EQUAL(0, m.CurrentSelection());
}

void test_menu_reset(void) {
    pulse::ui::MenuManager m;
    m.HandleButtonClick();
    m.HandleButtonLongPress(); // factory reset confirm
    m.Reset();
    TEST_ASSERT_EQUAL(pulse::ui::Screen::kMainMenu, m.CurrentScreen());
    TEST_ASSERT_FALSE(m.IsFactoryResetConfirmed());
}
