#include "action.h"
#include <string.h>

static const action_mapping_t g_action_mapping[] = {
    {1,  ACTION_J5_K1, "J5_K1"},
    {2,  ACTION_J5_K2, "J5_K2"},
    {3,  ACTION_J3_K3, "J3_K3"},
    {4,  ACTION_J3_K4, "J3_K4"},
    {5,  ACTION_J1_K5, "J1_K5"},
    {6,  ACTION_J1_K6, "J1_K6"},
    {7,  ACTION_J11_K7, "J11_K7"},
    {8,  ACTION_J11_K8, "J11_K8"},
    {9,  ACTION_J9_K9, "J9_K9"},
    {10, ACTION_J9_K10, "J9_K10"},
    {11, ACTION_J7_K11, "J7_K11"},
    {12, ACTION_J7_K12, "J7_K12"},
    {13, ACTION_J6_K13, "J6_K13"},
    {14, ACTION_J6_K14, "J6_K14"},
    {15, ACTION_J4_K15, "J4_K15"},
    {16, ACTION_J4_K16, "J4_K16"},
    {17, ACTION_J2_K17, "J2_K17"},
    {18, ACTION_J2_K18, "J2_K18"},
    {19, ACTION_J12_K19, "J12_K19"},
    {20, ACTION_J12_K20, "J12_K20"},
    {21, ACTION_J10_K21, "J10_K21"},
    {22, ACTION_J10_K22, "J10_K22"},
    {23, ACTION_J8_K23, "J8_K23"},
    {24, ACTION_J8_K24, "J8_K24"},
    {25, ACTION_J13_K25, "J13_K25"},
    {26, ACTION_J13_K26, "J13_K26"},
    {27, ACTION_J14_K27, "J14_K27"},
    {28, ACTION_J14_K28, "J14_K28"},
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

uint8_t get_channel_offset_by_action_id(uint32_t action_id)
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
