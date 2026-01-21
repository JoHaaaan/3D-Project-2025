CONTROLS:
WASD/QE - Camera movement
MOUSE   - Camera orientation
1       - Toggle albedo only
2       - Toggle diffuse lighting
3       - Toggle specular lighting
4       - Toggle wireframe mode
5       - Toggle tessellation
6       - Toggle Smaller frustum (for quadtree frustum culling)
9       - Toggle billboarded particle system
ESC     - Exit

Techniques showcase
1. Deferred Rendering
Press "1" to see albedo only
Press "2" and "3" to confirm the light pass works

2. Shadow Mapping
Observe if shadows are correct when all lighting toggles are on.

3. Level of Detail (LOD) using Tessellation
Press "4" to toggle wireframe mode. Press "5" to toggle tessellation on/off and observe the objects at different distances.   

4. OBJ parser
There are objects in the scene. 

5. Dynamic Cube Environment Mapping
There is a reflective sphere at (4, 2, -2)

6. Frustum Culling using Quadtree
Press "6" to reduce frustum size for the frustum culling

7. GPU-based Billboarded Particle System
Press "9" to toggle the emission of particles

8. Normal Mapping
Locate the rotating cube at position (5, 2, 0). 

9. Parallax Occlusion Mapping
Locate the rotating cube at position (-1, 2, 0).

Compilation Instructions:
Open RasterizerDemo.sln
Build all
Go to \3D-Project-2025\3DProject\RasterizerDemo\x64\Debug
Drag all .cso files to \3D-Project-2025\3DProject\RasterizerDemo\RasterizerDemo
Run project

