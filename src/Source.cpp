#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <shader-source-code.h>
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

const int w_win = 800;
const int h_win = 600;

glm::mat4 model = glm::mat4(1.0f);
glm::mat4 view = glm::mat4(1.0f);
glm::mat4 projection = glm::mat4(1.0f);

glm::vec3 pos(0.0f, 100.0f, 65.0f);
glm::vec3 front(0.0f, 0.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);
glm::vec3 right = glm::normalize(glm::cross(-front, up));

float Mposx = 300.0f;
float Mposy = 300.0f;

//time 
bool firstMouse = true;
float lastFrame = 0.0f;
float deltaTime = 0.0f;
float speed = 50.0f;
bool mouseMoveMode = false;
bool objectMoveMode = false;
bool collisionCheck = false;
int objectIndex = 0;
float explodeTime = 4.0f;
glm::vec3 mouseDir(0.0f,0.0f,0.0f);
std::vector<glm::vec3> translations(2500);
std::vector<int> discardIndices(2500,1);
float multiplier = 5;
float radius = 0.4f * multiplier;

float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
std::list<int> gravityIndices;
std::list<int> explosionIndices;
std::vector<short> movableObject(2500, 1);
std::vector<glm::vec3> explosionPositions;
std::vector<float> explosionStartTimes;

void processInput(GLFWwindow* window);
void mouse_move_callback(GLFWwindow* window, double posx, double posy);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);


void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
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
	// sphere equation- x2 + y2 + z2 = r22
	// parameteric form- 
	// x: rCpCt  
	// y: rCpSt 
	// z:rSp

    const float PI = acos(-1.0f);
    int sectorCount = longitude;
    int stackCount = latitude + 1;
   
    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // normal

    float sectorStep = 2.0f * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<int> indices;
    std::vector<float> lineIndices;

    for (int i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        std::cout << sinf(stackAngle);
        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // normalized vertex normal
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            normals.push_back(nx);
            normals.push_back(ny);
            normals.push_back(nz);
        }
    }

    // indices
    //  k1--k1+1
    //  |  / |
    //  | /  |
    //  k2--k2+1
    unsigned int k1, k2;
    for (int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding 1st and last stacks
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    // generate interleaved vertex array as well
    std::vector<float> interleavedVertices;
    std::size_t i, j;
    std::size_t count = vertices.size();
    for (i = 0, j = 0; i < count; i += 3, j += 2)
    {
        interleavedVertices.push_back(vertices[i]);
        interleavedVertices.push_back(vertices[i + 1]);
        interleavedVertices.push_back(vertices[i + 2]);

        interleavedVertices.push_back(normals[i]);
        interleavedVertices.push_back(normals[i + 1]);
        interleavedVertices.push_back(normals[i + 2]);
    }

    
    // Shaders 
    Shader shapeRenderer(primaryshader::vs, primaryshader::fs);   // to render objects
    Shader environRenderer(cubemapshader::vs, cubemapshader::fs); // to render the Cube map
    Shader explosionRenderer(explosionshader::vs, explosionshader::fs, explosionshader::gs); // to destroy objects
    
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

    explosionRenderer.use();
    explosionRenderer.setInt("skybox", 0);
    explosionRenderer.setFloat("lifetime", explodeTime);

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(GL_ARRAY_BUFFER, interleavedVertices.size()*sizeof(float), &interleavedVertices[0], GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);

    
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

    unsigned int instanceVBO, rendercheckVBO;
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


    unsigned int VAO2;
    glGenVertexArrays(1, &VAO2);

    glBindVertexArray(VAO2);

    explosionPositions = translations;
    explosionStartTimes = std::vector<float>(translations.size(), 0.0f);
    unsigned int instanceVBO2, explosionTimeVBO;
    glGenBuffers(1, &instanceVBO2);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * explosionPositions.size(), &explosionPositions[0], GL_DYNAMIC_DRAW);

    glGenBuffers(1, &explosionTimeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, explosionTimeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * explosionStartTimes.size(), &explosionStartTimes[0], GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GL_FLOAT), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GL_FLOAT), (void*)(3 * sizeof(GL_FLOAT)));
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO2);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glVertexAttribDivisor(2, 1);
    glBindBuffer(GL_ARRAY_BUFFER, explosionTimeVBO);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT), (void*)0);
    glVertexAttribDivisor(3, 1);

    glEnable(GL_DEPTH_TEST);

    float fov = 45.0f;
    
    shapeRenderer.use();
    shapeRenderer.setInt("skybox", 0);
    
    glm::vec3 overtime[20];
    for (int i = 0; i < 10; ++i)
    {
        glm::vec3 m;
        m = glm::vec3(0.0f, i*5.0f, 0.0f);
        overtime[i] = m;
    }
    for (int i = 11; i < 20; ++i)
    {
        glm::vec3 m;
        m = glm::vec3(0.0f, (20-i) * 5.0f, 0.0f);
        overtime[i] = m;
    }

    

    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    int numFrames = 0; float startTime = 0;
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
        view = glm::lookAt(pos, pos + front, up);
        projection = glm::perspective(glm::radians(fov), w_win * 1.0f / h_win, 0.1f, 2000.0f);
        
        processInput(window);

        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        model = glm::mat4(1.0f);
        
