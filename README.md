# HipotesisParty
Juego para actividad de retroalimentación del curso CI-0115
 
## 📁 Project Structure

```text
HypothesisParty/
│
├── CMakeLists.txt               # Main CMake configuration
│
├── core/                        # C++ Game Logic Engine
│   ├── GameEngine.cpp           # Main game logic
│   ├── GameEngine.hpp
│   ├── Player.cpp               # Player management
│   └── Player.hpp
│
├── game-server/                 # Authoritative Game Server (C++)
│   ├── main.cpp                 # Server entry point
│   └── GameServer.cpp           # Game state and communication
│
├── game-client/                 # Raylib Game Client (C++)
│   ├── main.cpp                 # Client entry point
│   ├── Board.cpp                # Game board
│   └── Renderer.cpp             # Graphics and animations
│
├── api/                         # ASP.NET Core Web API (C#)
│   ├── Program.cs               # API entry point
│   ├── GameAPI.csproj            # .NET project configuration
│   ├── Controllers/              # REST API endpoints
│   └── Services/                 # API services
│
└── web/                         # Mobile Web Client
    ├── index.html               # Player interface
    └── app.js                   # JavaScript client logic
```