/* PBS and PDN: a car's specs and body records (packed or not) */
#include "fuzz/fuzz.h"
#include "formats/pbs.h"
#include "formats/pdn.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t pbs[PBS_SIZE], pdn[PDN_SIZE];
    if (pbs_read(pbs, data, size)) (void)pbs_checksum_ok(pbs);
    if (pdn_read(pdn, data, size)) (void)pdn_i32(pdn, 0);
    return 0;
}
