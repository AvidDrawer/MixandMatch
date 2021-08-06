This project is intented as an implementation of a couple of different methods.

The program renders 2500 spheres in a grid pattern and allows for mouse picking of the the spheres. The sphere is considered to be 'picked' as long as the left mouse button is clicked on it. Once unclicked, the sphere starts to fall to the ground at a steady rate. 

To navigate, use W-A-S-D. To change orientation, right click + move mouse.

The movement sensitivity during pick is set to be very low as there seems to be no direct correlation between mouse movement and that of picked object. 

The spheres show limited collision characteristics- once two spheres are in contact, both come to rest. 

Each sphere is also given a mirror finish with respect to the skybox. 

To change the background, change the images inside the Skybox folder.

To build, run CMake on the source directory- preferably into a build directory.
Open the corresponding visual studio solution and build the project. Then build the Install project. This will create a bin folder that will contain the executable. 


Plans-
1. Use a bounding volume hierarchy for object picking and collision detection [NEXT]
2. Implement multiple reflections
3. Allow different shapes- spheres, pyramids, cuboids etc. inheritance vs templating. Perhaps assimp based model loading?!
4. decoupling the render thread from the calculations  [separated-render-thread branch]
5. ray tracing (?)