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

int w_win = 800;
int h_win = 600;

constexpr float explodeTime = 4.0f;
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

    bool moveMode = false;      // tracks mouse button click
    
    bool pickedObjectMoveMode = false; // is object picked?
    int pickedObjectIndex = 0;         // index of picked object
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
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
    glfwSetFramebufferSizeCallback(window, window_size_callback);
    glfwSetCursorPosCallback(window, mouse_move_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);


	int latitude=20, longitude=20;
	std::pair<std::vector<float>,std::vector<int>> sphereRenderData;
    sphereRenderData = objectDictionary::constructSphere(radius, latitude, longitude);

    // Shaders 
    Shader shapeRenderer(primaryshader::vs, primaryshader::fs, primaryshader::gs);   // to render objects and oversee destruction ;)
    Shader environRenderer(cubemapshader::vs, cubemapshader::fs); // to render the Cube map
    
    float skyboxVertices[] = {
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

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

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
    
    environRenderer.use();
    environRenderer.setInt("skybox", 0);

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(GL_ARRAY_BUFFER, sphereRenderData.first.size()*sizeof(float), &sphereRenderData.first[0], GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereRenderData.second.size() * sizeof(int), &sphereRenderData.second[0], GL_STATIC_DRAW);

    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GL_FLOAT), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GL_FLOAT), (void*)(3 * sizeof(GL_FLOAT)));
    glEnableVertexAttribArray(1);
    

    int index = 0; 
    for (float y = -25* multiplier; y < 25 * multiplier; y += 1 * multiplier)
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

    unsigned int instanceVBO, rendercheckVBO, explosionTimeVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 2500, &translations[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glVertexAttribDivisor(2, 1);

    glGenBuffers(1, &rendercheckVBO);
    glBindBuffer(GL_ARRAY_BUFFER, rendercheckVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 2500, &discardIndices[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GL_INT), (void*)0);
    glVertexAttribDivisor(3, 1);

    glGenBuffers(1, &explosionTimeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, explosionTimeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT)* explosionStartTimes.size(), &explosionStartTimes[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT), (void*)0);
    glVertexAttribDivisor(4, 1);


    glEnable(GL_DEPTH_TEST);

    float fov = 45.0f;
    
    shapeRenderer.use();
    shapeRenderer.setInt("skybox", 0);
    shapeRenderer.setFloat("lifetime", explodeTime);

    int numFrames = 0; float startTime = 0;
    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    int lastInstanceIndex = 2499;
    int finalSwapIndex = 2499;

    model = glm::mat4(1.0f);
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        ++numFrames;
        if (currentFrame - startTime >= 1.0f)
        {
            std::cout << "\nAverage render time: " << (1000.0f/numFrames) << " ms";
            std::cout << "\t\tFrames per second: " << numFrames;
            numFrames = 0;
            startTime = currentFrame;
        }
        view = glm::lookAt(camera::playerPosition, camera::playerPosition + camera::frontVector, camera::upVector);
        projection = glm::perspective(glm::radians(fov), w_win * 1.0f / h_win, 0.1f, 2000.0f);
        
        processInput(window, deltaTime);

        //glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 1
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        
        if(myMouse::pickedObjectIndex != -1)
        {
            glBufferSubData(GL_ARRAY_BUFFER, myMouse::pickedObjectIndex * sizeof(glm::vec3), sizeof(glm::vec3), glm::value_ptr(translations[myMouse::pickedObjectIndex]));
        }
        
        bool explosionAdded = false;
        if (gravityIndices.size())
        {
            glm::vec3 disp = {0,-10,0};

            std::list<int>::iterator gravityIterator = gravityIndices.begin();
            while (gravityIterator != gravityIndices.end())
            {
                if (*gravityIterator == myMouse::pickedObjectIndex)
                {
                    ++gravityIterator;
                    continue;
                }

                translations[*gravityIterator] += disp * deltaTime;
                bool remove = false;
                if (translations[*gravityIterator][1] < 0)
                {
                    translations[*gravityIterator][1] = 0;
                    remove = true;
                }
                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, *gravityIterator * sizeof(glm::vec3), sizeof(glm::vec3), glm::value_ptr(translations[*gravityIterator]));

                if (remove)
                {
                    std::swap(translations[finalSwapIndex], translations[*gravityIterator]);
                    discardIndices[finalSwapIndex] = 0;
                    explosionStartTimes[finalSwapIndex] = currentFrame;

                    glBufferSubData(GL_ARRAY_BUFFER, finalSwapIndex * sizeof(glm::vec3), sizeof(glm::vec3), glm::value_ptr(translations[finalSwapIndex]));
                    glBufferSubData(GL_ARRAY_BUFFER, *gravityIterator * sizeof(glm::vec3), sizeof(glm::vec3), glm::value_ptr(translations[*gravityIterator]));

                    glBindBuffer(GL_ARRAY_BUFFER, explosionTimeVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, finalSwapIndex * sizeof(float), sizeof(float), &explosionStartTimes[finalSwapIndex]);

                    glBindBuffer(GL_ARRAY_BUFFER, rendercheckVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, finalSwapIndex * sizeof(int), sizeof(int), &discardIndices[finalSwapIndex]);
                    --finalSwapIndex;
                    gravityIterator = gravityIndices.erase(gravityIterator);
                    explosionAdded = true;
                }
                else
                    ++gravityIterator;
            }
        }

        int numRemoved = 0;
        for (int i = lastInstanceIndex; i >= finalSwapIndex; --i)
        {
            if (explosionStartTimes[i] > 0 && currentFrame - explosionStartTimes[i] > explodeTime)
                ++numRemoved;
            else
                break;
        }
        lastInstanceIndex -= numRemoved;        
                
        shapeRenderer.use();
        shapeRenderer.setMat4("model", model);
        shapeRenderer.setMat4("view", view);
        shapeRenderer.setMat4("projection", projection);
        shapeRenderer.setVec3("cameraPos", camera::playerPosition);
        shapeRenderer.setFloat("currentTime", currentFrame);
        
        glBindVertexArray(VAO);
        
        shapeRenderer.setInt("linemode", 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElementsInstanced(GL_TRIANGLES, (unsigned int)sphereRenderData.second.size(), GL_UNSIGNED_INT, 0, 2);

        shapeRenderer.setInt("linemode", 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElementsInstanced(GL_TRIANGLES, (unsigned int)sphereRenderData.second.size(), GL_UNSIGNED_INT, 0, lastInstanceIndex + 1);
        
#endif

        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthFunc(GL_LEQUAL);
        glm::mat4 newView = glm::mat4(glm::mat3(view));
        
        environRenderer.use();
        environRenderer.setMat4("view", newView);
        environRenderer.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);


        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwTerminate();

    std::cout << "\nPress any key to quit";
    getch();
	return 0;
}

