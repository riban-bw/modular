#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>  // Provides fixed size integers

// Defines
#define MSG_TIMEOUT 2000  // CAN message timeout in ms

// Enumerations
enum RUN_MODE {
    RUN_MODE_INIT,       // Not started detect process
    RUN_MODE_PENDING_1,  // Sent REQ_ID_1, pending ACK_ID_1
    RUN_MODE_PENDING_2,  // Sent REQ_ID_2, pending SET_ID
    RUN_MODE_RUN,        // Received SET_ID - detection complete
    RUN_MODE_FIRMWARE,   // Firmware update in progress
};

// Structures
struct PANEL_ID_T {
    uint32_t uid[3];   // 96-bit UID of STM32
    uint32_t type;     // Panel type (see panel_types.h)
    uint8_t id;        // Panel id used for CAN bus comms
    uint32_t version;  // Panel firmware version
};

#endif  // GLOBAL_H