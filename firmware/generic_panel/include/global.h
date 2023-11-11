#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>  // Provides fixed size integers

// Defines
#define MSG_TIMEOUT 2  // CAN message timeout in ms

// Enumerations
enum DETECT_STATES {
    DETECT_STATE_INIT,       // Not started detect process
    DETECT_STATE_PENDING_1,  // Sent REQ_ID_1, pending ACK_ID_1
    DETECT_STATE_RTS_2,      // Received ACK_ID_1, ready to send REQ_ID_2
    DETECT_STATE_PENDING_2,  // Sent REQ_ID_2, pending SET_ID
    DETECT_STATE_RX_ID,      // Received SET_ID - detection complete
};

// Structures
struct PANEL_ID_T {
    uint32_t uid[3];   // 96-bit UID of STM32
    uint32_t type;     // Panel type (see panel_types.h)
    uint8_t id;        // Panel id used for CAN bus comms
    uint32_t version;  // Panel firmware version
};

#endif  // GLOBAL_H