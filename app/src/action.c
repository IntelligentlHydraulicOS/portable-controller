#include "action.h"
#include <string.h>

static const action_mapping_t g_action_mapping[] = {
    {0,  1, "J5_K1"},
    {1,  1, "J5_K2"},
    {2,  1, "J3_K1"},
    {3,  1, "J3_K2"},
    {4,  1, "J1_K1"},
    {5,  1, "J1_K2"},
    {6,  1, "J11_K1"},
    {7,  1, "J11_K2"},
    {8,  1, "J9_K1"},
    {9,  1, "J9_K2"},
    {10, 1, "J7_K1"},
    {11, 1, "J7_K2"},
    {12, 1, "J6_K1"},
    {13, 1, "J6_K2"},
    {14, 1, "J4_K1"},
    {15, 1, "J4_K2"},
    {16, 1, "J2_K1"},
    {17, 1, "J2_K2"},
    {18, 1, "J12_K1"},
    {19, 1, "J12_K2"},
    {20, 1, "J10_K1"},
    {21, 1, "J10_K2"},
    {22, 1, "J8_K1"},
    {23, 1, "J8_K2"},
    {24, 1, "J13_K1"},
    {25, 1, "J13_K2"},
    {26, 1, "J14_K1"},
    {27, 1, "J14_K2"},
};

#define ACTION_MAPPING_COUNT  (sizeof(g_action_mapping) / sizeof(action_mapping_t))

uint8_t get_channel_offset_by_name(const char *s)
{
    if (s == NULL)
    {
        return 0xFF;
    }

    for (uint8_t i = 0; i < ACTION_MAPPING_COUNT; i++)
    {
        if (strcmp(s, g_action_mapping[i].name) == 0)
        {
            return g_action_mapping[i].channel_offset;
        }
    }
    return 0xFF;
}

uint8_t get_channel_offset_by_action_id(uint8_t action_id)
{
    for (uint8_t i = 0; i < ACTION_MAPPING_COUNT; i++)
    {
        if (action_id == g_action_mapping[i].action_id)
        {
            return g_action_mapping[i].channel_offset;
        }
    }
    return 0xFF;
}
