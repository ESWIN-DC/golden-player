#ifndef __GP_LOG_H__
#define __GP_LOG_H__

#include <spdlog/spdlog.h>

#define INFO(fmt, ...)                                                         \
    if (ctx->enable_verbose)                                                   \
        SPDLOG_INFO("INFO: {}(): (line:{}) " fmt "\n", __FUNCTION__, __LINE__, \
                    ##__VA_ARGS__);

#define WARN(fmt, ...)                                                     \
    SPDLOG_WARN("WARN: {}(): (line:{}) " fmt "\n", __FUNCTION__, __LINE__, \
                ##__VA_ARGS__);

#define CHECK_ERROR_ABORT(expr)                  \
    do {                                         \
        if ((expr) < 0) {                        \
            abort();                             \
            GP_ORIGINATE_ERROR(#expr " failed"); \
        }                                        \
    } while (0);

#define CHECK_ERROR(cond, label, fmt) \
    if (!cond) {                      \
        error = 1;                    \
        SPDLOG_ERROR(fmt "\n");       \
        goto label;                   \
    }

#define ERROR_RETURN(fmt, ...)            \
    do {                                  \
        SPDLOG_ERROR(fmt, ##__VA_ARGS__); \
        return false;                     \
    } while (0)

#define GP_ERROR(_file, _func, _line, _str)                               \
    do {                                                                  \
        SPDLOG_ERROR("Error generated. {}, {}:{} ", _file, _func, _line); \
        SPDLOG_ERROR(_str);                                               \
    } while (0)

/**
 * Report and return an error that was first detected in the current method.
 */
#define GP_ORIGINATE_ERROR(_str) \
    do {                         \
        SPDLOG_ERROR(_str);      \
        return false;            \
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

#endif  // __GP_LOG_H__
