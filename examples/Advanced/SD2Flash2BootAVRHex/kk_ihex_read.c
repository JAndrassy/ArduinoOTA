/*
 * kk_ihex_read.c: A simple library for reading the Intel HEX (IHEX) format.
 *
 * See the header `kk_ihex.h` for instructions.
 *
 * Copyright (c) 2013-2019 Kimmo Kulovesi, https://arkku.com/
 * Provided with absolutely no warranty, use at your own risk only.
 * Use and distribute freely, mark modified copies as such.
 */

#include "kk_ihex_read.h"

#define IHEX_START ':'

#define ADDRESS_HIGH_MASK ((ihex_address_t) 0xFFFF0000U)

enum ihex_read_state {
    READ_WAIT_FOR_START = 0,
    READ_COUNT_HIGH = 1,
    READ_COUNT_LOW,
    READ_ADDRESS_MSB_HIGH,
    READ_ADDRESS_MSB_LOW,
    READ_ADDRESS_LSB_HIGH,
    READ_ADDRESS_LSB_LOW,
    READ_RECORD_TYPE_HIGH,
    READ_RECORD_TYPE_LOW,
    READ_DATA_HIGH,
    READ_DATA_LOW
};

#define IHEX_READ_RECORD_TYPE_MASK 0x07
#define IHEX_READ_STATE_MASK 0x78
#define IHEX_READ_STATE_OFFSET 3

void
ihex_begin_read (struct ihex_state * const ihex) {
    ihex->address = 0;
#ifndef IHEX_DISABLE_SEGMENTS
    ihex->segment = 0;
#endif
    ihex->flags = 0;
    ihex->line_length = 0;
    ihex->length = 0;
}

void
ihex_read_at_address (struct ihex_state * const ihex, ihex_address_t address) {
    ihex_begin_read(ihex);
    ihex->address = address;
}

#ifndef IHEX_DISABLE_SEGMENTS
void
ihex_read_at_segment (struct ihex_state * const ihex, ihex_segment_t segment) {
    ihex_begin_read(ihex);
    ihex->segment = segment;
}
#endif

void
ihex_end_read (struct ihex_state * const ihex) {
    uint_fast8_t type = ihex->flags & IHEX_READ_RECORD_TYPE_MASK;
    uint_fast8_t sum;
    if ((sum = ihex->length) == 0 && type == IHEX_DATA_RECORD) {
        return;
    }
    {
        // compute and validate checksum
        const uint8_t * const eptr = ihex->data + sum;
        const uint8_t *r = ihex->data;
        sum += type + (ihex->address & 0xFFU) + ((ihex->address >> 8) & 0xFFU);
        while (r != eptr) {
            sum += *r++;
        }
        sum = (~sum + 1U) ^ *eptr; // *eptr is the received checksum
    }
    if (ihex_data_read(ihex, type, (uint8_t) sum)) {
        if (type == IHEX_EXTENDED_LINEAR_ADDRESS_RECORD) {
            ihex->address &= 0xFFFFU;
            ihex->address |= (((ihex_address_t) ihex->data[0]) << 24) |
                             (((ihex_address_t) ihex->data[1]) << 16);
#ifndef IHEX_DISABLE_SEGMENTS
        } else if (type == IHEX_EXTENDED_SEGMENT_ADDRESS_RECORD) {
            ihex->segment = (ihex_segment_t) ((ihex->data[0] << 8) | ihex->data[1]);
#endif
        }
    }
    ihex->length = 0;
    ihex->flags = 0;
}

void
ihex_read_byte (struct ihex_state * const ihex, const char byte) {
    uint_fast8_t b = (uint_fast8_t) byte;
    uint_fast8_t len = ihex->length;
    uint_fast8_t state = (ihex->flags & IHEX_READ_STATE_MASK);
    ihex->flags ^= state; // turn off the old state
    state >>= IHEX_READ_STATE_OFFSET;

    if (b >= '0' && b <= '9') {
        b -= '0';
    } else if (b >= 'A' && b <= 'F') {
        b -= 'A' - 10;
    } else if (b >= 'a' && b <= 'f') {
        b -= 'a' - 10;
    } else if (b == IHEX_START) {
        // sync to a new record at any state
        state = READ_COUNT_HIGH;
        goto end_read;
    } else {
        // ignore unknown characters (e.g., extra whitespace)
        goto save_read_state;
    }

    if (!(++state & 1)) {
        // high nybble, store temporarily at end of data:
        b <<= 4;
        ihex->data[len] = b;
    } else {
        // low nybble, combine with stored high nybble:
        b = (ihex->data[len] |= b);
        // We already know the lowest bit of `state`, dropping it may produce
        // smaller code, hence the `>> 1` in switch and its cases.
        switch (state >> 1) {
        default:
            // remain in initial state while waiting for :
            return;
        case (READ_COUNT_LOW >> 1):
            // data length
            ihex->line_length = b;
#if IHEX_LINE_MAX_LENGTH < 255
            if (b > IHEX_LINE_MAX_LENGTH) {
                ihex_end_read(ihex);
                return;
            }
#endif
            break;
        case (READ_ADDRESS_MSB_LOW >> 1):
            // high byte of 16-bit address
            ihex->address &= ADDRESS_HIGH_MASK; // clear the 16-bit address
            ihex->address |= ((ihex_address_t) b) << 8U;
            break;
        case (READ_ADDRESS_LSB_LOW >> 1):
            // low byte of 16-bit address
            ihex->address |= (ihex_address_t) b;
            break;
        case (READ_RECORD_TYPE_LOW >> 1):
            // record type
            if (b & ~IHEX_READ_RECORD_TYPE_MASK) {
                // skip unknown record types silently
                return;
            }
            ihex->flags = (ihex->flags & ~IHEX_READ_RECORD_TYPE_MASK) | b;
            break;
        case (READ_DATA_LOW >> 1):
            if (len < ihex->line_length) {
                // data byte
                ihex->length = len + 1;
                state = READ_DATA_HIGH;
                goto save_read_state;
            }
            // end of line (last "data" byte is checksum)
            state = READ_WAIT_FOR_START;
        end_read:
            ihex_end_read(ihex);
        }
    }
save_read_state:
    ihex->flags |= state << IHEX_READ_STATE_OFFSET;
}

void
ihex_read_bytes (struct ihex_state * restrict ihex,
                 const char * restrict data,
                 ihex_count_t count) {
    while (count > 0) {
        ihex_read_byte(ihex, *data++);
        --count;
    }
}

