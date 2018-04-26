#ifndef FUSEGX_MISC_H
#define FUSEGX_MISC_H

#include "sgx_urts.h"
#include "sgx_utils/sgx_utils.h"

void print_sealed_data(sgx_sealed_data_t *block);

void print_buffer(const uint8_t *buffer, size_t payload_size);
#endif //FUSEGX_MISC_H
