Light Out – ReadMe
Team Clockworkz
All content © 2025 DigiPen (SINGAPORE) Corporation, all rights reserved.


==================================================
1. Game Concept
==================================================

Light Out is a top-down survival puzzle game built on a custom engine developed by Team Clockworkz.  
The player navigates through dark, maze-like environments where light becomes both a resource and a threat.

Core mechanics:
  • The player’s battery level decreases over time; spending too long in darkness is dangerous.
  • Exposure to bright light weakens enemies but overexposure damages the player.
  • Enemies patrol the area and will begin to chase if the player is detected.
  • Walls and structures block both movement and light, introducing tactical routing and stealth.
  • The goal of each level is to navigate the environment, avoid or manipulate enemies through lighting, and reach the exit.

Every enemy and hazard follows predictable logic that can be learned and exploited, encouraging cautious exploration and real-time problem solving rather than pure action.



==================================================
2. Controls / Input Usage (Demo Version)
==================================================

----------- Movement -----------
W / A / S / D ............... Move player
Shift            ............ Run (faster movement)

----------- Interaction / Gameplay -----------
R............................ Flashlight On and Off (Flashlight Drains over Time)
Esc .......................... Pause 


----------- Debug / Developer Tools -----------
Z ...........................  Toggle Level Editor 
Mouse (Level Editor only) .... Object selection and placement


Demo expectations:
  • Player should be able to move around the level.
  • Enemies patrol and begin to chase when player enters detection radius.
  • Touching an enemy results in player death (respawn mechanic triggers).
  • Collision prevents player from walking through solid walls or props.



==================================================
3. Team Roster & Roles
==================================================

Team Clockworkz – CSD2401F25

| Name                                      | Role(s)                          | Leadership / Championing |
|------------------------------------------ |----------------------------------|---------------------------|
| Muhamad Amzar Bin Mohamad Zamre           | Audio & Collision                | Physics / Collision Champion |
| Ching Wei An                              | Graphics / Rendering             | Rendering Champion |
| Amirul Syafiq Bin Mohamad Faizul          | Technical Lead                   | Engine Architecture Lead |
| Koh Teng Meng                             | Artist                           | Visual Asset & Artwork Champion |
| Ho Xin Yu Lebon                           | Project Manager                  | Scheduling / Coordination Lead |
| Yusuf Bin Abdul Rahman Bahajjaj           | Level Editor & Design            | Tools / Usability Champion |

Leadership Summary:
  • Project Leadership – Ho Xin Yu Lebon
  • Technical Leadership – Amirul Syafiq
  • Gameplay / Physics Leadership – Muhamad Amzar
  • Rendering Leadership – Ching Wei An
  • Asset / Visual Champion – Koh Teng Meng
  • Tooling / Level Editor Champion – Yusuf



==================================================
4. Running the Game (Demo Instructions)
==================================================

1. Open the generated Visual Studio 2022 solution (see ProjectGuide.txt for setup).
2. Set the build target to **Debug**.
3. Build and run the project.
4. Playtest the demo level:
   - Move around the environment
   - Avoid enemy patrol routes
   - Use light strategically to manipulate danger and survival
5. Toggle debug/editor tools if needed:
   - Level Editor via F1
   - Debug rendering via F2

For final milestone and performance testing, run in **Release** mode.



==================================================
5. Notes
==================================================

• All gameplay, rendering, physics, audio, serialization, and editor features are created using the custom Clockworkz Engine (no Unity / Unreal / Godot used).  
• Audio and textures used during development are placeholders or DigiPen-approved resources only.  
• The engine and game executable are bundled together; there is no separate editor executable since editing occurs in real time in-engine.


End of ReadMe
