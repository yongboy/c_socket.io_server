#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include "zlog.h"

#define log_debug(format, args...) \
    dzlog_debug(format, ##args)
#define log_info(format, args...) \
    dzlog_info(format, ##args)
#define log_notice(format, args...) \
    dzlog_notice(format, ##args)
#define log_warn(format, args...) \
    dzlog_warn(format, ##args)
#define log_error(format, args...) \
    dzlog_error(format, ##args)
#define log_fatal(format, args...) \
    dzlog_fatal(format, ##args)

bool init_log();    
void fini_log();

#endif