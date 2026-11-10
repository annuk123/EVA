
#ifndef EVA_DISPLAY_TYPES_H
#define EVA_DISPLAY_TYPES_H

#include <Arduino.h>

// Controls what appears on the OLED.
enum DisplayMode : uint8_t {
    DISPLAY_FACE,
    DISPLAY_HALLOWEEN
};

// Describes EVA's operational state.
enum EvaState : uint8_t {
    EVA_IDLE,
    EVA_LISTENING,
    EVA_THINKING,
    EVA_SPEAKING
};

#endif
