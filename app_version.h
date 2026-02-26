#ifndef APP_VERSION_H
#define APP_VERSION_H

#define APP_VERSION "1.3.7"
#define APP_DATE    "2026-02-25"

/* 1 = build APP, 0 = build FACTORY */
#define COMPILE_APP 0

#if COMPILE_APP
#define COMPILE_MODE_LABEL "APP"
#define COMPILE_LOG_PREFIX "M"
#else
#define COMPILE_MODE_LABEL "FACTORY"
#define COMPILE_LOG_PREFIX "F"
#endif

#endif // APP_VERSION_H
