# DX11Engine

A DirectX 11-based rendering engine built in C++ with HLSL shaders, designed for experimenting with graphics programming, physically-based rendering (PBR), and sandbox visualization.

## Overview

DX11Engine is a modular graphics rendering framework built from the ground up using DirectX 11 and C++. Whether you're interested in learning graphics programming, experimenting with PBR shading techniques, or building a foundation for a custom game engine, this project provides a clean, well-structured codebase to work with. The engine integrates ImGui for intuitive debugging and UI, making it easy to visualize and tweak rendering features in real-time.

## Features

- **Physically Based Rendering (PBR)** – Industry-standard shaders implementing metallic and roughness workflows
- **Sandbox Environment** – Dedicated testing ground for experimenting with graphics features and shader variations
- **ImGui Integration** – Real-time debugging interface using ImGui's docking branch for flexible UI layouts
- **Asset Pipeline** – Built-in support for importing 3D models and textures via Assimp
- **Modular Architecture** – Clean separation of concerns with organized engine modules
- **Cross-Platform Build System** – Premake5-based project generation with MSBuild support

## Installation

### Prerequisites

- **Windows** with Visual Studio 2019 or later
- **DirectX 11** SDK
- **Premake5** executable in your system PATH (or in the repository root)

### Setup Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/flavian101/DX11Engine.git
   cd DX11Engine
   ```

2. **Generate project files:**
   
   **Option A: Using the batch script**
   ```bash
   WindowsGenerateProject.bat
   ```
   
   **Option B: Using Premake5 directly**
   ```bash
   premake5.exe vs2022
   ```

3. **Build the project:**
   - Open `DX11Engine.sln` in Visual Studio
   - Select your desired configuration (Debug/Release)
   - Build the solution (Ctrl+Shift+B)

## Usage

### Running the Sandbox

After a successful build, launch the **Sandbox** project to start experimenting:

1. Navigate to the build output directory
2. Execute the Sandbox application
3. Use the ImGui interface to interact with rendering features

### Experimenting with Shaders

- HLSL shader files are located in the `shaders/` directory
- Modify shader code and rebuild to see changes immediately
- The Sandbox provides real-time visualization of your shader modifications

### Exploring Engine Modules

The engine is organized into modular components:
- **Core** – Base engine functionality and utilities
- **Renderer** – DirectX 11 rendering pipeline
- **Graphics** – Shader management and material systems
- **Assets** – Model and texture loading via Assimp

## Contributing

We welcome contributions! To contribute:

1. **Fork the repository** to your personal GitHub account
2. **Create a feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make your changes** following our coding standards:
   - Follow C++ style guidelines and best practices
   - Write clear, descriptive commit messages
   - Keep commits focused and atomic
4. **Push to your fork** and submit a **Pull Request**
5. Describe your changes in the PR, including motivation and testing details

### Code Standards

- Use clear, descriptive variable and function names
- Keep functions focused and modular
- Comment complex logic and non-obvious design decisions
- Follow existing code style conventions in the repository

## License

This project is licensed under the **BSD-3-Clause License**. See the [LICENSE](LICENSE) file for full details.

---

**Getting Started?** Check out the [Sandbox](src/Sandbox/) directory for example usage and rendering features.

Have questions or found a bug? Open an [Issue](https://github.com/flavian101/DX11Engine/issues) to let us know!