#include <stdbool.h>

#include "zlog.h"

bool init_log() {
    int rc = dzlog_init("log.conf", "default");

    if (rc) {
        fprintf(stderr, "log.conf init failed !!!!!!!!!!\n");
        return false;
    }

    return true;
}

void fini_log() {
    zlog_fini();
}