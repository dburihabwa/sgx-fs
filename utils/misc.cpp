#include <cstdio>
#include <iostream>

#include "misc.h"

using namespace std;

void print_sealed_data(sgx_sealed_data_t *block) {
    printf("(%p) = ", block);
    cout << "{" << endl;
    cout << "\taes_data.payload_size: " << block->aes_data.payload_size << endl;
    cout << "\taes_data.payload_tag";
    printf("(%p): ", block->aes_data.payload_tag);
    print_buffer(block->aes_data.payload_tag, SGX_SEAL_TAG_SIZE);
    cout << "\taes_data.payload";
    printf("(%p): ", block->aes_data.payload);
    print_buffer(block->aes_data.payload, block->aes_data.payload_size);
    cout << "}" << endl;
}


void print_buffer(const uint8_t *buffer, size_t payload_size) {
    printf("[ ");
    for (size_t i = 0; i < payload_size; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("]\n");
}