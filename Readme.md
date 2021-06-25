Introduction
This project includes a couple of different methods as a form of demonstration. 

The program renders 2500 spheres in a grid pattern and allows for mouse picking of the the spheres. The sphere is considered to be 'picked' as long as the left mouse is button is clicked on it. Once unclicked, the sphere starts to fall to the ground at a steady rate. 

To navigate, use W-A-S-D. To change orientation, right click + move mouse.

The movement sensitivity during pick is set to be very low as correlation between mouse movement and that of the object do not appear to be directly related. 

The spheres also show collision characteristics- to the extent that once two spheres are in contact, both come to rest. 

Each sphere is also given a mirror finish with respect to the skybox. 

To change the background, change the images inside the Skybox folder.

Building
To build, run CMake on the source directory- preferably into a build directory.
Open the corresponding visual studio solution and build the project. Then build the Install project. This will create a bin folder that will contain the executable. 