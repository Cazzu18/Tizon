# Tizón 🪵🔥

A game engine written from scratch in C++ using Vulkan.

## Description

Tizon is a C++ game engine being developed from the ground up, leveraging the power of the Vulkan graphics API. This project aims to provide a foundational engine for game development, focusing on core rendering and game loop functionalities.

## Table of Contents

- [Features](#features)
- [Tech Stack](#tech-stack)
- [Installation](#installation)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)
- [Important Links](#important-links)
- [Footer](#footer)

## Features ✨

- **Vulkan Rendering:** Utilizes the Vulkan API for high-performance graphics.
- **Game Loop Implementation:** Core game loop with event processing and window management.
- **Component-Based Architecture:** Entities and components for modular game object design.
- **3D Model Loading:** Support for loading 3D models (e.g., from `.obj` files).
- **Transformation System:** Handles object positioning, rotation, and scaling.
- **Camera System:** Implements camera components with perspective projection and view matrix generation.
- **Input Handling:** Basic input system for keyboard events.
- **Event System:** A robust event bus for inter-component communication.
- **Scene Management:** Basic scene graph structure with entities and components.
- **Modern C++:** Developed using modern C++ practices.

## Tech Stack 💻

- **Languages:** C++
- **Graphics API:** Vulkan
- **Libraries:**
  - GLFW (Windowing and Input)
  - GLM (Mathematics)
  - tinyobjloader (OBJ Model Loading)
  - stb_image
- **Build System:** CMake

## Installation 🛠️

This project uses `vcpkg` for dependency management. Ensure you have `vcpkg` installed.

1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/Cazzu18/Tizon.git
    cd Tizon
    ```

2.  **Set up vcpkg:**
    Ensure `vcpkg` is available and its toolchain file is configured. The `vcpkg-configuration.json` file indicates the use of the official `microsoft/vcpkg` repository.

3.  **Configure with CMake:**
    Use CMake to configure the build system, pointing to your `vcpkg` installation.
    ```bash
    # Replace %VCPKG_ROOT% with the actual path to your vcpkg installation
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    ```

4.  **Build the Project:**
    Compile the project using CMake.
    ```bash
    cmake --build build --config Debug
    ```

## Usage 🎮

To run the Tizon engine:

1.  **Navigate to the build directory** (e.g., `build/Debug`).
2.  **Execute the compiled application:**
    ```bash
    .\Tizon.exe
    ```

The engine will initialize a window and begin its main loop, rendering a scene

### Core Concepts:

-   **Window Management:** Uses GLFW for creating and managing the application window.
-   **Vulkan Initialization:** Sets up Vulkan instance, physical device, logical device, and swap chain.
-   **Rendering Pipeline:** Configures graphics pipeline with shaders, vertex input, rasterization, and color blending.
-   **Entity-Component System:** Game objects are represented as `Entity` objects, with behavior and data defined by attached `Component`s (e.g., `TransformComponent`, `CameraComponent`, `MeshComponent`).
-   **Event Handling:** An `EventBus` facilitates communication between components, handling input and window events.

## Project Structure 📂

```
Tizon/
├── assets/
│   ├── models/
│   │   ├── Sugarcube_Corner.obj
│   │   └── viking_room.obj
├── build/
├── shaders/
│   └── source/
│       └── shader.slang
├── src/
│   ├── app/
│   │   └── main.cpp
│   ├── engine/
│   │   ├── core/
│   │   │   ├── Component.h
│   │   │   ├── ComponentTypeIDSystem.h
│   │   │   ├── Entity.cpp
│   │   │   └── Entity.h
│   │   ├── events/
│   │   │   ├── Event.h
│   │   │   ├── EventBus.cpp
│   │   │   ├── EventBus.h
│   │   │   ├── EventDispatcher.h
│   │   │   ├── EventListener.h
│   │   │   └── Events.h
│   │   ├── input/
│   │   │   ├── InputSystem.cpp
│   │   │   └── InputSystem.h
│   │   ├── platform/
│   │   │   └── window/
│   │   │       ├── Window.cpp
│   │   │       └── Window.h
│   │   ├── rendering/
│   │   │   ├── AsyncResourceManager.h
│   │   │   ├── HotReloadResourceManager.h
│   │   │   ├── Material.cpp
│   │   │   ├── Material.h
│   │   │   ├── Mesh.cpp
│   │   │   ├── Mesh.h
│   │   │   ├── Resource.h
│   │   │   ├── ResourceHandle.h
│   │   │   ├── ResourceManager.h
│   │   │   ├── Shader.h
│   │   │   ├── Texture.h
│   │   │   ├── culling/
│   │   │   │   └── CullingSystem.h
│   │   │   ├── vulkan/
│   │   │   │   ├── RenderPassManager.h
│   │   │   │   ├── Renderer.h
│   │   │   │   └── Rendergraph.h
│   │   ├── scene/
│   │   │   ├── components/
│   │   │   │   ├── CameraComponent.cpp
│   │   │   │   ├── CameraComponent.h
│   │   │   │   ├── CameraController.cpp
│   │   │   │   ├── MeshComponent.cpp
│   │   │   │   ├── MeshComponent.h
│   │   │   │   ├── PhysicsComponent.cpp
│   │   │   │   ├── PhysicsComponent.h
│   │   │   │   ├── TransformComponent.cpp
│   │   │   │   └── TransformComponent.h
│   │   ├── ui/
│   │   │   ├── UIElement.h
│   │   │   ├── UISystem.cpp
│   │   │   └── UISystem.h
├── CMakeLists.txt
├── CMakeLists copy.txt
└── vcpkg-configuration.json
```

## Contributing 🤝

Contributions are welcome! Please feel free to:

-   **Fork the repository**
-   **Create a new branch** (`git checkout -b feature/your-feature-name`)
-   **Make your changes**
-   **Commit your changes** (`git commit -m 'Add some Feature'`)
-   **Push to the branch** (`git push origin feature/your-feature-name`)
-   **Open a Pull Request**

Please ensure your contributions adhere to the project's coding style and include tests where applicable.

## License 📄

This project does not YET have a license.

## Important Links 🔗

-   **Repository:** [https://github.com/Cazzu18/Tizon](https://github.com/Cazzu18/Tizon)

## Footer

© 2023 Tizón. All rights reserved.

[Back to Top](#tizon-🚀)
