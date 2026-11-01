#pragma once
#include <iostream>
#include "Event.h"

class WindowResizeEvent : public Event {
    private:
        int width;
        int height;

    public:
        WindowResizeEvent(int w, int h) : width(w), height(h) {}

        int GetWidth() const { return width; }
        int GetHeight() const { return height; }

        DEFINE_EVENT_TYPE(WindowResizeEvent, static_cast<int>(EventCategory::Window));
};

//example with categories
class KeyPressEvent : public Event {
    private:
        int keyCode;
        bool repeat;

    public:
        KeyPressEvent(int key, bool isRepeat) : keyCode(key), repeat(isRepeat) {}

        int GetKeyCode() const { return keyCode; }
        bool IsRepeat() const { return repeat; }

        DEFINE_EVENT_TYPE(KeyPressEvent,
                            static_cast<int>(EventCategory::Input) |
                            static_cast<int>(EventCategory::Keyboard))
    };