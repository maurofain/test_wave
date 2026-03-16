#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "fsm.h"

#define FSM_EVENT_TEXT_MAX_LEN (64U)

typedef enum {
    FSM_STATE_IDLE = 0,
    FSM_STATE_CREDIT,
    FSM_STATE_RUNNING,
    FSM_STATE_PAUSED,
    FSM_STATE_LVGL_PAGES_TEST,
} fsm_state_t;

typedef enum {
    FSM_EVENT_NONE = 0,
    FSM_EVENT_USER_ACTIVITY,
    FSM_EVENT_PAYMENT_ACCEPTED,
    FSM_EVENT_PROGRAM_SELECTED,
    FSM_EVENT_PROGRAM_STOP,
    FSM_EVENT_PROGRAM_PAUSE,
    FSM_EVENT_PROGRAM_RESUME,
    FSM_EVENT_TIMEOUT,
    FSM_EVENT_CREDIT_ENDED,
    FSM_EVENT_ENTER_LVGL_TEST,
    FSM_EVENT_EXIT_LVGL_TEST,
} fsm_event_t;

/* --------------------------------------------------------------------------
 * Agent / action identifiers for the new event bus
 *
 * Each task, driver or module that generates or consumes messages on the
 * application event bus must be assigned a unique ``agn_id`` value.  These
 * constants are used in the ``from`` and ``to`` fields of the extended
 * ``fsm_input_event_t`` (see below).  Additional agents should be added here
 * as the migration proceeds.
 */
typedef enum {
    AGN_ID_NONE = 0,
    AGN_ID_FSM,               /* finite state machine core */
    AGN_ID_WEB_UI,            /* web interface / emulator */
    AGN_ID_TOUCH,             /* touch driver */
    AGN_ID_TOKEN,             /* token reader */

    /* hardware/peripheral agents */
    AGN_ID_AUX_GPIO,          /* aux gpio (GPIO33 etc) */
    AGN_ID_IO_EXPANDER,       /* I/O expander component */
    AGN_ID_PWM1,              /* PWM output driver */
    AGN_ID_PWM2,              /* PWM output driver */
    AGN_ID_LED,               /* LED controller */
    AGN_ID_SHT40,             /* temperature/humidity sensor */
    AGN_ID_CCTALK,            /* CCtalk bus */
    AGN_ID_RS232,             /* RS-232 UART */
    AGN_ID_RS485,             /* RS-485 UART */
    AGN_ID_MDB,               /* MDB bus */
    AGN_ID_USB_CDC_SCANNER,   /* USB CDC barcode scanner */
    AGN_ID_USB_HOST,          /* USB host CDC-ACM */
    AGN_ID_SD_CARD,           /* SD card interface */
    AGN_ID_EEPROM,            /* external EEPROM */
    AGN_ID_REMOTE_LOGGING,    /* network log sender */
    AGN_ID_HTTP_SERVICES,     /* internal HTTP services */
    AGN_ID_DEVICE_CONFIG,     /* configuration manager */
    AGN_ID_DEVICE_ACTIVITY,   /* activity logging service */
    AGN_ID_ERROR_LOG,         /* error/crash logger */
    AGN_ID_LVGL,              /* LVGL graphics engine */
    AGN_ID_WAVESHARE_LCD,     /* display driver */
    /* add more agents as components are introduced */
} agn_id_t;

/* Actions describe the intent of a message.  They are orthogonal to the old
 * ``fsm_input_event_type_t`` enums and will gradually replace them.  Define
 * new action codes here as you refactor components.
 */
