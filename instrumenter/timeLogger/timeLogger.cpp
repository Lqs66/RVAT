#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define MAX_FILENAME_LEN 128
#define MAX_BUFFER_SIZE 1073741824 // 1 GiB
#define BUFFER_CAPACITY 1024

extern "C" {

    extern __thread FILE* timeLogFilePtr;
    extern char fileNamePattern[];
    // extern __thread int32_t timeCounter;

    __thread uint64_t buffer[BUFFER_CAPACITY];
    __thread int32_t buffer_idx;

    void time_logger(uint64_t bbNum_subNum, uint64_t exeTime) {
        char timeLogFileName[MAX_FILENAME_LEN];

        if (buffer_idx >= BUFFER_CAPACITY) {
            if (timeLogFilePtr == NULL) {
                pid_t pid = getpid();
                snprintf(timeLogFileName, MAX_FILENAME_LEN, fileNamePattern, pid);
                
                timeLogFilePtr = fopen(timeLogFileName, "ab");
                
                setvbuf(timeLogFilePtr, NULL, _IOFBF, MAX_BUFFER_SIZE);
            }
            
            int32_t size_to_write = buffer_idx * 8;
            fwrite(buffer, size_to_write, 1, timeLogFilePtr);
            // printf("timeCounter: %d \n", timeCounter);
            buffer_idx = 0;
        }
        
        buffer[buffer_idx++] = bbNum_subNum;
        buffer[buffer_idx++] = exeTime;
    }
}