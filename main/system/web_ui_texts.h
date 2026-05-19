#pragma once

#include "system/web_ui_texts/common.h"
#include "system/web_ui_texts/params.h"
#include "system/web_ui_texts/view.h"

#define PULSE_WEB_UI_TEXTLIST_SCRIPT \
R"html(
<script>
window.PulseWebUi = {
  texts: {
)html" \
PULSE_WEB_UI_TEXTS_COMMON_GROUP \
PULSE_WEB_UI_TEXTS_VIEW_GROUP \
PULSE_WEB_UI_TEXTS_PARAMS_GROUP \
R"html(
  },
  get(path, fallback) {
    const value = path.split('.').reduce((current, key) => current && current[key], this.texts)
    return value === undefined ? fallback : value
  },
  format(path, replacements, fallback) {
    const template = this.get(path, fallback)
    return String(template).replace(/\{(\w+)\}/g, (_, key) => replacements && replacements[key] !== undefined ? replacements[key] : '')
  },
  apply() {
    document.querySelectorAll('[data-text]').forEach((element) => {
      element.textContent = this.get(element.dataset.text, element.textContent)
    })
    document.querySelectorAll('[data-placeholder]').forEach((element) => {
      const value = this.get(element.dataset.placeholder, element.getAttribute('placeholder') || '')
      element.setAttribute('placeholder', value)
    })
  }
}
</script>
)html"
