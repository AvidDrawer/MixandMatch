#include <glad/glad.h>
#include <conio.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <object-source-code.h>
#include <shader-source-code.h>
#include <utility>
#include <Shader.h>
#include <vector>
#include <list>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <string>
#include <algorithm>
#include <separateRenderer.h>
#include <thread>
#include <glm/gtx/string_cast.hpp>


int w_win = 800;
int h_win = 600;

constexpr float explodeTime = 8.0f;
constexpr float multiplier = 5;
constexpr float radius = 0.4f * multiplier;

glm::mat4 model = glm::mat4(1.0f);
glm::mat4 view = glm::mat4(1.0f);
glm::mat4 projection = glm::mat4(1.0f);

namespace camera
{
    // Chosen coordinate system
    // +y
    // |   -z  
    // |  /
    // | /
    // O _ _ _ _ +x
    
    // yaw is initialized to -90.0 degrees so that we start by looking towards the -z axis
    float yaw = 90.0f;	 // rotate about y
    float pitch = 0.0f;  // rotate about x

    glm::vec3 playerPosition(0.0f, 40.0f, -265.0f); // same as camera position
    glm::vec3 frontVector(0.0f, 0.0f, 1.0f);
    glm::vec3 upVector(0.0f, 1.0f, 0.0f);
    glm::vec3 rightVector = glm::normalize(glm::cross(-frontVector, upVector));
}

namespace myMouse
{
    float posx = 400.0f;
    float posy = 300.0f;

    bool pointerReset = true;   // set mouse pointer location to the current location
    float speed = 50.0f;        // mouse sensitivity

    bool cameraMoveMode = false;      // tracks mouse button click
    
    bool pickedObjectMoveMode = false; // is object picked?
    int pickedObjectIndex = -1;         // index of picked object
    glm::vec3 pickedObjectOffset = glm::vec3(0); // if object picked, how much did mouse move it?

    bool leftMouseClick = false;
    bool leftMouseRelease = false;
    glm::vec3 mouseDir = glm::vec3(0);
}



std::vector<glm::vec3> translations(2500);
std::vector<int> discardIndices(2500,2);
std::list<int> gravityIndices;
std::vector<short> movableObject(2500, 1);
std::vector<glm::vec3> explosionPositions;
std::vector<float> explosionStartTimes;

void processInput(GLFWwindow* window, float deltaTime);
void mouse_move_callback(GLFWwindow* window, double posx, double posy);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);


void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    
    w_win = width;
    h_win = height;
}

unsigned int loadCubemap(std::vector<std::string> faces);

