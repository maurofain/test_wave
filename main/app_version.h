#ifndef APP_VERSION_H
#define APP_VERSION_H

#define APP_VERSION "1.3.7"
#define APP_DATE    "2026-02-24"

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


// I flag DNA_* (DoNotActivate) sono definiti centralmente in CMakeLists.txt
// (add_compile_definitions) e sono visibili a tutti i componenti e a main/.
// NON ridefinirli qui.
#define DNA_NFC 0