#if 1
        shapeRenderer.use();
        shapeRenderer.setMat4("model", model);
        shapeRenderer.setMat4("view", view);
        shapeRenderer.setMat4("projection", projection);
        shapeRenderer.setVec3("cameraPos", pos);
       
        
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        
        if(objectIndex != -1)
        {
            glBufferSubData(GL_ARRAY_BUFFER, objectIndex * sizeof(glm::vec3), sizeof(glm::vec3), glm::value_ptr(translations[objectIndex]));
        }
        
        bool explosionAdded = false;
        if (gravityIndices.size())
        {
            glm::vec3 disp = {0,-10,0};

            std::list<int>::iterator gravityIterator = gravityIndices.begin();
            while (gravityIterator != gravityIndices.end())
            {
                if (*gravityIterator == objectIndex)
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
                    explosionIndices.push_back(*gravityIterator);
                    explosionPositions[explosionIndices.size() - 1] = translations[*gravityIterator];
                    explosionStartTimes[explosionIndices.size() - 1] = currentFrame;
                    
                    discardIndices[*gravityIterator] = 0;
                    glBindBuffer(GL_ARRAY_BUFFER, rendercheckVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, *gravityIterator * sizeof(int), sizeof(int), &discardIndices[*gravityIterator]);

                    gravityIterator = gravityIndices.erase(gravityIterator);
                    explosionAdded = true;
                }
                else
                    ++gravityIterator;
            }
        }

        int numRemoved = 0;
        for (int i = 0; i < explosionIndices.size(); ++i)
        {
            if (currentFrame - explosionStartTimes[i] > explodeTime)
            {
                // remove from here
                ++numRemoved;
            }
            else
            {
                // nothing to update since geometry shader will do this

                //break;  // "start time sorted"
            }
        }
        if (numRemoved || explosionAdded)
        {
            if (explosionIndices.size() - numRemoved)
            {
                if (numRemoved)
                {
                    std::copy(explosionStartTimes.begin() + numRemoved, explosionStartTimes.begin() + explosionIndices.size(), explosionStartTimes.begin());
                    std::copy(explosionPositions.begin() + numRemoved, explosionPositions.begin() + explosionIndices.size(), explosionPositions.begin());
                }
                glBindBuffer(GL_ARRAY_BUFFER, explosionTimeVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, (explosionIndices.size() - numRemoved) * sizeof(GL_FLOAT), &explosionStartTimes[0]);
                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO2);
                glBufferSubData(GL_ARRAY_BUFFER, 0, (explosionIndices.size() - numRemoved) * sizeof(glm::vec3), glm::value_ptr(explosionPositions[0]));
            }

            if(numRemoved > 0)
                while (numRemoved--)
                {
                    explosionIndices.pop_front();
                }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glBindVertexArray(VAO);
        shapeRenderer.setInt("linemode", 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElementsInstanced(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0, 2500);
        
        shapeRenderer.setInt("linemode", 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElementsInstanced(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0, 2);
        
#endif

        if (explosionIndices.size())
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
            explosionRenderer.use();
            explosionRenderer.setMat4("model", model);
            explosionRenderer.setMat4("view", view);
            explosionRenderer.setMat4("projection", projection);
            explosionRenderer.setVec3("cameraPos", pos);
            explosionRenderer.setFloat("currentTime", currentFrame);
            
            glBindVertexArray(VAO2);
            glDrawElementsInstanced(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0, explosionIndices.size());
        }

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
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);


        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwTerminate();

    std::cout << "\nPress any key to quit";
    getchar();
	return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        pos += cameraSpeed * front; 
        //std::cout << pos[0]<<" " << pos[1] << " " << pos[2] << " " << deltaTime << "\n";
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        pos -= cameraSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        pos -= glm::normalize(glm::cross(front, up)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        pos += glm::normalize(glm::cross(front, up)) * cameraSpeed;
}

