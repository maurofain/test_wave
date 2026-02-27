#pragma once

#include <stdbool.h>

/**
 * @file web_ui_profile.h
 * @brief Definizioni dei profili e delle feature Web UI
 */
typedef enum {
    WEB_UI_SCOPE_APP = 0,
    WEB_UI_SCOPE_FACTORY,
    WEB_UI_SCOPE_ALL,
} web_ui_scope_t;

typedef enum {
    WEB_UI_FEATURE_HOME_TEST = 0,
    WEB_UI_FEATURE_HOME_TASKS,
    WEB_UI_FEATURE_HOME_HTTP_SERVICES,
    WEB_UI_FEATURE_HOME_EMULATOR,
    WEB_UI_FEATURE_ENDPOINT_TEST,
    WEB_UI_FEATURE_ENDPOINT_TASKS,
    WEB_UI_FEATURE_ENDPOINT_HTTP_SERVICES,
    WEB_UI_FEATURE_ENDPOINT_EMULATOR,
    WEB_UI_FEATURE_ENDPOINT_PROGRAMS,
} web_ui_feature_t;

/**
 * @brief Calcola lo scope (app/factory) per una feature
 */
static inline web_ui_scope_t web_ui_feature_scope(web_ui_feature_t feature)
{
    switch (feature) {
    case WEB_UI_FEATURE_HOME_TEST:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_HOME_TASKS:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_HOME_HTTP_SERVICES:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_HOME_EMULATOR:
        return WEB_UI_SCOPE_ALL;
    case WEB_UI_FEATURE_ENDPOINT_TEST:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_ENDPOINT_TASKS:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_ENDPOINT_HTTP_SERVICES:
        return WEB_UI_SCOPE_FACTORY;
    case WEB_UI_FEATURE_ENDPOINT_EMULATOR:
        return WEB_UI_SCOPE_ALL;
    case WEB_UI_FEATURE_ENDPOINT_PROGRAMS:
        return WEB_UI_SCOPE_ALL;
    default:
        return WEB_UI_SCOPE_ALL;
    }
}

/**
 * @brief Verifica se uno scope è consentito nello stato corrente
 */
static inline bool web_ui_scope_allows(web_ui_scope_t scope, bool is_factory_runtime)
{
    if (scope == WEB_UI_SCOPE_ALL) {
        return true;
    }
    if (scope == WEB_UI_SCOPE_FACTORY) {
        return is_factory_runtime;
    }
    return !is_factory_runtime;
}
