
#ifndef HALLOWEEN_ANIMATION_H
#define HALLOWEEN_ANIMATION_H

#include <Arduino.h>
#include <U8g2lib.h>

class HalloweenAnimation {

public:

    // Use EVA's existing OLED instance.
    explicit HalloweenAnimation(U8G2& display);

    // Start a new Halloween experience.
    void start(uint32_t now);

// Draws one frame into the existing OLED buffer.
// The caller is responsible for clearBuffer() and sendBuffer().
    void draw(uint32_t now);

private:

    U8G2& display_;

    uint32_t startedAt_ = 0;

    void drawMoon(int x, int y);

    void drawStars(uint32_t frameNo);

    void drawBat(int x, int y, bool flap);

    void drawGhost(int x, int y);

    void drawPumpkin(int x, int y, bool blink);

    void drawHouse();

    void drawTree();
};

#endif
