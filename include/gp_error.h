#ifndef __GP_ERROR_H__
#define __GP_ERROR_H__

#include <spdlog/spdlog.h>

#define GP_ERROR(_file, _func, _line, _str, ...)                           \
    do {                                                                   \
        spdlog::error("Error generated. {}, {}:{} ", _file, _func, _line); \
        spdlog::error(_str, ##__VA_ARGS__);                                \
        spdlog::error("\n");                                               \
    } while (0)

/**
 * Simply report an error.
 */
#define GP_REPORT_ERROR(_str, ...)                                         \
    do {                                                                   \
        GP_ERROR(__FILE__, __FUNCTION__, __LINE__, (_str), ##__VA_ARGS__); \
    } while (0)

/**
 * Report and return an error that was first detected in the current method.
 */
#define GP_ORIGINATE_ERROR(_str, ...)                                      \
    do {                                                                   \
        GP_ERROR(__FILE__, __FUNCTION__, __LINE__, (_str), ##__VA_ARGS__); \
        return false;                                                      \
    } while (0)

/**
 * Report an error that was first detected in the current method, then jumps to
 * the "fail:" label.
 */
#define GP_ORIGINATE_ERROR_FAIL(_str, ...)                                  \
    do {                                                                    \
        LOG_ERROR(__FILE__, __FUNCTION__, __LINE__, (_str), ##__VA_ARGS__); \
        goto fail;                                                          \
    } while (0)

/**
 * Report and return an error that was first detected in some method
 * called by the current method.
 */
#define GP_PROPAGATE_ERROR(_err)                                         \
    do {                                                                 \
        bool peResult = (_err);                                          \
        if (peResult != true) {                                          \
            GP_ERROR(__FILE__, __FUNCTION__, __LINE__, "(propagating)"); \
            return false;                                                \
        }                                                                \
    } while (0)

/**
 * Calls another function, and if an error was returned it is reported before
 * jumping to the "fail:" label.
 */
#define GP_PROPAGATE_ERROR_FAIL(_err, ...)                                \
    do {                                                                  \
        bool peResult = (_err);                                           \
        if (peResult != true) {                                           \
            GP__ERROR(__FILE__, __FUNCTION__, __LINE__, "(propagating)"); \
            goto fail;                                                    \
        }                                                                 \
    } while (0)

/**
 * Calls another function, and if an error was returned it is reported. The
 * caller does not return.
 */
#define GP_PROPAGATE_ERROR_CONTINUE(_err)                                \
    do {                                                                 \
        bool peResult = (_err);                                          \
        if (peResult != true) {                                          \
            GP_ERROR(__FILE__, __FUNCTION__, __LINE__, "(propagating)"); \
        }                                                                \
    } while (0)

#endif  // __GP_ERROR_H__