void mouse_move_callback(GLFWwindow* window, double posx, double posy)
{
    if (!mouseMoveMode)
        return;

    if (firstMouse)
    {
        firstMouse = false;
        Mposx = posx;
        Mposy = posy;
    }

    float offsetx, offsety;

    offsetx = posx - Mposx;
    offsety = posy - Mposy;

    
    if (objectMoveMode)
    {
        //move the sphere. check for collisions first. if colliding, don't update mouse.
        bool collision = false;
        float multiplier = 0.03f;
        
        glm::vec3 newup = glm::normalize(glm::cross(front, right));
        glm::vec3 disp = -glm::vec3(multiplier) * (right * offsetx * multiplier + newup * offsety* multiplier);
        //glm::vec3 disp = { multiplier * offsetx, 0, multiplier * offsety};

        for (int i = 0; i < 2500 && !collision; ++i)
        {
            if (i != objectIndex)
            {
                glm::vec3 diff = translations[i] - (disp + translations[objectIndex]);
                if (glm::dot(diff,diff) < 4 * radius * radius)
                {
                    //std::cout << "\nCollision with index " << i;
                    collision = true;
                    return;
                }
            }

        }
        translations[objectIndex] += disp;
        //std::cout << glm::dot(disp,disp) << ":";
        return;
    }
    
    Mposx = posx;
    Mposy = posy;

    //camera.ProcessMouseMovement(offsetx, -offsety);

    float sensitivity = 0.05f;
    offsetx *= sensitivity;
    offsety *= sensitivity;

    yaw += offsetx;
    pitch -= offsety;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(direction);
    right = glm::normalize(glm::cross(-front, up));
    //std::cout << front[0] << ":" << front[1] << ":" << front[2] << "\n";;
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
            //std::cout << "\nCursor Position at (" << xpos << " : " << ypos << ")";

     
            float x = (2.0f * xpos) / w_win - 1.0f;
            float y = 1.0f - (2.0f * ypos) / h_win;
            float z = 1.0f;
            glm::vec3 ray_nds = glm::vec3(x, y, z);
            glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0, 1.0);
            glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
            ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
            glm::vec3 dir = (glm::inverse(view) * ray_eye);
            dir = glm::normalize(dir);   

            mouseDir = dir;

            std::pair<int, float> minval = { 0,1000000000 };
            bool chk = false;
            for (int i = 0; i < 2500; ++i)
            {
                if (!discardIndices[i])
                    continue;
                glm::vec3 diff = pos - translations[i];

                float b = glm::dot(mouseDir, diff);
                float c = glm::dot(diff, diff) - radius * radius;
                if (b * b - c < 0)
                {

                }
                else
                {
                    chk = true;
                    //std::cout << "\tcolliding with sphere index " << i;

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
                //std::cout << "\tclosest sphere index is " << minval.first;
                objectMoveMode = true;
                objectIndex = minval.first;
                mouseMoveMode = true;
                firstMouse = true;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            objectMoveMode = false;
            mouseMoveMode = false;
            if (objectIndex != -1)
            {
                if (movableObject[objectIndex])
                {
                    gravityIndices.push_back(objectIndex);
                    movableObject[objectIndex] = false;
                }
            }
            objectIndex = -1;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mouseMoveMode = true;
            firstMouse = true;
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseMoveMode = false;
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
