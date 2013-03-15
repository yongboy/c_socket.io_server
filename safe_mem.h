#ifndef SAFE_MEM_H
#define SAFE_MEM_H

#include <stdlib.h>
#include <stdio.h>

static void *debug_malloc(size_t size, const char *file, int line, const char *func) {
    void *p;
    p = g_malloc(size);
    // printf("%s:%d:%s:malloc(%ld): p=0x%lx\n",
    //        file, line, func, size, (unsigned long)p);
    if(p == NULL){
    	fprintf(stderr, "malloc apply size %ld failure!\n", size);
    }
    
    return p;
}

#define malloc(s) debug_malloc(s, __FILE__, __LINE__, __func__)
#define free(p)  do {                                                \
        /*printf("%s:%d:%s:free(0x%lx)\n", __FILE__, __LINE__,          \
               __func__, (unsigned long)p);*/                            \
        if(p){                                                          \
            g_free(p);                                                     \
            p = NULL;                                                     \
        }                                                                  \
    } while (0)

#endif