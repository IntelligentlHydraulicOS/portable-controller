#ifndef ACTION_H
#define ACTION_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t channel_offset;
    uint8_t action_id;
    const char *name;
} action_mapping_t;

uint8_t get_channel_offset_by_action_id(uint8_t action_id);
uint8_t get_channel_offset_by_name(const char *s);
#endif