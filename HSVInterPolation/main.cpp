#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <format>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <random>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "shader.h"
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void RenderUI();
glm::vec3 camPos = glm::vec3(10.0f, 10.0f, 10.0f);
glm::vec3 camFront = glm::vec3(-1.0f, -1.0f, -1.0f);
glm::vec3 camUp = glm::vec3(0.0f, 0.0f, 1.0f);
float sensitivity = 5.0f;
bool focused = false;

bool firstMouse = true;
float yaw = 90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;
// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

const float PI = 3.14159265359f;
float deltaTimeFrame = .0f;
float lastFrame = .0f;


struct Vertex {
    glm::vec3 pos;
    glm::vec3 col;
    glm::vec3 normal;
    //glm::vec2 texCoord;

    bool operator==(const Vertex& other) const {
        return pos == other.pos;
    }
};

struct Object {
    int index;
    std::string model_path;
    unsigned int vao, vbo, ebo;
    unsigned int wvao, wvbo, webo;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> windices;
    glm::vec3 pos;
    glm::vec3 scale;
    int mode;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos)) >> 1);
        }
    };
}


bool loadModel(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::string model_path) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./"; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(model_path, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            std::unordered_map<Vertex, uint32_t> uniqueVertices{};
            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                Vertex vert{};
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                vert.pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vert.pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vert.pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                
                // Check if `normal_index` is zero or positive. negative = no normal data
                
                if (idx.normal_index >= 0) {
                    vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                }

                /*
                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    vert.texCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vert.texCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }
                */
                // Optional: vertex colors
                vert.col.r = attrib.colors[3 * size_t(idx.vertex_index) + 0];
                vert.col.g = attrib.colors[3 * size_t(idx.vertex_index) + 1];
                vert.col.b = attrib.colors[3 * size_t(idx.vertex_index) + 2];
                uniqueVertices[vert] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vert);
                //printf("vert pos: %f, %f, %f, \t index:%i \t col: %f, %f, %f\n", vert.pos.x, vert.pos.y, vert.pos.z, idx.vertex_index, vert.col.r, vert.col.g, vert.col.b );
                //vertices.push_back(vert);
                indices.push_back(uniqueVertices[vert]);
            }
            index_offset += fv;

        }
    }
    return true;
}


void generateGridData(std::vector<glm::vec3>& gridVertices, int a) {
    gridVertices.clear();
    glm::vec3 orig = glm::vec3(-a / 2.0f, -a / 2.0f, -2.0f);
    int sq = a * a;
    for (int i = 0; i < sq; i++) {
        glm::vec3 p1(i % a, i / a, 0.0f);
        glm::vec3 p2(i % a + 1, i / a, 0.0f);
        glm::vec3 p3(i % a, i / a + 1, 0.0f);
        glm::vec3 p4(i % a + 1, i / a + 1, 0.0f);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p2);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p3);

        gridVertices.push_back(orig + p2);
        gridVertices.push_back(orig + p4);

        gridVertices.push_back(orig + p3);
        gridVertices.push_back(orig + p4);
    }
}

void updateBufferData(unsigned int& bufIndex, std::vector<glm::vec3>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, bufIndex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * data.size(), &data[0], GL_DYNAMIC_DRAW);
}


void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float camSpeed = static_cast<float>(sensitivity * deltaTimeFrame);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camPos += camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camPos -= camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camPos += camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camPos -= camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


Object constructObj(std::string model_path) {
    Object obj{};
    obj.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    obj.model_path = model_path;
    if (loadModel(obj.vertices, obj.indices, obj.model_path)) {



        for (int i = 0; i < obj.indices.size(); i++) {
            obj.windices.push_back(obj.indices[i]);
            obj.windices.push_back(obj.indices[i + 1]);
            obj.windices.push_back(obj.indices[i + 1]);
            obj.windices.push_back(obj.indices[i + 2]);
            obj.windices.push_back(obj.indices[i + 2]);
            obj.windices.push_back(obj.indices[i]);
            i += 2;
        }
        glGenVertexArrays(1, &obj.vao);
        glGenBuffers(1, &obj.vbo);
        glGenBuffers(1, &obj.ebo);

        glBindVertexArray(obj.vao);
        glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
        glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(Vertex), &obj.vertices[0], GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);//pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));//col
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));//norm
        glEnableVertexAttribArray(2);
         //glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(9 * sizeof(float)));//texcoord
         //glEnableVertexAttribArray(3);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj.indices.size() * sizeof(float), &obj.indices[0], GL_DYNAMIC_DRAW);

        glGenVertexArrays(1, &obj.wvao);
        glGenBuffers(1, &obj.wvbo);
        glGenBuffers(1, &obj.webo);

        glBindVertexArray(obj.wvao);
        glBindBuffer(GL_ARRAY_BUFFER, obj.wvbo);
        glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(Vertex), &obj.vertices[0], GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);//pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));//col

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.webo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj.windices.size() * sizeof(float), &obj.windices[0], GL_DYNAMIC_DRAW);
    }
    return obj;
}

