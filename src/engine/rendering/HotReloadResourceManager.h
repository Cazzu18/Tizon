#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <thread>
#include <chrono>
#include "ResourceManager.h"
#include "ResourceHandle.h"


/* To update resources withoiut restarting the application during development */
class HotReloadResourceManager : public ResourceManager {
    private:
        std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps;
        std::thread watcherThread;
        bool running = false;
    public:
        HotReloadResourceManager(){
            StartWatcher();
        }

        ~HotReloadResourceManager(){
            StopWatcher();
        }

        void StartWatcher(){
            running = true;
            watcherThread = std::thread([this](){
                WatcherThread();
            });
        }

        void StopWatcher(){
            running = false;
            if(watcherThread.joinable()){
                watcherThread.join();
            }
        }

        template<typename T>
        ResourceHandle<T> Load(const std::string& resourceId){
            auto handle = ResourceManager::Load<T>(resourceId); //parent class call

            //storing file fileTimestamps
            std::string filePath = GetFilePath<T>(resourceId);
            try{
                fileTimestamps[filePath] = std::filesystem::last_write_time(filePath);
            } catch (const std::filesystem::filesystem_error& e){
                //File doesn't exist or can't be accessedd
            }

            return handle;
        }

    private:
        template<typename T>
        std::string GetFilePath(const std::string& resourceId){
            //determine filepath based on resource type and ID

            if constexpr (std::is_same_v<T, Texture>){
                return "textures/" + resourceId + ".ktx";
            } else if constexpr (std::is_same_v<T, Mesh>){
                return "models/" + resourceId + ".gltf";
            } else if constexpr (std::is_same_v<T, Shader>){
                return "shaders/" + resourceId + ".spv";
            } else {
                return "";
            }
        }

        void WatcherThread(){
            while(running){
                //checking for file changes
                for(auto& [filePath, timestamp] : fileTimestamps){
                    try{
                        auto currentTimestamp = std::filesystem::last_write_time(filePath);
                        if(currentTimestamp != timestamp) {
                            //file has changed and we reload
                            ReloadResource(filePath);
                            timestamp = currentTimestamp;
                        }
                    } catch(const std::filesystem::filesystem_error& e){
                        //File doesn't exist or can't be accessed
                    }
                }

                //sleep to avoid high CPU usage
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        void ReloadResource(const std::string& filePath){
            //Extract resource ID and type from file path
            //Reload the resource
            //...
        }

};