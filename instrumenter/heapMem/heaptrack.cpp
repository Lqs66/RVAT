#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <typeinfo>

#define MAX_FILENAME_LEN 128

/******************************************************************************************/
/***                         Some helper functions for heaptrack                        ***/
/******************************************************************************************/
extern "C" {
typedef struct MemEntry {
    void* ptr;
    size_t size;
    size_t IRTypeHash;
    struct MemEntry* next;
} MemEntry;

static MemEntry* list_head = NULL;
static pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;

static MemEntry* alloc_entry() {
    MemEntry* entry = (MemEntry*)malloc(sizeof(MemEntry));
    if (entry) {
        entry->ptr = NULL;
        entry->size = 0;
        entry->IRTypeHash = 0;
        entry->next = NULL;
    }
    return entry;
}

static void free_entry(MemEntry* entry) {
    free(entry);
}

static void list_add(void* ptr, size_t size, size_t IRTypeHash) {
    pthread_mutex_lock(&mem_mutex);
    MemEntry* entry = alloc_entry();
    if (entry) {
        entry->ptr = ptr;
        entry->size = size;
        entry->IRTypeHash = IRTypeHash;
        entry->next = list_head;
        list_head = entry;
    }
    pthread_mutex_unlock(&mem_mutex);
}

static void list_remove(void* ptr) {
    pthread_mutex_lock(&mem_mutex);
    MemEntry** prev = &list_head;
    MemEntry* current = list_head;
    while (current) {
        if (current->ptr == ptr) {
            *prev = current->next;
            free_entry(current);
            break;
        }
        prev = &current->next;
        current = current->next;
    }
    pthread_mutex_unlock(&mem_mutex);
}

/******************************************************************************************/
/***                           Some hooks for heaptrack                                 ***/
/******************************************************************************************/

void* malloc_hook(size_t size, size_t typeHash) {
    void* ptr = malloc(size);
    if (typeHash == 0)
        printf("Warning: typeHash is %zu in malloc_hook, size=%zu\n", typeHash, size);
    if (ptr) list_add(ptr, size, typeHash);
    return ptr;
}

void* calloc_hook(size_t num, size_t size, size_t typeHash) {
    void* ptr = calloc(num, size);
    if (typeHash == 0)
        printf("Warning: typeHash is %zu in calloc_hook, num=%zu, size=%zu\n", typeHash, num, size);
    if (ptr) list_add(ptr, num * size, typeHash);
    return ptr;
}

void* realloc_hook(void* ptr, size_t size, size_t typeHash) {
    if (ptr) list_remove(ptr);
    ptr = realloc(ptr, size);
    if (typeHash == 0)
        printf("Warning: typeHash is %zu in realloc_hook, size=%zu\n", typeHash, size);
    if (ptr) list_add(ptr, size, typeHash);
    return ptr;
}

void free_hook(void* ptr) {
    if (ptr) list_remove(ptr);
    free(ptr);
}

extern void* _Znwm(size_t size); // operator new
extern void* _ZdlPv(void* ptr); // operator delete

extern void* _Znam(size_t size); // operator new[]
extern void* _ZdaPv(void* ptr); // operator delete[]

void* new_hook(size_t size, size_t typeHash) {
    void* ptr = _Znwm(size);
    if (typeHash == 0)
        printf("Warning: typeHash is 0 in new_hook, size=%zu\n", size);
    if (ptr) list_add(ptr, size, typeHash);
    return ptr;
}

void* new_array_hook(size_t size, size_t typeHash, size_t cookie) {
    void* ptr = _Znam(size);
    if (typeHash == 0)
        printf("Warning: typeHash is 0 in new_array_hook, size=%zu\n", size);
    if (ptr) list_add(ptr, size - cookie, typeHash);
    return ptr;
}

void delete_hook(void* ptr) {
    if (ptr) list_remove(ptr);
    _ZdlPv(ptr);
}

void delete_array_hook(void* ptr) {
    if (ptr) list_remove(ptr);
    _ZdaPv(ptr);
}

extern char heapLogFileName[]; // e.g. "A_RTL_P1_Heap_%ld.in\00"
extern uint64_t curr_timestamp; // @curr_timestamp = internal global i64 0
// dump the heap variables to the binary file
void dump_heap_vars() {
    char filename[MAX_FILENAME_LEN];
    snprintf(filename, MAX_FILENAME_LEN, heapLogFileName, curr_timestamp);
    FILE* file = fopen(filename, "wb");
    if (!file) return;

    pthread_mutex_lock(&mem_mutex);
    MemEntry* current = list_head;
    while(current) {
        fwrite(&current->IRTypeHash, sizeof(size_t), 1, file); // Write the IRTypeHash to the file
        fwrite(&current->ptr, sizeof(void*), 1, file); // Write the memory address to the file
        fwrite(&current->size, sizeof(size_t), 1, file); // Write the size to the file
        fwrite(current->ptr, current->size, 1, file); // According to size, write the memory content to the file
        current = current->next;
    }
    pthread_mutex_unlock(&mem_mutex);
    fclose(file);
}

// dump the heap status to the csv file
void dump_heap_status(const char* suffix) {
    char filename[MAX_FILENAME_LEN];
    snprintf(filename, MAX_FILENAME_LEN, "memtrace_%s.csv", suffix);
    FILE* file = fopen(filename, "w");
    if (!file) return;

    fputs("Address,Size,Type\n", file);

    pthread_mutex_lock(&mem_mutex);
    MemEntry* current = list_head;
    while (current) {
        // printf("%s\n", &typeid(*current->ptr).name());
        fprintf(file, "%p,%zu,%zu\n", current->ptr, current->size, current->IRTypeHash);
        current = current->next;
    }
    pthread_mutex_unlock(&mem_mutex);

    fclose(file);
}
}