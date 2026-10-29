#pragma once
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>


//forward declarations
class RenderPass;
class RenderTarget;

class RenderPassManager{
    private:
        std::unordered_map<std::string, std::unique_ptr<RenderPass>> renderPasses;
        std::vector<RenderPass*> sortedPasses;
        bool dirty = true;

    public:
        
};