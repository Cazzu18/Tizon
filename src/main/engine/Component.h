#pragma once

class Entity; //forward declaration

class Component {
    public:
        //managing lifecycle of components
        enum class State {
            Uninitialized,
            Initializing,
            Active,
            Destroying,
            Destroyed
        };

    private:
        State state = State::Uninitialized;
        Entity* owner = nullptr;

    public:
        //virtual is used to achieve runtime polymorphism
        //Adding virtual tells the compiler to look at the actual OBJECT type at runtime instead.
        virtual ~Component(){
            if(state != State::Destroyed){
                OnDestroy();
                state = State::Destroyed;
            }
        } //virtual destructors. Any class with virtual functions must have a virtual destructor

        virtual void Initialize() {
            if(state == State::Uninitialized){
                state = State::Initializing;
                OnInitialize();
                state = State::Active;
            }
        }

        bool IsActive() const {return state == State::Active; }
        void SetOwner(Entity* entity) { owner = entity; }
        Entity* GetOwner() const { return owner; }

    protected:
        virtual void OnInitialize() {}
        virtual void OnDestroy() {}
        virtual void Update(float deltaTime) {}
        virtual void Render() {}

        friend class Entity; //Allowing Entity to call protected methods

};