typedef enum {
    ACTION_ID_NONE = 0,
    ACTION_ID_USER_ACTIVITY,
    ACTION_ID_PAYMENT_ACCEPTED,
    ACTION_ID_PROGRAM_SELECTED,
    ACTION_ID_PROGRAM_STOP,
    ACTION_ID_PROGRAM_PAUSE_TOGGLE,
    ACTION_ID_CREDIT_ENDED,
    ACTION_ID_BUTTON_PRESSED,    /* new event requested */
    ACTION_ID_SYSTEM_IDLE,       /* new event requested */
    ACTION_ID_SYSTEM_RUN,        /* added per request */
    ACTION_ID_SYSTEM_ERROR,      /* added per request */
    ACTION_ID_LVGL_TEST_ENTER,
    ACTION_ID_LVGL_TEST_EXIT,

    /* GPIO peripheral actions */
    ACTION_ID_GPIO_READ_PORT,     /* read a single GPIO port */
    ACTION_ID_GPIO_READ_ALL,      /* read all ports */
    ACTION_ID_GPIO_WRITE_PORT,    /* write a single port */
    ACTION_ID_GPIO_WRITE_ALL,     /* write all ports */
    ACTION_ID_GPIO_RESET_ALL,     /* reset/clear all ports */

    /* serial/bus peripherals */
    ACTION_ID_CCTALK_RX_DATA,     /* data received on CCtalk */
    ACTION_ID_CCTALK_TX_DATA,     /* data transmitted on CCtalk */
    ACTION_ID_CCTALK_CONFIG,      /* configuration change */
    ACTION_ID_CCTALK_RESET,       /* reset command */
    ACTION_ID_CCTALK_START,       /* start acceptor sequence */
    ACTION_ID_CCTALK_STOP,        /* stop acceptor sequence */
    ACTION_ID_CCTALK_MASK,        /* set per-channel mask via value_u32 (low=LSB, high=next byte) */

    ACTION_ID_RS232_RX_DATA,
    ACTION_ID_RS232_TX_DATA,
    ACTION_ID_RS232_CONFIG,
    ACTION_ID_RS232_RESET,

    ACTION_ID_RS485_RX_DATA,
    ACTION_ID_RS485_TX_DATA,
    ACTION_ID_RS485_CONFIG,
    ACTION_ID_RS485_RESET,

    ACTION_ID_MDB_RX_DATA,
    ACTION_ID_MDB_TX_DATA,
    ACTION_ID_MDB_CONFIG,
    ACTION_ID_MDB_RESET,

    /* IO expander events */
    ACTION_ID_IOEXP_READ_PORT,
    ACTION_ID_IOEXP_WRITE_PORT,
    ACTION_ID_IOEXP_CONFIG,
    ACTION_ID_IOEXP_RESET,

    /* PWM output events */
    ACTION_ID_PWM_SET_DUTY,
    ACTION_ID_PWM_START,
    ACTION_ID_PWM_STOP,
    ACTION_ID_PWM_CONFIG,

    /* LED control events */
    ACTION_ID_LED_SET_RGBCOLOR,
    ACTION_ID_LED_ALL_OFF,
    ACTION_ID_LED_CONFIG,

    /* Sensor SHT40 */
    ACTION_ID_SHT40_MEASURE_READY,
    ACTION_ID_SHT40_ERROR,

    /* USB CDC scanner */
    ACTION_ID_USB_CDC_SCANNER_RX,
    ACTION_ID_USB_CDC_SCANNER_TX,
    ACTION_ID_USB_CDC_SCANNER_CONNECT,
    ACTION_ID_USB_CDC_SCANNER_DISCONNECT,
    ACTION_ID_USB_CDC_SCANNER_READ,   /* request barcode read */
    ACTION_ID_USB_CDC_SCANNER_ON,     /* power on scanner */
    ACTION_ID_USB_CDC_SCANNER_OFF,    /* power off scanner */

    /* SD card operations */
    ACTION_ID_SD_CARD_INSERT,
    ACTION_ID_SD_CARD_REMOVE,
    ACTION_ID_SD_CARD_READ,
    ACTION_ID_SD_CARD_WRITE,
    ACTION_ID_SD_CARD_DELETE,
    ACTION_ID_SD_CARD_ERROR,

    /* ... more actions ... */
} action_id_t;

/* legacy input event type used by the FSM; kept for compatibility during
 * migration.  New code may ignore it or translate an ``action_id`` into it.
 */