void processInput(GLFWwindow* window, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

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
    if (!myMouse::moveMode)
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
        glm::vec3 disp = -glm::vec3(multiplier) * (camera::rightVector * offsetx * multiplier + newup * offsety* multiplier);

        for (int i = 0; i < 2500 && !collision; ++i)
        {
            if (i != myMouse::pickedObjectIndex)
            {
                glm::vec3 diff = translations[i] - (disp + translations[myMouse::pickedObjectIndex]);
                if (glm::dot(diff,diff) < 4 * radius * radius)
                {
                    collision = true;
                    return;
                }
            }

        }
        translations[myMouse::pickedObjectIndex] += disp;
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

            glm::vec3 mouseDir = dir;

            std::pair<int, float> minval = { 0,1000000000.0f };
            bool chk = false;
            for (int i = 0; i < 2500; ++i)
            {
                if (!discardIndices[i])
                    continue;
                glm::vec3 diff = camera::playerPosition - translations[i];

                float b = glm::dot(mouseDir, diff);
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
                myMouse::moveMode = true;
                myMouse::pointerReset = true;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            myMouse::pickedObjectMoveMode = false;
            myMouse::moveMode = false;
            if (myMouse::pickedObjectIndex != -1)
            {
                if (movableObject[myMouse::pickedObjectIndex])  // to prevent 2x 'gravity' effect. hack
                {
                    gravityIndices.push_back(myMouse::pickedObjectIndex);
                    movableObject[myMouse::pickedObjectIndex] = false;
                }
            }
            myMouse::pickedObjectIndex = -1;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            myMouse::moveMode = true;
            myMouse::pointerReset = true;
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            myMouse::moveMode = false;
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