int main() {
    //set glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Curves for GS", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW Window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to init GLAD");
        return -1;
    }

    std::vector<Object> objects;
    std::vector<Object> testTriangles;
    testTriangles.push_back(constructObj("C:\\Src\\meshes\\plane.obj"));
    objects.push_back(constructObj("C:\\Src\\meshes\\dome1.obj"));

    unsigned int VAO_plane;
    glGenVertexArrays(1, &VAO_plane);

    float axisVertices[] = {
        0.0f,0.0f,0.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,

        0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,

        0.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f
    };

    unsigned int VBO_pos_p;
    glGenBuffers(1, &VBO_pos_p);

    glBindVertexArray(VAO_plane);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_pos_p);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVertices), &axisVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    std::vector<glm::vec3> gridVertices;
    glm::vec3 orig = glm::vec3(-8.0f, -8.0f, -2.0f);
    for (int i = 0; i < 256; i++) {
        glm::vec3 p1(i % 16, i / 16, 0.0f);
        glm::vec3 p2(i % 16 + 1, i / 16, 0.0f);
        glm::vec3 p3(i % 16, i / 16 + 1, 0.0f);
        glm::vec3 p4(i % 16 + 1, i / 16 + 1, 0.0f);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p2);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p3);

        gridVertices.push_back(orig + p2);
        gridVertices.push_back(orig + p4);

        gridVertices.push_back(orig + p3);
        gridVertices.push_back(orig + p4);
    }

    unsigned int VAO_grid;
    glGenVertexArrays(1, &VAO_grid);

    unsigned int VBO_grid;
    glGenBuffers(1, &VBO_grid);

    glBindVertexArray(VAO_grid);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_grid);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * 3 * sizeof(float), &gridVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int width, height;
    Shader rgbShader = Shader("C:\\Src\\shaders\\vertRGB.glsl", "C:\\Src\\shaders\\fragRGB.glsl");
    Shader cvShader = Shader("C:\\Src\\shaders\\vertCV.glsl", "C:\\Src\\shaders\\fragCV.glsl");
    Shader hsvShader = Shader("C:\\Src\\shaders\\vertHSV.glsl", "C:\\Src\\shaders\\fragHSV.glsl");
    Shader lineshader = Shader("C:\\Src\\shaders\\vertCoss.glsl", "C:\\Src\\shaders\\fragCoss.glsl");
    Shader uberColShader("C:\\Src\\shaders\\vertHSV.glsl", "C:\\Src\\shaders\\fragHSV.glsl");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glLineWidth(2.0f);

    float orthoScale = 10.0f;
    glm::vec3 lightPos = { 10.0f,10.f,10.f };
    bool xz = true, yz = true, xy = true, abc = false, x = true, y = true, z = true, ax = true, grid = true, inc = false, inc2 = false, wireframe = true;
    int config = 7, gridSize = 8;
    int ABC[3] = { 1,1,1 };

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    //ImGui::SetNextWindowPos(viewport->Pos);
    //ImGui::SetNextWindowSize(viewport->Size);
    //ImGui::SetNextWindowViewport(viewport->ID);
    bool ortho = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    int objDel = 0;
    static char objPathBuffer[32] = "C:\\Src\\meshes\\dome1.obj";
    int modeN = 9;
    std::string modes[] = {
        "RGB",
        "sRGB",
        "CV",
        "HSV",
        "HSL",
        "CIEXYZ",
        "CIELAB",
        "CIELUV",
        "CIEHLC",
    };
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTimeFrame = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        // render
        // ------
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        hsvShader.use();
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        uberColShader.setMat4("view", view);

        glfwGetWindowSize(window, &width, &height);

        float ratio = (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.01f, 1000.0f);
        if (ortho)
            projection = glm::ortho(-orthoScale * ratio, orthoScale * ratio, -orthoScale, orthoScale, -1000.0f, 1000.0f);

        uberColShader.setMat4("projection", projection);

        glm::mat4 model = glm::mat4(1.0f);
        uberColShader.setMat4("model", model);

        for (Object& o : testTriangles) {    
            glm::mat4 m = glm::translate(model, o.pos);
            m = glm::scale(m, o.scale);
            for (int i = 0; i < modeN; i++) {
                m = translate(m, glm::vec3(3.0f, 0.0f, 0.0f));
                uberColShader.setMat4("model", m);
                uberColShader.setInt("mode", i);
                glBindVertexArray(o.vao);
                glDrawElements(GL_TRIANGLES, o.indices.size(), GL_UNSIGNED_INT, 0);
            }
        }


        for (Object& o : objects) {
            glm::mat4 m = glm::translate(model, o.pos);
            m = glm::scale(m, o.scale);
            for (int i = 0; i < modeN; i++) {
                m = translate(m, glm::vec3(3.0f, 0.0f, 0.0f));
                uberColShader.setMat4("model", m);
                uberColShader.setInt("mode", i);
                glBindVertexArray(o.vao);
                glDrawElements(GL_TRIANGLES, o.indices.size(), GL_UNSIGNED_INT, 0);
            }
        }
        model = glm::mat4(1.0f);
        lineshader.use();
        lineshader.setMat4("projection", projection);
        lineshader.setMat4("view", view);
        lineshader.setMat4("model", model);
        
        if (grid) {
            glBindVertexArray(VAO_grid);
            glDrawArrays(GL_LINES, 0, gridVertices.size());
        }
        if (ax) {
            glBindVertexArray(VAO_plane);
            glDrawArrays(GL_LINES, 0, 12);
        }
        if (wireframe) {
            for (Object& o : objects) {
                glm::mat4 m = glm::translate(model, o.pos);
                m = glm::scale(m, o.scale);
                lineshader.setMat4("model", m);
                glBindVertexArray(o.wvao);
                glDrawElements(GL_LINES, o.windices.size(), GL_UNSIGNED_INT, 0);
            }
        }
        //start of imgui init stuff
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(viewport, dockspace_flags);

        ImGui::Begin("Render Settings");
        ImGui::DragFloat3("Light Pos", &lightPos[0], 0.05);
        ImGui::Checkbox("Orthographic", &ortho);
        if (ortho)
            ImGui::DragFloat("Orthographic Scale", &orthoScale, 0.05, 0.01f, 100.0f);
        ImGui::Checkbox("Grid", &grid);

        if (grid) {
            if (ImGui::DragInt("Size", &gridSize), 1, 100) {
                generateGridData(gridVertices, gridSize);
                updateBufferData(VBO_grid, gridVertices);
            }
        }
        ImGui::Checkbox("Axis", &ax);
        ImGui::Checkbox("Wireframe", &wireframe);
        if (ImGui::Button("Update Shaders")) {
            rgbShader = Shader("C:\\Src\\shaders\\vertRGB.glsl", "C:\\Src\\shaders\\fragRGB.glsl");
            cvShader = Shader("C:\\Src\\shaders\\vertCV.glsl", "C:\\Src\\shaders\\fragCV.glsl");
            hsvShader = Shader("C:\\Src\\shaders\\vertHSV.glsl", "C:\\Src\\shaders\\fragHSV.glsl");
            lineshader = Shader("C:\\Src\\shaders\\vertCoss.glsl", "C:\\Src\\shaders\\fragCoss.glsl");
            uberColShader = Shader("C:\\Src\\shaders\\vertHSV.glsl", "C:\\Src\\shaders\\fragHSV.glsl");
        }
        ImGui::End();
        ImGui::Begin("Object H.");
        if (ImGui::TreeNode("Obj Tree")) {
            ImGui::InputText("Object Path", objPathBuffer, IM_ARRAYSIZE(objPathBuffer));
            if (ImGui::Button("Add Object")) {
                Object obj = constructObj(objPathBuffer);
                if(!obj.vertices.empty())
                    objects.push_back(obj);
            }
            for (int i = 0; i < modeN; i++) {
                ImGui::Text((std::to_string(i) + ": " + modes[i]).c_str());
            }
            for (int i = 0; i < objects.size(); i++) {
                if (ImGui::TreeNode((void*)(intptr_t)(i), "Object %d", i+1)) {
                    ImGui::DragFloat3("Object Loc", &objects[i].pos[0], 0.01f, 0.01f);
                    ImGui::DragFloat3("Object Scale", &objects[i].scale[0], 0.01f, 0.01f);
                    ImGui::InputInt("Color Scheme", &objects[i].mode);
                    ImGui::TreePop();
                }

            }
            ImGui::InputInt("Obj No", &objDel);
            if (ImGui::Button("Delete Obj")) {
                objects.erase(objects.begin()+objDel - 1);
            }
            ImGui::TreePop();
        }
        ImGui::End();
        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    //stbi_image_free(data);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (focused) {

        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY; // reversed since y-coordinates go from bottom to top
        lastX = xpos;
        lastY = ypos;

        float mouseSens = 0.2f;
        xoffset *= mouseSens;
        yoffset *= mouseSens;

        yaw -= xoffset;
        pitch -= yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.z = sin(glm::radians(pitch));
        camFront = glm::normalize(front);

    }
    else {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);
        lastX = xpos;
        lastY = ypos;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {

    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        if (focused)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        focused = !focused;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    sensitivity += 0.2f * static_cast<float>(yoffset);
    if (sensitivity < 0) {
        sensitivity = 0.01f;
    }

    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

