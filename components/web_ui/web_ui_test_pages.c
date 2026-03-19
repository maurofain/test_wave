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
    return webpages_send_external_or_error(req, "test.html", "text/html; charset=utf-8");
}

esp_err_t led_bar_test_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "led_bar_test_section.html", "text/html; charset=utf-8");
}

esp_err_t digital_io_test_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "digital_io.html", "text/html; charset=utf-8");
}
