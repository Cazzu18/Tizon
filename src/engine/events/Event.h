#pragma once
#include <iostream>
#include <string>
/*
    Why an Event System?

        subsystems need to communicate with each other efficiently.
            1. Physics needs to notify Audio when collisions occur.
            2. Input needs to notify Game Logic when buttons are pressed.
            3. Game Logic needs to notify Rendering when objects change.
            4. Resource Management needs to notify Rendering when assets are loaded.
            ETC...

    Principals:
        1. Decoupling - Minimize dependencies between event producers and consumers.
        2. Type Safety - Use the type system to prevent errors.
        3. Performance - Efficiently dispatch events, especially for high-frequency events.
        4. Flexibility - Support different event delivery patterns (immediate, queued, etc.).
        5. Debugging - Make it easy to debug event flow.
*/

//Event categories(enum classes are strongly typed versions of an enum)
enum class EventCategory {
    None = 0,
    Application = 1 << 0, //bitwise left shift by 0 so remains 00000001
    Input = 1 << 1, //shift by one to the left 00000010 which represents 2
    Keyboard = 1 << 2, //4
    Mouse = 1 << 3, // ...
    MouseButton = 1 << 4,
    Window = 1 << 5,
    UI = 1 << 6,
    Physics = 1 << 7
};

class Event{
    public:
        virtual ~Event() =default;

        //Getting the type of Event
        virtual const char* GetType() const = 0;

        //Cloning the event(for queued events)
        virtual Event* Clone() const = 0;

        //Get categories this event belongs to
        virtual int GetCategoryFlags() const = 0;

        //Checking if event is in category
        bool IsInCategory(EventCategory category) const {
            return GetCategoryFlags() & static_cast<int>(category);
        }
};

//Macro to help define event types. This generates three functions for EVERY event class
//#type -> The # in macro means convert the macro argument into a string literal
//Enhanced  the macro to define event types with categories
#define DEFINE_EVENT_TYPE_CATEGORY(type, categoryFlags) \
    static const char* GetStaticType() { return #type; } \
    virtual const char* GetType() const override { return GetStaticType(); } \
    virtual Event* Clone() const override { return new type(*this); } \
    virtual int GetCategoryFlags() const override { return categoryFlags; }