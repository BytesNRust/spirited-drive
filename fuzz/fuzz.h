/* The fuzz harnesses (docs/fuzzing.md): one per reader of a file a mod can carry. Each defines LLVMFuzzerTestOneInput;
 * with -DTNFS_FUZZ=ON it links libFuzzer, and every build links it to replay.c, which runs it over the files kept in
 * tests/fuzz/<reader>/ (the inputs that once broke a reader, and small made-up seeds) as a ctest. */
#ifndef TNFS_FUZZ_H
#define TNFS_FUZZ_H

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* Reads every byte of a span a reader handed back, so a span past the input shows under ASan. */
static inline void fuzz_touch(const void *p, size_t n)
{
    const volatile uint8_t *b = (const volatile uint8_t *)p;
    uint8_t x = 0;
    for (size_t i = 0; p != NULL && i < n; i++) x ^= b[i];
    (void)x;
}

#endif
