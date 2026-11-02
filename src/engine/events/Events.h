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

        bool Bubbles() const { returns bubbles; }

        void StopPropagation(){
            cancelBubble = true;
        }

        bool IsPropagationStopped() const {
            return cancelBubble;
        }

        DEFINE_EVENT_TYPE_CATEGORY(UIEvent, static_cast<int>(EventCategory::UI))
};

//UI system with event bubbling
class UISystem {
    public:
        void DispatchEvent(UIEvent& event){
            UIElement* target = event.GetTarget();

            //capturing phase (top-down)
            std::vector<UIElement*> path;
            UIElement* current = target;

            while(current){
                path.push_back(current);
                current = current->GetParent();
            }

            //dispatch to each element in the path(bottom-up)
            //rbegin() returns a reverse iterator pointing to the last element of the vector. We can use it to traverse a vector backwards toward the beginning
            //in this case, we have the target at the beginnning of th array and the root at the end, so we start with the root
            for(auto it = path.rbegin(); it != path.rend(); ++it){
                (*it)->OnEvent(event);

                if(event.IsPropagationStopped()){
                    break;
                }
            }
        }
};