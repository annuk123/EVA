
#include "HalloweenAnimation.h"

// Constructor
HalloweenAnimation::HalloweenAnimation(U8G2& display)
    : display_(display) {
}

// Start animation and reset its timeline.
void HalloweenAnimation::start(uint32_t now) {
    startedAt_ = now;
}

// ---------- MOON ----------
void HalloweenAnimation::drawMoon(int x, int y) {

    display_.drawDisc(x, y, 8, U8G2_DRAW_ALL);

    display_.setDrawColor(0);
    display_.drawDisc(x + 5, y - 3, 7, U8G2_DRAW_ALL);

    display_.setDrawColor(1);
}

// ---------- STARS ----------
void HalloweenAnimation::drawStars(uint32_t frameNo) {

    display_.drawPixel(12, 19);
    display_.drawPixel(28, 14);
    display_.drawPixel(37, 22);
    display_.drawPixel(96, 17);
    display_.drawPixel(119, 35);

    if (frameNo % 4 < 2) {

        display_.drawLine(32, 17, 32, 21);
        display_.drawLine(30, 19, 34, 19);

        display_.drawLine(91, 28, 91, 32);
        display_.drawLine(89, 30, 93, 30);
    }
}

// ---------- BATS ----------
void HalloweenAnimation::drawBat(
    int x,
    int y,
    bool flap
) {

    int wingY = flap ? y - 5 : y + 1;

    display_.drawLine(x, y, x - 4, wingY);
    display_.drawLine(x - 4, wingY, x - 9, y - 2);
    display_.drawLine(x - 9, y - 2, x - 11, y + 3);
    display_.drawLine(x - 11, y + 3, x - 5, y + 2);
    display_.drawLine(x - 5, y + 2, x, y + 1);

    display_.drawLine(x, y, x + 4, wingY);
    display_.drawLine(x + 4, wingY, x + 9, y - 2);
    display_.drawLine(x + 9, y - 2, x + 11, y + 3);
    display_.drawLine(x + 11, y + 3, x + 5, y + 2);
    display_.drawLine(x + 5, y + 2, x, y + 1);

    display_.drawPixel(x, y);
}

// ---------- GHOST ----------
void HalloweenAnimation::drawGhost(int x, int y) {

    display_.drawCircle(x, y + 5, 6, U8G2_DRAW_ALL);

    display_.drawLine(x - 6, y + 5, x - 6, y + 16);
    display_.drawLine(x + 6, y + 5, x + 6, y + 16);

    display_.drawLine(x - 6, y + 16, x - 3, y + 13);
    display_.drawLine(x - 3, y + 13, x, y + 16);

    display_.drawLine(x, y + 16, x + 3, y + 13);
    display_.drawLine(x + 3, y + 13, x + 6, y + 16);

    display_.drawDisc(x - 2, y + 5, 1, U8G2_DRAW_ALL);
    display_.drawDisc(x + 2, y + 5, 1, U8G2_DRAW_ALL);
}

// ---------- PUMPKIN ----------
void HalloweenAnimation::drawPumpkin(
    int x,
    int y,
    bool blink
) {

    display_.drawCircle(x, y, 9, U8G2_DRAW_ALL);

    // Stem
    display_.drawBox(x - 1, y - 13, 3, 5);

    // Pumpkin ribs
    display_.drawLine(x - 4, y - 7, x - 4, y + 7);
    display_.drawLine(x + 4, y - 7, x + 4, y + 7);

    // Eyes
    if (blink) {

        display_.drawLine(x - 5, y - 2, x - 2, y - 2);
        display_.drawLine(x + 2, y - 2, x + 5, y - 2);

    } else {

        display_.drawTriangle(
            x - 6, y - 4,
            x - 2, y - 4,
            x - 4, y + 1
        );

        display_.drawTriangle(
            x + 2, y - 4,
            x + 6, y - 4,
            x + 4, y + 1
        );
    }

    // Jagged mouth
    display_.drawLine(x - 5, y + 4, x - 2, y + 7);
    display_.drawLine(x - 2, y + 7, x, y + 4);

    display_.drawLine(x, y + 4, x + 2, y + 7);
    display_.drawLine(x + 2, y + 7, x + 5, y + 4);
}

// ---------- HAUNTED HOUSE ----------
void HalloweenAnimation::drawHouse() {

    // Chimney
    display_.drawFrame(73, 19, 7, 11);

    // Roof
    display_.drawLine(40, 30, 63, 15);
    display_.drawLine(63, 15, 87, 30);

    // House body
    display_.drawFrame(46, 29, 35, 24);

    // Windows
    display_.drawFrame(51, 35, 8, 9);
    display_.drawFrame(69, 35, 8, 9);

    // Window crosses
    display_.drawLine(55, 35, 55, 43);
    display_.drawLine(51, 39, 58, 39);

    display_.drawLine(73, 35, 73, 43);
    display_.drawLine(69, 39, 76, 39);

    // Door
    display_.drawFrame(61, 41, 8, 12);
    display_.drawPixel(66, 47);
}

// ---------- CROOKED TREE ----------
void HalloweenAnimation::drawTree() {

    display_.drawLine(7, 54, 10, 29);
    display_.drawLine(10, 29, 4, 23);
    display_.drawLine(10, 29, 17, 21);
    display_.drawLine(8, 40, 2, 34);
    display_.drawLine(9, 36, 16, 31);
}

// ---------- MAIN ANIMATION ----------
void HalloweenAnimation::draw(uint32_t now) {
    display_.setDrawColor(1);

    // Time since Halloween Mode started.
    const uint32_t elapsed = now - startedAt_;

    // Approximately one animation frame per 100 ms.
    const uint32_t frameNo = elapsed / 100U;

    // Caller clears the OLED buffer before this function.
    // Caller sends the buffer after this function.

    // STAGE 1: Darkness
    if (elapsed < 500) {
        return;
    }

    // STAGE 2: Haunted sky
    drawMoon(108, 23);
    drawStars(frameNo);

    // STAGE 3: Haunted environment
    if (elapsed >= 1500) {

        drawTree();
        drawHouse();
    }

    // STAGE 4: Halloween characters
    if (elapsed >= 2500) {

        // Flying bat
        const int batX =
            8 + ((frameNo * 2U) % 115U);

        drawBat(
            batX,
            20 + ((frameNo / 4U) % 5U),
            (frameNo / 2U) % 2U == 0
        );

        // Floating ghost
        const int ghostBob = (frameNo / 5U) % 3U;

        drawGhost(25, 31 + ghostBob);

        // Blinking pumpkin
        drawPumpkin(
            105,
            44,
            frameNo % 35U < 3U
        );
    }

    // STAGE 5: Grand Halloween reveal
    if (elapsed >= 3500) {

        display_.drawRFrame(1, 1, 126, 11, 2);

        display_.setFont(u8g2_font_5x7_tf);

        const char* title = "HAPPY HALLOWEEN";

        const int titleWidth = display_.getStrWidth(title);

        display_.drawStr(
            (128 - titleWidth) / 2,
            9,
            title
        );

        // Scrolling text
        display_.drawLine(0, 55, 127, 55);

        const char* message = "TRICK OR TREAT  *  BOO!";

        const int messageWidth =
            display_.getStrWidth(message);

        const int offset =
            (frameNo * 2U) % (messageWidth + 128);

        display_.drawStr(
            128 - offset,
            63,
            message
        );
    }
}
