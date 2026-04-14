#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEVICE_COMPONENT_STATUS_DISABLED = 0,
    DEVICE_COMPONENT_STATUS_ACTIVE,
    DEVICE_COMPONENT_STATUS_OFFLINE,
    DEVICE_COMPONENT_STATUS_ONLINE,
} device_component_status_t;

static inline const char *device_component_status_to_string(device_component_status_t status)
{
    switch (status) {
        case DEVICE_COMPONENT_STATUS_DISABLED:
            return "disattivato";
        case DEVICE_COMPONENT_STATUS_ACTIVE:
            return "attivato";
        case DEVICE_COMPONENT_STATUS_OFFLINE:
            return "offline";
        case DEVICE_COMPONENT_STATUS_ONLINE:
            return "online";
        default:
            return "offline";
    }
}

#ifdef __cplusplus
}
#endif