void threadfun(GLFWwindow* window)
{
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
    }

    for (int i = 0; i < separateRenderer::updatelist.size(); ++i)
    {
        separateRenderer::renderlist.push_back(separateRenderer::mycreateBuffers(separateRenderer::updatelist[i]));
    }

    // load textures
    // -------------
    std::vector<std::string> faces
    {
        "Skybox//right.jpg",
        "Skybox//left.jpg",
        "Skybox//top.jpg",
        "Skybox//bottom.jpg",
        "Skybox//front.jpg",
        "Skybox//back.jpg",
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    Shader shapeRenderer(separateRenderer::updatelist[0].shaderListing[0].c_str(), 
                            separateRenderer::updatelist[0].shaderListing[1].c_str());   // to render objects
    
    Shader explosionRenderer(separateRenderer::updatelist[1].shaderListing[0].c_str(), 
                                separateRenderer::updatelist[1].shaderListing[1].c_str(),
                                    separateRenderer::updatelist[1].shaderListing[2].c_str());   // oversee destruction ;)
                                    
    Shader environRenderer(separateRenderer::updatelist[2].shaderListing[0].c_str(), 
                                separateRenderer::updatelist[2].shaderListing[1].c_str()); // to render the Cube map

    glEnable(GL_DEPTH_TEST);
    while (!separateRenderer::stop.load())
    {
        long long thisval = separateRenderer::myptr.load();
        long long otherval = separateRenderer::otherptr.load();
        if (thisval - otherval > 0)
        {
            separateRenderer::otherptr.fetch_add(1);
            glClearColor(0.82f, 0.82f, 0.82f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            std::vector<separateRenderer::bufferData>& currFrameData = thisval % 2 ? separateRenderer::updatelist : separateRenderer::updatelist2;
            std::vector<separateRenderer::bufferData>& prevFrameData = thisval % 2 ? separateRenderer::updatelist2 : separateRenderer::updatelist;

            //update all uniforms
            shapeRenderer.use();
            for (auto mypair : currFrameData[0].uniformListing)
            {
                switch (currFrameData[0].uniformValues[mypair.second].index())
                {
                case 0:shapeRenderer.setInt(mypair.first, std::get<int>(currFrameData[0].uniformValues[mypair.second])); break;
                case 1:shapeRenderer.setFloat(mypair.first, std::get<float>(currFrameData[0].uniformValues[mypair.second])); break;
                case 2:shapeRenderer.setVec3(mypair.first, std::get<glm::vec3>(currFrameData[0].uniformValues[mypair.second])); break;
                case 3:shapeRenderer.setMat4(mypair.first, std::get<glm::mat4>(currFrameData[0].uniformValues[mypair.second])); break;
                }
            }

            // update buffer data 
            // ideally, all updates to a buffer should be done first before moving to next to minimize state changes
            // to-do
            for (int i = 0; i < currFrameData[0].listofbufferchanges.size(); ++i)
            {
                int ind = currFrameData[0].listofbufferchanges[i][0];
                void* firstelemptr = std::visit(separateRenderer::getElementPointerVisitor{}, currFrameData[0].allData[ind+1]);
                int elemsize = std::visit(separateRenderer::getElementSizeVisitor{}, currFrameData[0].allData[ind+1]);
                int offset = currFrameData[0].listofbufferchanges[i][1];
                int length = currFrameData[0].listofbufferchanges[i][2];

                int startlocind = currFrameData[0].bufferlocations[i];
                char* myptr = (char*)firstelemptr + elemsize* startlocind;
                int vboi = separateRenderer::renderlist[0].vbo[ind];

                glBindBuffer(GL_ARRAY_BUFFER, vboi);
                glBufferSubData(GL_ARRAY_BUFFER, offset, length, (void*)myptr);
            }
            

            // draw
            glBindVertexArray(separateRenderer::renderlist[0].vao);            
            if (currFrameData[0].instanced)
            {
                if (currFrameData[0].drawarrays)
                    glDrawArraysInstanced(currFrameData[0].primitiveType, 0, currFrameData[0].vertexIndexCount, currFrameData[0].instanceCount);
                else
                    glDrawElementsInstanced(currFrameData[0].primitiveType, (unsigned int)currFrameData[0].vertexIndexCount, GL_UNSIGNED_INT, 0, currFrameData[0].instanceCount);
            }



            explosionRenderer.use();
            for (auto mypair : currFrameData[1].uniformListing)
            {
                switch (currFrameData[1].uniformValues[mypair.second].index())
                {
                case 0:explosionRenderer.setInt(mypair.first, std::get<int>(currFrameData[1].uniformValues[mypair.second])); break;
                case 1:explosionRenderer.setFloat(mypair.first, std::get<float>(currFrameData[1].uniformValues[mypair.second])); break;
                case 2:explosionRenderer.setVec3(mypair.first, std::get<glm::vec3>(currFrameData[1].uniformValues[mypair.second])); break;
                case 3:explosionRenderer.setMat4(mypair.first, std::get<glm::mat4>(currFrameData[1].uniformValues[mypair.second])); break;
                }
            }

            for (int i = 0; i < currFrameData[1].listofbufferchanges.size(); ++i)
            {
                int ind = currFrameData[1].listofbufferchanges[i][0];
                void* firstelemptr = std::visit(separateRenderer::getElementPointerVisitor{}, currFrameData[1].allData[ind + 1]);
                int elemsize = std::visit(separateRenderer::getElementSizeVisitor{}, currFrameData[1].allData[ind + 1]);
                int offset = currFrameData[1].listofbufferchanges[i][1];
                int length = currFrameData[1].listofbufferchanges[i][2];

                int startlocind = currFrameData[1].bufferlocations[i];
                char* myptr = (char*)firstelemptr + elemsize * startlocind;
                int vboi = separateRenderer::renderlist[1].vbo[ind];

                glBindBuffer(GL_ARRAY_BUFFER, vboi);
                glBufferSubData(GL_ARRAY_BUFFER, offset, length, (void*)myptr);
            }

            glBindVertexArray(separateRenderer::renderlist[1].vao);
            if (currFrameData[1].instanced)
            {
                if (currFrameData[1].drawarrays)
                    glDrawArraysInstanced(currFrameData[1].primitiveType, 0, currFrameData[1].vertexIndexCount, currFrameData[1].instanceCount);
                else
                {
                    if (currFrameData[1].instanceCount)
                    {
                        glDrawElementsInstanced(currFrameData[1].primitiveType, (unsigned int)currFrameData[1].vertexIndexCount, GL_UNSIGNED_INT, 0, currFrameData[1].instanceCount);

                        //std::cout << "\tinstance count:" << currFrameData[1].instanceCount;
                    }
                }
            }




            environRenderer.use();
            for (auto mypair : currFrameData[2].uniformListing)
            {
                switch (currFrameData[2].uniformValues[mypair.second].index())
                {
                case 0:environRenderer.setInt(mypair.first, std::get<int>(currFrameData[2].uniformValues[mypair.second])); break;
                case 1:environRenderer.setFloat(mypair.first, std::get<float>(currFrameData[2].uniformValues[mypair.second])); break;
                case 2:environRenderer.setVec3(mypair.first, std::get<glm::vec3>(currFrameData[2].uniformValues[mypair.second])); break;
                case 3:environRenderer.setMat4(mypair.first, std::get<glm::mat4>(currFrameData[2].uniformValues[mypair.second])); break;
                }
            }

            glBindVertexArray(separateRenderer::renderlist[2].vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
            glDepthFunc(GL_LEQUAL);
            if (currFrameData[2].instanced)
            {
                if (currFrameData[2].drawarrays)
                    glDrawArraysInstanced(currFrameData[2].primitiveType, 0, currFrameData[2].vertexIndexCount, currFrameData[2].instanceCount);
                else
                    glDrawElementsInstanced(currFrameData[2].primitiveType, (unsigned int)currFrameData[2].vertexIndexCount, GL_UNSIGNED_INT, 0, currFrameData[2].instanceCount);
            }
            else
            {
                glDrawArrays(currFrameData[2].primitiveType, 0, currFrameData[2].vertexIndexCount);
            }
            glDepthFunc(GL_LESS);

            //currFrameData[0];
            glfwSwapBuffers(window);
        }        
    }
}


int main()
{   
    glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(w_win, h_win, "Falling spheres", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Window creation failed" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(NULL);
	glfwSetFramebufferSizeCallback(window, window_size_callback);
    glfwSetCursorPosCallback(window, mouse_move_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);


	int latitude=20, longitude=20;
	std::pair<std::vector<float>,std::vector<int>> sphereRenderData;
    sphereRenderData = objectDictionary::constructSphere(radius, latitude, longitude);

    int index = 0;
    for (float y = -25 * multiplier; y < 25 * multiplier; y += 1 * multiplier)
    {
        for (float x = -25 * multiplier; x < 25 * multiplier; x += 1 * multiplier)
        {
            glm::vec3 translation;
            translation.x = x;
            translation.y = 50;
            translation.z = y;
            translations[index] = translation;
            ++index;
        }
    }
    explosionStartTimes = std::vector<float>(translations.size(), -1.0f);
    explosionPositions = translations;


    separateRenderer::bufferData object;
    object.allData.push_back(sphereRenderData.second);
    object.allData.push_back(sphereRenderData.first);
    object.allData.push_back(translations);
    object.attributeCount = { 2,1 };
    object.attributeSize = { 3,3,3 };
    object.totalAttributeCount = { 6,1 };
    object.shaderListing = { primaryshader::vs, primaryshader::fs };
    object.addUniform("model");
    object.addUniform("view");
    object.addUniform("projection");
    object.addUniform("linemode");
    object.addUniform("skybox");
    object.addUniform("cameraPos");
    object.updateUniform("skybox", 0);
    object.instancedBufferIndex = { 0,1 };
    object.primitiveType = GL_TRIANGLES;
    object.drawarrays = false;
    object.vertexIndexCount = sphereRenderData.second.size();
    object.instanceCount = 2500;
    object.instanced = true;

    separateRenderer::bufferData exploder;
    exploder.allData.push_back(sphereRenderData.second);
    exploder.allData.push_back(sphereRenderData.first);
    exploder.allData.push_back(explosionPositions);
    exploder.allData.push_back(explosionStartTimes);
    exploder.attributeCount = { 2,1,1 };
    exploder.attributeSize = { 3,3,3,1 };
    exploder.totalAttributeCount = { 6,1,1 };
    exploder.shaderListing = { primaryshader::vs, primaryshader::fs, primaryshader::gs };
    exploder.addUniform("model");
    exploder.addUniform("view");
    exploder.addUniform("projection");
    exploder.addUniform("skybox");
    exploder.addUniform("cameraPos");
    exploder.addUniform("currentTime");
    exploder.addUniform("lifetime");
    exploder.updateUniform("skybox", 0);
    exploder.updateUniform("lifetime", explodeTime);
    exploder.instancedBufferIndex = { 0,1,1 };
    exploder.primitiveType = GL_TRIANGLES;
    exploder.drawarrays = false;
    exploder.vertexIndexCount = sphereRenderData.second.size();
    exploder.instanceCount = 0;
    exploder.instanced = true;

    std::vector<float> skyboxVertices = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    separateRenderer::bufferData sky;
    sky.allData.push_back(std::vector<int>());  //no indices
    sky.allData.push_back(skyboxVertices);
    sky.attributeCount = { 1 };
    sky.attributeSize = { 3 };
    sky.totalAttributeCount = { 3 };
    sky.shaderListing = { cubemapshader::vs, cubemapshader::fs};
    sky.addUniform("view");
    sky.addUniform("projection");
    sky.addUniform("skybox");
    sky.updateUniform("skybox", 0);
    sky.primitiveType = GL_TRIANGLES;
    sky.drawarrays = true;
    sky.vertexIndexCount = 36; // = skyboxVertices.size() ?
    sky.instanceCount = 0;
    sky.instancedBufferIndex = { 0 };
    sky.instanced = false;

    separateRenderer::updatelist.push_back(object);
    separateRenderer::updatelist.push_back(exploder);
    separateRenderer::updatelist.push_back(sky);
    separateRenderer::updatelist2 = separateRenderer::updatelist;


    std::thread th1(threadfun, window);
    
    float fov = 45.0f;
    int numFrames = 0; float startTime = 0;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    int finalSwapIndex = 2499;
    int explodeIndex = 0;

    // is event polling asynchronous? Does it occupy the current render time or the next frame's render time?
    // and hence, should we poll at the start of the frame or at the end??
    model = glm::mat4(1.0f);
    while (!glfwWindowShouldClose(window))
    {
        long long thisval = separateRenderer::myptr.load();
        long long otherval = separateRenderer::otherptr.load();
        if (thisval - otherval == 0)
        {
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            ++numFrames;
            if (currentFrame - startTime >= 1.0f)
            {
                std::cout << "\nAverage render time: " << (1000.0f / numFrames) << " ms";
                std::cout << "\t\tFrames per second: " << numFrames;
                numFrames = 0;
                startTime = currentFrame;
            }
            
            processInput(window, deltaTime);

            view = glm::lookAt(camera::playerPosition, camera::playerPosition + camera::frontVector, camera::upVector);
            projection = glm::perspective(glm::radians(fov), w_win * 1.0f / h_win, 0.1f, 2000.0f);

            std::vector<separateRenderer::bufferData>& currFrameData = thisval % 2? separateRenderer::updatelist2:separateRenderer::updatelist;
            std::vector<separateRenderer::bufferData>& prevFrameData = thisval % 2 ? separateRenderer::updatelist : separateRenderer::updatelist2;
            //std::vector<separateRenderer::bufferData>& prevFrameData = separateRenderer::updatelist2;

            auto& currFrameObjectOffsets = std::get<std::vector<glm::vec3>>(currFrameData[0].allData[2]);
            auto& prevFrameObjectOffsets = std::get<std::vector<glm::vec3>>(prevFrameData[0].allData[2]);

            auto& currFrameExplodeOffsets = std::get<std::vector<glm::vec3>>(currFrameData[1].allData[2]);
            auto& currFrameExplodeTimes = std::get<std::vector<float>>(currFrameData[1].allData[3]);

            // SOURCE OF ERROR. THE BUFFER UPDATES ARE NOT CARRYING THROUGH CONSISTENTLY FROM THE FIRST FRAME OBJECT TO THE SECOND
            // AND THEN BACK TO THE FIRST AND ......
            //make buffer updates from previous frame
            for (int i = 0; i < prevFrameData.size(); ++i)
            {
                for (int j = 0; j < prevFrameData[i].listofbufferchanges.size(); ++j)
                {
                    int ind = prevFrameData[i].listofbufferchanges[j][0]+1;
                    int ind2 = prevFrameData[i].bufferlocations[j];
                    switch (prevFrameData[i].allData[ind].index())
                    {
                    case 0:break;
                    case 1:std::get<std::vector<float>>(currFrameData[i].allData[ind])[ind2] = std::get<std::vector<float>>(prevFrameData[i].allData[ind])[ind2]; break;
                    case 2:std::get<std::vector<glm::vec3>>(currFrameData[i].allData[ind])[ind2] = std::get<std::vector<glm::vec3>>(prevFrameData[i].allData[ind])[ind2]; break;
                    }
                }
                currFrameData[i].instanceCount = prevFrameData[i].instanceCount;
            }

            // reset current frame updates
            for (int i = 0; i < currFrameData.size(); ++i)
            {
                currFrameData[i].listofbufferchanges.clear();
                currFrameData[i].bufferlocations.clear();
            }

#if 1
            if (myMouse::leftMouseClick)
            {
                myMouse::leftMouseClick = false;
                std::pair<int, float> minval = { 0,1000000000.0f };
                bool chk = false;
                for (int i = 0; i < 2500; ++i)
                {
                    if (!discardIndices[i])
                        continue;
                    glm::vec3 diff = camera::playerPosition - currFrameObjectOffsets[i];

                    float b = glm::dot(myMouse::mouseDir, diff);
                    float c = glm::dot(diff, diff) - radius * radius;
                    if (b * b - c < 0)
                    {

                    }
                    else
                    {
                        chk = true;
                        float dist = glm::dot(diff, diff);
                        if (dist < minval.second)
                        {
                            minval.second = dist;
                            minval.first = i;
                        }
                    }
                }
                if (chk)
                {
                    myMouse::pickedObjectMoveMode = true;
                    myMouse::pickedObjectIndex = minval.first;
                    myMouse::cameraMoveMode = true;
                    myMouse::pointerReset = true;
                }
            }
            if(myMouse::leftMouseRelease)
            {
                myMouse::leftMouseRelease = false;
                if (myMouse::pickedObjectIndex != -1)
                {
                    if (movableObject[myMouse::pickedObjectIndex])  // to prevent 2x 'gravity' effect. hack
                    {
                        gravityIndices.push_back(myMouse::pickedObjectIndex);
                        movableObject[myMouse::pickedObjectIndex] = false;
                    }
                    myMouse::pickedObjectIndex = -1;
                }
            }
            if (myMouse::pickedObjectIndex != -1)
            {
                // naive collision check
                for (int i = 0; i < 2500; ++i)
                {
                    if (i != myMouse::pickedObjectIndex && discardIndices[i])
                    {
                        glm::vec3 diff = translations[i] - (myMouse::pickedObjectOffset + currFrameObjectOffsets[myMouse::pickedObjectIndex]);
                        if (glm::dot(diff, diff) < 4 * radius * radius)
                        {
                            myMouse::pickedObjectOffset = glm::vec3(0);
                            break;
                        }
                    }
                }
                currFrameObjectOffsets[myMouse::pickedObjectIndex] += myMouse::pickedObjectOffset;

                //std::cout << glm::to_string(myMouse::pickedObjectOffset);
                currFrameData[0].listofbufferchanges.push_back({ 1,(int)myMouse::pickedObjectIndex * (int)sizeof(glm::vec3),sizeof(glm::vec3) });
                currFrameData[0].bufferlocations.push_back(myMouse::pickedObjectIndex);
            }
#endif
            if (gravityIndices.size())
            {
                glm::vec3 disp = { 0,-10,0 };

                std::list<int>::iterator gravityIterator = gravityIndices.begin();
                while (gravityIterator != gravityIndices.end())
                {
                    if (*gravityIterator == myMouse::pickedObjectIndex)
                    {
                        ++gravityIterator;
                        continue;
                    }

                    currFrameObjectOffsets[*gravityIterator] = prevFrameObjectOffsets[*gravityIterator] + disp*deltaTime;
                    bool remove = false;
                    if (currFrameObjectOffsets[*gravityIterator][1] < 0)
                    {
                        currFrameObjectOffsets[*gravityIterator][1] = 0;
                        remove = true;
                    }
                    
                    currFrameData[0].listofbufferchanges.push_back({ 1,(int)*gravityIterator * (int)sizeof(glm::vec3),sizeof(glm::vec3) });
                    currFrameData[0].bufferlocations.push_back(*gravityIterator);
                    if (remove)
                    {
                        std::swap(currFrameObjectOffsets[finalSwapIndex], currFrameObjectOffsets[*gravityIterator]);
                        discardIndices[finalSwapIndex] = 0;

                        currFrameData[0].listofbufferchanges.push_back({ 1,(int)*gravityIterator * (int)sizeof(glm::vec3),sizeof(glm::vec3) });
                        currFrameData[0].bufferlocations.push_back(*gravityIterator);
                        
                        currFrameData[0].listofbufferchanges.push_back({ 1,(int)finalSwapIndex * (int)sizeof(glm::vec3),sizeof(glm::vec3) });
                        currFrameData[0].bufferlocations.push_back(finalSwapIndex);

                        currFrameExplodeOffsets[explodeIndex] = currFrameObjectOffsets[finalSwapIndex];
                        currFrameExplodeTimes[explodeIndex] = currentFrame;

                        currFrameData[1].listofbufferchanges.push_back({ 1,(int)explodeIndex * (int)sizeof(glm::vec3),sizeof(glm::vec3) });
                        currFrameData[1].bufferlocations.push_back(explodeIndex);
                        
                        currFrameData[1].listofbufferchanges.push_back({ 2,(int)explodeIndex * (int)sizeof(float),sizeof(float) });
                        currFrameData[1].bufferlocations.push_back(explodeIndex);
                        
                        ++explodeIndex;

                        --finalSwapIndex;
                        currFrameData[0].instanceCount = finalSwapIndex+1;
                        currFrameData[1].instanceCount = explodeIndex;
                        gravityIterator = gravityIndices.erase(gravityIterator);
                    }
                    else
                        ++gravityIterator;
                }
            }

            int numRemoved = 0;
            for (int i = 0; i < explodeIndex; ++i)
            {
                if (currFrameExplodeTimes[i] > 0 && currentFrame - currFrameExplodeTimes[i] > explodeTime)
                    ++numRemoved;
                else
                    break;
            }
            if (numRemoved)
            {
                std::copy(currFrameExplodeOffsets.begin() + numRemoved, currFrameExplodeOffsets.begin() + explodeIndex, currFrameExplodeOffsets.begin());
                std::copy(currFrameExplodeTimes.begin() + numRemoved, currFrameExplodeTimes.begin() + explodeIndex, currFrameExplodeTimes.begin());

                explodeIndex -= numRemoved;
                currFrameData[1].instanceCount = explodeIndex;

                if (explodeIndex)
                {
                    currFrameData[1].listofbufferchanges.push_back({ 1,0,explodeIndex * (int)sizeof(glm::vec3) });
                    currFrameData[1].bufferlocations.push_back(0);
                    currFrameData[1].listofbufferchanges.push_back({ 2,0,explodeIndex * (int)sizeof(float) });
                    currFrameData[1].bufferlocations.push_back(0);
                }
            }

            currFrameData[0].updateUniform("model", model);
            currFrameData[0].updateUniform("view", view);
            currFrameData[0].updateUniform("projection", projection);
            currFrameData[0].updateUniform("cameraPos", camera::playerPosition);

            currFrameData[1].updateUniform("model", model);
            currFrameData[1].updateUniform("view", view);
            currFrameData[1].updateUniform("projection", projection);
            currFrameData[1].updateUniform("cameraPos", camera::playerPosition);
            currFrameData[1].updateUniform("currentTime", currentFrame);

            glm::mat4 newView = glm::mat4(glm::mat3(view));
            currFrameData[2].updateUniform("view", newView);
            currFrameData[2].updateUniform("projection", projection);
            
            separateRenderer::myptr.fetch_add(1);
        }
        glfwPollEvents();
    }

    th1.join();
    glfwTerminate();

    std::cout << "\nPress any key to quit";
    _getch();
	return 0;
}

void processInput(GLFWwindow* window, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        separateRenderer::stop.store(true);
        glfwSetWindowShouldClose(window, true);
    }

    float cameraSpeed = myMouse::speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera::playerPosition += cameraSpeed * camera::frontVector; 
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera::playerPosition -= cameraSpeed * camera::frontVector;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera::playerPosition -= glm::normalize(glm::cross(camera::frontVector, camera::upVector)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera::playerPosition += glm::normalize(glm::cross(camera::frontVector, camera::upVector)) * cameraSpeed;
}

void mouse_move_callback(GLFWwindow* window, double posx, double posy)
{
    if (!myMouse::cameraMoveMode)
        return;

    if (myMouse::pointerReset)
    {
        myMouse::pointerReset = false;
        myMouse::posx = posx;
        myMouse::posy = posy;
    }

    float offsetx, offsety;

    offsetx = posx - myMouse::posx;
    offsety = posy - myMouse::posy;

    
    if (myMouse::pickedObjectMoveMode)
    {
        //move the sphere. check for collisions first. if colliding, don't update mouse.
        bool collision = false;
        float multiplier = 0.03f;
        
        glm::vec3 newup = glm::normalize(glm::cross(camera::frontVector, camera::rightVector));
        myMouse::pickedObjectOffset = -glm::vec3(multiplier) * (camera::rightVector * offsetx * multiplier + newup * offsety* multiplier);
        return;
    }
    
    myMouse::posx = posx;
    myMouse::posy = posy;

    float sensitivity = 0.05f;
    offsetx *= sensitivity;
    offsety *= sensitivity;

    camera::yaw += offsetx;
    camera::pitch -= offsety;

    if (camera::pitch > 89.0f)
        camera::pitch = 89.0f;
    if (camera::pitch < -89.0f)
        camera::pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(camera::yaw)) * cos(glm::radians(camera::pitch));
    direction.y = sin(glm::radians(camera::pitch));
    direction.z = sin(glm::radians(camera::yaw)) * cos(glm::radians(camera::pitch));
    camera::frontVector = glm::normalize(direction);
    camera::rightVector = glm::normalize(glm::cross(-camera::frontVector, camera::upVector));
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        double xpos, ypos;
        //getting cursor position
        if (action == GLFW_PRESS)
        {
            glfwGetCursorPos(window, &xpos, &ypos);
     
            float x = (2.0f * xpos) / w_win - 1.0f;
            float y = 1.0f - (2.0f * ypos) / h_win;
            float z = 1.0f;
            glm::vec3 ray_nds = glm::vec3(x, y, z);
            glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0, 1.0);
            glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
            ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
            glm::vec3 dir = (glm::inverse(view) * ray_eye);
            dir = glm::normalize(dir);   

            myMouse::mouseDir = dir;
            myMouse::leftMouseClick = true;
        }
        else if (action == GLFW_RELEASE)
        {
            myMouse::pickedObjectMoveMode = false;
            myMouse::cameraMoveMode = false;
            myMouse::leftMouseRelease = true;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            myMouse::cameraMoveMode = true;
            myMouse::pointerReset = true;
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            myMouse::cameraMoveMode = false;
        }
    }
}


unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}