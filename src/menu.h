#ifndef MENUBUTTONS
#define MENUBUTTONS

#include "raylib.h"
#include "raymath.h"

class Button {
    public:
    int x, y;
    int width = 200;
    int height = 100;
    std::string text = "Button";
    std::function<void(void)> action;

    Button(int centerX, int centerY, int width, int height, std::string text): x(centerX-width/2), y(centerY-height/2), width(width), height(height), text(text) {}
    Button() {}
    bool isMouseTouching(Vector2 pos) const {
        return pos.x > x && pos.y > y && pos.x < (x+width) && pos.y < (y+height);
    }
    void render(Vector2 pos) const {
        Color color = isMouseTouching(pos) ? DARKGRAY : GRAY;
        if (isMouseTouching(pos)) SetMouseCursor(MouseCursor::MOUSE_CURSOR_POINTING_HAND);
        DrawRectangleRounded({(float)x, (float)y, (float)width, (float)height}, 2.0f, 0, color);
        float textWidth = MeasureText(text.c_str(), 20);
        int textHeight = 20;
        DrawText(text.c_str(), x+width/2-textWidth/2, y+height/2-textHeight/2, textHeight, BLACK);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isMouseTouching(pos)) {
            action();
        }
    }
};

#endif