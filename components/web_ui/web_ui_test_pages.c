#include "web_ui_internal.h"
#include "esp_log.h"

#define TAG "WEB_UI_TEST_PAGE"

/**
 * @brief Renderizza la pagina `/test` per diagnostica e comandi hardware.
 *
 * La pagina include sezioni collassabili per periferiche e comandi manuali,
 * con JavaScript per invocare gli endpoint sotto il prefisso `/api/test/` e mostrare i risultati.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t test_page_handler(httpd_req_t *req)
{
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "test.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "test.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[C] GET /test");

    const char *extra_style = WEBPAGE_TEST_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Test Hardware", extra_style, true);

    const char *body = WEBPAGE_TEST_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
}

esp_err_t led_bar_test_page_handler(httpd_req_t *req)
{
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "led_bar_test_section.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "led_bar_test_section.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[C] GET /led_bar_test");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "LED Bar Test", "", false);

    // Per ora fallback alla pagina esterna se embedded non disponibile
    return webpages_send_external_or_error(req, "led_bar_test_section.html", "text/html; charset=utf-8");
#endif
}
