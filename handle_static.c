#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include <errno.h>
#include "socket_io.h"

extern config *global_config;

struct _ext_to_content_type ext_to_content[] = {
    { { "html", "htm", NULL }, "text/html" },
    { { "js", NULL } , "application/x-javascript" },
    { { "css", NULL } , "text/css" },
    { { "jpeg", "jpg", "jpe", NULL }, "image/jpeg" },
    { { "png", NULL } , "image/png" },
    { { "gif", NULL } , "image/gif" },
    { { "swf", NULL } , "application/x-shockwave-flash"},
    { { "txt", "c", "h", "asm", "cpp", NULL }, "text/plain" },
    { { "pdf", NULL }, "application/pdf" }
};

char *get_content_type(const char *extension, char *content_type) {
    int i, j;
    for (i = 0; i < sizeof(ext_to_content) / sizeof(ext_to_content[0]); i++) {
        for (j = 0; ext_to_content[i].extnsn[j] != NULL; j++) {
            if (strcmp(extension, ext_to_content[i].extnsn[j]) == 0) {
                sprintf(content_type, "%s", ext_to_content[i].contentname);
                return content_type;
            }
        }
    }

    sprintf(content_type, "%s", "application/octet-stream");

    return content_type;
}

char *substr(const char *str, unsigned start, unsigned end, char *file_ext) {
    unsigned n = end - start;
    char stbuf[256];
    strncpy(stbuf, str + start, n);
    stbuf[n] = 0; //字串最后加上0
    strcpy(file_ext, stbuf);
    return file_ext;
}

char *get_extension(const char *fileName, char *file_ext) {
    char *ptr, c = '.';
    ptr = strrchr(fileName, c); //最后一个出现c的位置
    int pos = ptr - fileName; //用指针相减 求得索引
    substr(fileName, pos + 1, strlen(fileName), file_ext);

    return file_ext;
}

char *read_file(const char *filepath, int *filelength) {
    int file = open(filepath, O_RDONLY);
    char *filedate;
    struct stat info;
    if (fstat(file, &info) != -1) {
        *filelength = info.st_size;
        filedate = malloc(*filelength);
        read(file, filedate, *filelength);
    } else {
        log_warn("the file %s is NULL ", filepath);
        *filelength = 0;
        filedate = NULL;
    }
    close(file);

    return filedate;
}

int handle_static(http_parser *parser, const char *urlStr) {
    client_t *client = parser->data;

    if (client == NULL) {
        log_warn("the client is NULL !");
        return -1;
    }

    char file_path[200];
    sprintf(file_path, "%s%s", global_config->static_path, urlStr);
    char file_ext[256];
    get_extension(file_path, file_ext);
    char content_type[200];
    get_content_type(file_ext, content_type);

    int file = open(file_path, O_RDONLY);
    struct stat info;
    if (fstat(file, &info) == -1) {
        log_warn("the file %s is NULL ", file_path);
        write(client->fd, RESPONSE_404, strlen(RESPONSE_404));

        close(file);
        on_close(client);

        return 0;
    }

    int file_len = info.st_size;
    char head_msg[200] = "";
    sprintf(head_msg, RESPONSE_TEMPLATE, content_type, file_len);
    write(client->fd, head_msg, strlen(head_msg));

    int read_count;
    int BUFSIZE = 8 * 1024;//8096;
    char buffer[BUFSIZE + 1];
    while ((read_count = read(file, buffer, BUFSIZE)) > 0) {
        int bytes_left = read_count;
        char *ptr = buffer;
        int need_break = 0;
        while (bytes_left > 0) {
            ssize_t write_len = write(client->fd, ptr, bytes_left);

            if (write_len == -1) {
                log_warn("write failed(errno = %d): %s", errno, strerror(errno));
                switch (errno) {
                    case EAGAIN:
                    case EINTR:
                    case EINPROGRESS:
                        log_debug("now sleep 0.2s");
                        ev_sleep(0.2);
                        break;
                        // case EPIPE:
                        // case ECONNRESET:
                        //     return -2;
                    default:
                        need_break = 1;
                        break;
                    }
            } else if (write_len == 0) {
                need_break = 1;
                log_error("write_len is zero, and break now");
                break;
            } else if (write_len < bytes_left) {
                bytes_left -= write_len;
                ptr += write_len;
                log_warn("write client with something wrong wtih bytes_left = %d & write_len = %d and write the left data !", (int)bytes_left, (int)write_len);
            } else {
                break;
            }
        }

        if (need_break) {
            break;
        }
    }

    close(file);
    on_close(client);

    return 0;
}