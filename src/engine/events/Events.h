#pragma once
#include <iostream>
#include "Event.h"
#include "Entity.h"
#include "UIElement.h"

class WindowResizeEvent : public Event {
    private:
        int width;
        int height;

    public:
        WindowResizeEvent(int w, int h) : width(w), height(h) {}

        int GetWidth() const { return width; }
        int GetHeight() const { return height; }

        DEFINE_EVENT_TYPE_CATEGORY(WindowResizeEvent, static_cast<int>(EventCategory::Window));
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

        DEFINE_EVENT_TYPE_CATEGORY(KeyPressEvent,
                            static_cast<int>(EventCategory::Input) |
                            static_cast<int>(EventCategory::Keyboard))
};

//example with categories
class KeyReleaseEvent : public Event {
    private:
        int keyCode;
        bool repeat;

    public:
        KeyReleaseEvent(int key, bool isRepeat) : keyCode(key), repeat(isRepeat) {}

        int GetKeyCode() const { return keyCode; }
        bool IsRepeat() const { return repeat; }

        DEFINE_EVENT_TYPE_CATEGORY(KeyReleaseEvent,
                            static_cast<int>(EventCategory::Input) |
                            static_cast<int>(EventCategory::Keyboard))
};


/*
    Event Bubbling and Capturing
        In hierarchical systems like UI, events can propagate through the hierarchy in two ways:

            Event Bubbling - The event starts at the target element and "bubbles up" through parent elements in the hierarchy. 
            For example, a click event on a button first triggers on the button, then on its container, and continues up to the root element.

            Event Capturing - The event starts at the root element and travels down the hierarchy to the target element (the opposite direction of bubbling).

        This approach allows parent elements to intercept and handle events triggered on their children, while also giving children the ability to stop propagation if needed. 
*/
class UIEvent : public Event {
    private:
        UIElement* target;
        bool bubbles;
        bool cancelBubble = false;

    public:
        UIEvent(UIElement* targetElement, bool bubbling = true) : target(targetElement), bubbles(bubbling) {}

        UIElement* GetTarget() const { return target; }

        bool Bubbles() const { return bubbles; }

        void StopPropagation(){
            cancelBubble = true;
        }

        bool IsPropagationStopped() const {
            return cancelBubble;
        }

        DEFINE_EVENT_TYPE_CATEGORY(UIEvent, static_cast<int>(EventCategory::UI))
};

class CollisionEvent : public Event {
    private:
        Entity* entity1;
        Entity* entity2;
    public:
        CollisionEvent(Entity* e1, Entity* e2) : entity1(e1), entity2(e2) {}

        Entity* GetEntity1() const { return entity1; }
        Entity* GetEntity2() const { return entity2; }

        DEFINE_EVENT_TYPE_CATEGORY(CollisionEvent, static_cast<int>(EventCategory::Physics))
};