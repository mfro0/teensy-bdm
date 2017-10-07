#include <stdint.h>

uint8_t command_exec(uint8_t *, uint32_t);

typedef enum
{
    CF_BDM = TARGET_TYPE_CF_BDM,
    JTAG = TARGET_TYPE_JTAG
} target_type_e;

typedef enum
{
    NO_RESET_ACTIVITY = 0,
    RESET_DETECTED = 1
} reset_e;

typedef struct
{
    uint8_t target_type   :3;  /* target_type_e */
    uint8_t reset         :1;        /* reset_e */
} cable_status_t;

extern cable_status_t cable_status;