typedef enum {
    FSM_INPUT_EVENT_NONE = 0,
    FSM_INPUT_EVENT_USER_ACTIVITY,
    FSM_INPUT_EVENT_TOUCH,
    FSM_INPUT_EVENT_KEY,
    FSM_INPUT_EVENT_COIN,
    FSM_INPUT_EVENT_TOKEN,
    FSM_INPUT_EVENT_CARD_CREDIT,
    FSM_INPUT_EVENT_QR_CREDIT,
    FSM_INPUT_EVENT_QR_SCANNED,
    FSM_INPUT_EVENT_PROGRAM_SELECTED,
    FSM_INPUT_EVENT_PROGRAM_STOP,
    FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
    FSM_INPUT_EVENT_CREDIT_ENDED,
    FSM_INPUT_EVENT_PROGRAM_SWITCH,  /* cambio programma a macchina running: scala il tempo residuo */
} fsm_input_event_type_t;

/* extended message structure that will circulate on the shared mailbox.
 * ``from`` and ``to`` support multi‑recipient dispatch; ``action`` encodes the
 * intended operation.  The old fields remain at the end for backward
 * compatibility with existing consumers.
 */
typedef struct {
    agn_id_t from;            /* sender agent identifier */
    agn_id_t to[10];          /* recipient list, zero‑filled when unused */
    action_id_t action;       /* action requested by the message */

    /* legacy fields follow; they were the entire message in the old model */
    fsm_input_event_type_t type;
    uint32_t timestamp_ms;
    int32_t value_i32;
    uint32_t value_u32;
    uint32_t aux_u32;
    void * data_ptr;
    char text[FSM_EVENT_TEXT_MAX_LEN];
} fsm_input_event_t;

typedef struct {
    fsm_state_t state;
    int32_t credit_cents;
    int32_t ecd_coins;          /* credito effettivo (monete+QR), definitivo */
    int32_t vcd_coins;          /* credito virtuale (tessera), scalato alla selezione */
    int32_t ecd_used;           /* quota ecd consumata nei cicli avviati */
    int32_t vcd_used;           /* quota vcd consumata nei cicli avviati */
    bool program_running;
    uint32_t running_elapsed_ms;
    uint32_t pause_elapsed_ms;
    uint32_t pause_max_ms;
    bool pause_limit_reached;
    uint32_t running_target_ms;
    int32_t running_price_units;
    char running_program_name[FSM_EVENT_TEXT_MAX_LEN];
    uint32_t inactivity_ms;
    uint32_t splash_screen_time_ms;
} fsm_ctx_t;

void fsm_init(fsm_ctx_t *ctx);
bool fsm_handle_event(fsm_ctx_t *ctx, fsm_event_t event);
bool fsm_handle_input_event(fsm_ctx_t *ctx, const fsm_input_event_t *event);
bool fsm_tick(fsm_ctx_t *ctx, uint32_t elapsed_ms);
const char *fsm_state_to_string(fsm_state_t state);
bool fsm_enter_lvgl_pages_test(void);
bool fsm_exit_lvgl_pages_test(void);

bool fsm_event_queue_init(size_t queue_len);
bool fsm_event_publish(const fsm_input_event_t *event, TickType_t timeout_ticks);
bool fsm_event_publish_from_isr(const fsm_input_event_t *event, BaseType_t *task_woken);
bool fsm_event_receive(fsm_input_event_t *event, agn_id_t receiver_id, TickType_t timeout_ticks);
bool fsm_publish_simple_event(fsm_input_event_type_t type, int32_t value_i32, const char *text, TickType_t timeout_ticks);
size_t fsm_pending_messages_copy(char out[][FSM_EVENT_TEXT_MAX_LEN], size_t max_count);
void fsm_append_message(const char *message);
void fsm_runtime_publish(const fsm_ctx_t *ctx);
bool fsm_runtime_snapshot(fsm_ctx_t *out_ctx);
