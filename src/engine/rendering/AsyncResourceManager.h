#pragma once
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include "ResourceManager.h"

/*
    For LARGE Resources, it's often beneficial to load them asynchronously to avoid blocking the main thread

    Essentially:
        Don't make the main thread wait while a resource is being loaded. Put the loading operation into a queue and let another thread perform it.

    Resource Streaming
        For very large resources like high-resolution textures or detailed meshes, you might want to implement streaming:
            Level of Detail (LOD) - Load lower-resolution versions first, then progressively load higher-resolution versions.
            Texture Streaming - Load mipmap levels progressively, starting with the smallest.
            Mesh Streaming - Load simplified versions of meshes first, then add detail.


*/
class AsyncResourceManager {
    private:
        ResourceManager resourceManager;
        std::thread workerThread;
        std::queue<std::function<void()>> taskQueue; //std::function is a c++ object that can store a callable function. So we create a queue of callable functions
        std::mutex queueMutex; //Mutual Exclusion is a synchronization primitive that protects shared resources from concurrent access, helping prevent race conditions in multithreaded programs.
        std::condition_variable condition;
        bool running = false;
    public:
        AsyncResourceManager() {
            Start(); //automatically starting worker thread upon object creation
        }

        ~AsyncResourceManager(){
            Stop(); //stopping worker thread after managaer that owns it has disappeared
        }

        void Start(){
            running = true;

            //creating a new thread that executes the lambda. Here, the lambda calls WorkerThread(). [this]() is a LAMBDA CAPTURE. The new thread needs to know wich AsyncResourceManager object it should call WorkerThread() on because WorkerThread() is a member function of AsyncResourceManager
            //[this] captures the current object's this pointer
            workerThread = std::thread([this](){
                WorkerThread();
            });
        }

        void Stop(){ 
            {
                std::lock_guard<std::mutex> lock(queueMutex); //lock gaurd locks queueMutex immediately and unlocks it when lock goes out of scope
                running = false;
            } //lock destroyed here -> mutex unlocked

            condition.notify_one(); //waking up worker thread if it is asleep so that it realizes running is now false and works to shutdown(wait() in WorkerThread finishes then returns on the preceding conditional check)

            if(workerThread.joinable()){
                workerThread.join(); //mainThread is calling this. MainThread saying "I will wait here until the worker thread completely finishes"
            }
        }

        //the std::function<void(ResourceHandle<T>)> callback says "Give me a function that accepts ResourceHandle<T> and returns nothing(void)"
        template <typename T>
        void LoadAsync(const std::string& resourceId, std::function<void(ResourceHandle<T>)> callback){
            std::lock_guard<std::mutex> lock(queueMutex);

            //adding tasks(as lambdas)
            taskQueue.push([this, resourceId, callback](){
                auto handle = resourceManager.Load<T>(resourceId); //worker thread loads resource
                callback(handle); //worker thread calls the user provided callback
            });
            condition.notify_one(); //This tells the worker that There's a new task available(we just pushed one). This awakes the worker
        }
    private:
        void WorkerThread(){
            while(running){

                //creating a variable that can hole on queued task. The extra {} creates a scope so we don't hold the mutex while executing the task
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex); //locking the mutex. unique_lock is similar to lock_gaurd but has additional capabilites that condition_variable::wait( requires
                    
                    //we pass the lock and a lambda. This basically says Go to sleep until there's a task or we're shutting down
                    //the condition variable needs the mutex because they work together. When the worker sleeps, wait() temporarily releases the mitex so another thread can acquire it and add a task
                    condition.wait(lock, [this]() {
                        return !taskQueue.empty() || !running;
                    });

                    //exit if we're shutting down AND there are no tasks left in the queue
                    if(!running && taskQueue.empty()){
                        return;
                    }    

                    task = std::move(taskQueue.front()); //taking the first task out of the queue and moving(transferring ownership) it to the local variable
                    taskQueue.pop();
                }

                task(); //executing task
            }
        }
};

/*
    USAGE EXAMPLE:

        AsyncResourceManager asyncResourceManager;

        asyncResourceManager.LoadAsync<Texture>("large_texture", [](ResourceHandle<Texture> texture) {
            // This callback will be called when the texture is loaded
            if (texture) {
                std::cout << "Texture loaded successfully!" << std::endl;
            } else {
                std::cout << "Failed to load texture." << std::endl;
            }
        }); //manager out of scope and C++ calls the destructor which calls Stop()
*/