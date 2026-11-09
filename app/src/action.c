#include "action.h"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(action, LOG_LEVEL_INF);


keyConfig_t g_key_config[TOTAL_KEY_NUMBER] = 
{
    {1,   FUNCTION_ID_LIFTING,                  ACTION_J3_K3,    ACTION_NONE},
    {2,   FUNCTION_ID_LOWERING,                 ACTION_J3_K4,    ACTION_NONE},
    {3,   FUNCTION_ID_MOVE,                     ACTION_J1_K6,    ACTION_NONE},
    {4,   FUNCTION_ID_PUSH_RAIL,                ACTION_J1_K5,    ACTION_NONE},

    {5,   FUNCTION_ID_LIFT_BOTTOM,              ACTION_J2_K17,   ACTION_NONE},
    {6,   FUNCTION_ID_SPRAY_FRONT,              ACTION_J2_K18,   ACTION_NONE},
    {7,   FUNCTION_ID_STRETCH_GURAD,            ACTION_J4_K15,   ACTION_NONE},
    {8,   FUNCTION_ID_TAKE_BACK_GURAD,          ACTION_J4_K16,   ACTION_NONE},

    {9,   FUNCTION_ID_STRETCH_SIDE_GURAD,       ACTION_J5_K1,    ACTION_NONE},
    {10,  FUNCTION_ID_TAKE_BACK_SIDE_GURAD,     ACTION_J5_K2,    ACTION_NONE},
    {11,  FUNCTION_ID_STRETCH_FLEXIBLE_BEAM,    ACTION_J7_K11,   ACTION_NONE},
    {12,  FUNCTION_ID_TAKE_BACK_FLEXIBLE_BEAM,  ACTION_J7_K12,   ACTION_NONE},
    {13,  FUNCTION_ID_STERETCH_BACK_RAIL,       ACTION_J8_K23,   ACTION_NONE},
    {14,  FUNCTION_ID_PULL_BACK_RAIL,           ACTION_J8_K24,   ACTION_NONE},
    {15,  FUNCTION_ID_PULL_RAIL,                ACTION_J1_K6,    ACTION_NONE},

    {16,  FUNCTION_ID_STRETCH_FRONT_BEAM,       ACTION_J6_K13,   ACTION_NONE},
    {17,  FUNCTION_ID_TAKE_BACK_FRONT_BEAM,     ACTION_J6_K14,   ACTION_NONE},
    {18,  FUNCTION_ID_UP_BACK_WASHING,          ACTION_J9_K9,    ACTION_NONE},
    {19,  FUNCTION_ID_DOWN_BACK_WASHING,        ACTION_J9_K10,   ACTION_NONE},
};

#define ACTION_MAPPING_COUNT  (sizeof(g_key_config) / sizeof(keyConfig_t))

uint32_t get_channel_offset_by_action_id(uint32_t action_id)
{
    for (uint8_t i = 0; i < ACTION_MAPPING_COUNT; i++)
    {
        if (action_id == g_key_config[i].functionID)
        {
            LOG_INF("Found %d action_id %u, channel:%x", i, action_id, g_key_config[i].sbc1Channel);
            // Assuming you want to return the SBC1 channel offset
            return g_key_config[i].sbc1Channel;
        }
    }
    return 0xFF;
}
