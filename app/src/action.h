#ifndef ACTION_H
#define ACTION_H

#include <stdint.h>
#include <stddef.h>


#define ACTION_NONE                             0x00000000U
#define ACTION_J5_K1                            0x00000001U
#define ACTION_J5_K2                            0x00000002U
#define ACTION_J3_K3                            0x00000004U
#define ACTION_J3_K4                            0x00000008U
#define ACTION_J1_K5                            0x00000010U
#define ACTION_J1_K6                            0x00000020U
#define ACTION_J11_K7                           0x00000040U
#define ACTION_J11_K8                           0x00000080U
#define ACTION_J9_K9                            0x00000100U
#define ACTION_J9_K10                           0x00000200U
#define ACTION_J7_K11                           0x00000400U
#define ACTION_J7_K12                           0x00000800U
#define ACTION_J6_K13                           0x00001000U
#define ACTION_J6_K14                           0x00002000U
#define ACTION_J4_K15                           0x00004000U
#define ACTION_J4_K16                           0x00008000U
#define ACTION_J2_K17                           0x00010000U
#define ACTION_J2_K18                           0x00020000U
#define ACTION_J12_K19                          0x00040000U
#define ACTION_J12_K20                          0x00080000U
#define ACTION_J10_K21                          0x00100000U
#define ACTION_J10_K22                          0x00200000U
#define ACTION_J8_K23                           0x00400000U
#define ACTION_J8_K24                           0x00800000U
#define ACTION_J13_K25                          0x02000000U
#define ACTION_J13_K26                          0x01000000U
#define ACTION_J14_K27                          0x08000000U
#define ACTION_J14_K28                          0x04000000U

typedef struct {
    uint8_t channel_offset;
    uint32_t action_id;
    const char *name;
} action_mapping_t;

uint8_t get_channel_offset_by_action_id(uint32_t action_id);
uint8_t get_channel_offset_by_name(const char *s);
#endif