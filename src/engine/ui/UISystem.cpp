#pragma once
#include "UISystem.h"
#include "Event.h"
#include "UIElement.h"


//UI system with event bubbling
void UISystem::DispatchEvent(UIEvent& event){
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
