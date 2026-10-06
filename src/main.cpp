#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "rcamera.h"

#include "physics.h"
#include "menu.h"

#include <random>
#include <iostream>

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif


//`CameraMode` is already defined in raylib but not quite what we want
enum CameraType {
    THIRD_PERSON,
    THIRD_PERSON_LOCKED,
    FIRST_PERSON,
};

namespace Basis {
    Vector3 i = {1.0f, 0.0f, 0.0f};
    Vector3 j = {0.0f, 1.0f, 0.0f};
    Vector3 k = {0.0f, 0.0f, 1.0f};
}

enum GameState {
    PLAYING,
    PAUSED,
    MENU,
};

enum AimType {
    DOT,
    RING,
};

float randomFloat() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    return dis(gen);
}

Vector3 anglesInCoordinateSystem(float xAngle, float yAngle, Vector3 xAxis, Vector3 yAxis, Vector3 zAxis, float distance=1) {
    return Vector3Scale(Vector3Scale(xAxis, sin(xAngle)*cos(yAngle)) + 
        Vector3Scale(yAxis, sin(yAngle)) + 
        Vector3Scale(zAxis, cos(xAngle)*cos(yAngle)), distance);
}

namespace Game {
    const int screenWidth = 1280;
    const int screenHeight = 800;
    GameState state;
    Camera camera;
    Vector2 camAngle;
    float cameraDistance;
    CameraType cameraMode;
    float mouseWheelMomentum;
    Vector2 mouseMomentum;
    bool hasGotMouseInput;
    Model f16;
    Model enemyPlane;
    Model f16Cockpit;
    Model map;
    Model skybox;
    Plane plane;
    Plane enemy;
    Shader depthShader;
    int depthLoc, colorLoc;
    Mesh ringAimerMesh;
    Model ringAimer;
    constexpr int gridSize = 100;
    PerlinNoise<8> PerlinMap{};
    PerlinNoise<gridSize> PerlinMapFiner{};
    float heights [gridSize][gridSize] {};
    std::array<int, 3> colors [gridSize][gridSize] {};
    Mesh generatedMesh = {};
    Model genMap;

    std::vector<Button> menuButtons {};
    Button playButton{};

    void setup() {
        camera = Camera3D {{ 0 }};
        camAngle = {0, -.1f};
        cameraMode = CameraType::THIRD_PERSON;
        mouseWheelMomentum = 0;
        mouseMomentum = {0, 0};
        hasGotMouseInput = false;
        f16 = LoadModel("src/assets/plane.obj");
        enemyPlane = LoadModel("src/assets/plane.obj");
        f16Cockpit = LoadModel("src/assets/cockpit.obj");
        map = LoadModel("src/assets/landscape_test.obj");
        skybox = LoadModel("src/assets/skybox.obj");
        plane = Plane(&f16, &f16Cockpit);
        enemy = Plane(&enemyPlane);
        ringAimerMesh = GenMeshTorus(0.1f, 0.5f, 6, 12);
        ringAimer = LoadModelFromMesh(ringAimerMesh);

        camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        camera.fovy = 60.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        cameraDistance = 10.0f;
        
        plane.position = {4000.0f, 300.0f, 4000.0f};
        
        enemy.position = {4000.0f, 300.0f, 4500.0f};
        enemy.front = {0.0f, 0.0f, -1.0f};

        generatedMesh.triangleCount = gridSize*gridSize*2;
        generatedMesh.vertexCount = generatedMesh.triangleCount*3;
        generatedMesh.vertices = (float *)MemAlloc(generatedMesh.vertexCount*3*sizeof(float)); 
        generatedMesh.normals = (float *)MemAlloc(generatedMesh.vertexCount*3*sizeof(float)); 
        generatedMesh.colors = (unsigned char *)MemAlloc(generatedMesh.vertexCount * 4 * sizeof(unsigned char));
        
        for (int i = 0; i < gridSize; i += 1) {
            for (int j = 0; j < gridSize; j += 1) {
                heights[i][j] = PerlinMap.value(i*0.1f, j*0.1f) * 1000.0f + PerlinMapFiner.value(i, j) * 200.0f;
                float colorMult = -randomFloat();
                colors[i][j] = {static_cast<int>(std::round(9.83f*colorMult)), static_cast<int>(std::round(15.7f*colorMult)), static_cast<int>(std::round(20.0f*colorMult))};
            }
        }

        float scale = 100.0f;

        int index = 0;
        for (int i = 0; i < gridSize-1; ++i) {
            for (int j = 0; j < gridSize-1; ++j) {
                for (const std::pair<int, int> pair: std::vector<std::pair<int, int>>{/*first triangle in quad*/{i, j}, {i, j+1}, {i+1, j+1}, /*second triangle in quad*/{i, j}, {i+1, j+1}, {i+1, j}}) {
                    int x = pair.first, y = pair.second;
                    std::array<int, 3> colorMod = colors[x][y];
                    std::array<int, 3> color = {static_cast<int>(std::floor(heights[x][y]))+100, 255, 20};
                    if (heights[x][y] > 30.0f) color = {255, 255, 255};
                    
                    generatedMesh.colors[index*4 + 0] = color[0];
                    generatedMesh.colors[index*4 + 1] = color[1];
                    generatedMesh.colors[index*4 + 2] = color[2];
                    generatedMesh.colors[index*4 + 3] = 255;
                    generatedMesh.vertices[index*3 + 0] = x*scale;
                    generatedMesh.vertices[index*3 + 1] = heights[x][y];
                    generatedMesh.vertices[index*3 + 2] = y*scale;
                    generatedMesh.normals[index*3 + 0] = 0.0f;
                    generatedMesh.normals[index*3 + 1] = 1.0f;
                    generatedMesh.normals[index*3 + 2] = 0.0f;

                    ++index;
                }
            }
        }


        UploadMesh(&generatedMesh, false);
        genMap = LoadModelFromMesh(generatedMesh);

        playButton = Button(screenWidth/2, screenHeight/2, 200, 100, "Deploy!");
        playButton.action = [] () {
            state = GameState::PLAYING;
        };
        menuButtons = {playButton};

        if (IsCursorHidden()) EnableCursor();
    }
    void cleanup() {
        UnloadModel(f16);
        UnloadModel(enemyPlane);
        UnloadModel(f16Cockpit);
        UnloadModel(map);
        UnloadModel(skybox);
        UnloadModel(ringAimer);
        UnloadModel(genMap);
    }

    void renderGame() {
        // Draw into screen using depth/color texture buffer 
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                rlDisableDepthMask();
                DrawModel(skybox, camera.position, 1.0f, WHITE); //skybox is independent of depth buffer and is regarded as "behind" everything else
                rlEnableDepthMask();
                
            EndMode3D();

            BeginMode3D(camera);
                Vector3 shake = Vector3Scale({randomFloat()-0.5f, randomFloat()-0.5f, randomFloat()-0.5f}, plane.elevator.deflection * 0.03f);
                if (cameraMode != CameraType::FIRST_PERSON) shake *= 3;
                if (state == GameState::PLAYING) plane.position += shake;

                float pipper3DDistance = 20.0f;
                float bulletSpeed = 10.0f;
                if (cameraMode == CameraType::FIRST_PERSON) {
                    DrawModel(*plane.cockpitModel, plane.position, 1.0f, WHITE);
                    
                    float distanceToEnemy = Vector3Length(enemy.position-plane.position);
                    float angleToEnemy = Vector3Angle(enemy.position-plane.position, plane.front);
                    
                    Vector3 enemyLead = leadAngleCalculation(enemy.position-plane.position, enemy.velocity, bulletSpeed);
                    Vector3 pipperInFront = camera.position + Vector3Scale(plane.front, pipper3DDistance);
                    Vector3 pipperWithLead = pipperInFront - enemyLead*pipper3DDistance/distanceToEnemy;
                    
                    ringAimer.transform = plane.model->transform;
                    
                    
                    if (angleToEnemy < 0.4f) {
                        DrawSphere(pipperInFront, .04f, RED);
                        DrawModel(ringAimer, pipperWithLead, 1.0f, RED);
                    } else {
                        DrawSphere(pipperInFront, .04f, RED);
                        DrawModel(ringAimer, pipperInFront, 1.0f, RED);
                    }
                    
                    //DrawSphere(enemy.position + enemyLead, .4f, RED);
                } else {
                    DrawModel(*plane.model, plane.position, 1.0f, WHITE);
                }

                if (state == GameState::PLAYING) plane.position -= shake;

                DrawModel(*enemy.model, enemy.position, 1.0f, WHITE);
                
                //DrawModel(map, {0, 0, 0}, 1.0f, WHITE);
                DrawModel(genMap, {0, 0, 0}, 1.0f, WHITE);                
            EndMode3D();
        EndDrawing();
    }
    void mainLoop() {
        if (state == GameState::PAUSED || state == GameState::PLAYING) {
            SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
            if (state == GameState::PLAYING) {
                if (!IsCursorHidden() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    DisableCursor();
                }

                if (IsKeyPressed(KEY_ESCAPE)) {
                    state = GameState::PAUSED;
                }

                Vector2 mouseMovement = GetMouseDelta();
                
                if (hasGotMouseInput && (true || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsCursorHidden())) {
                    mouseMomentum += mouseMovement * 0.3f;
                }
                camAngle.x += mouseMomentum.x*0.002f;
                camAngle.y -= mouseMomentum.y*0.002f;
                camAngle.y = Clamp(camAngle.y, -PI/2, PI/2);
                if (!hasGotMouseInput && (mouseMovement.x != 0 || mouseMovement.y != 0)) hasGotMouseInput = true;
                mouseMomentum *= 0.8f;

                mouseWheelMomentum += GetMouseWheelMove() * 0.03f;
                if (cameraMode == CameraType::FIRST_PERSON) {
                    camera.fovy = Clamp(camera.fovy * (1-mouseWheelMomentum), 10, 100);
                } else {
                    cameraDistance = Clamp(cameraDistance - mouseWheelMomentum*cameraDistance/5.0f, 4, 400);
                }
                mouseWheelMomentum *= 0.6f;
                            
                if (IsKeyDown(KEY_A)) {
                    plane.rudder.turn(-1);
                } else if (IsKeyDown(KEY_D)) {
                    plane.rudder.turn(1);
                } else plane.rudder.reset();
                
                if (IsKeyDown(KEY_S)) {
                    plane.elevator.turn(1);
                } else if (IsKeyDown(KEY_W)) {
                    plane.elevator.turn(-1);
                } else plane.elevator.reset();
                
                if (IsKeyDown(KEY_E)) {
                    plane.aileron.turn(1);
                } else if (IsKeyDown(KEY_Q)) {
                    plane.aileron.turn(-1);
                } else plane.aileron.reset();
                
                plane.update();
                enemy.attack(plane);
                enemy.update();
                
                if (IsKeyPressed(KEY_C)) {
                    if (cameraMode == CameraType::THIRD_PERSON) {
                        cameraMode = CameraType::FIRST_PERSON;
                        camAngle = {0, 0};
                    } else if (cameraMode == CameraType::FIRST_PERSON) {
                        cameraMode = CameraType::THIRD_PERSON_LOCKED;
                        camera.fovy = 60.0f;
                        cameraDistance = 10.0f;
                        camAngle = {-.1f, -.2f};
                    } else if (cameraMode == CameraType::THIRD_PERSON_LOCKED) {
                        cameraMode = CameraType::THIRD_PERSON;
                        camAngle = {0, 0};
                    } 
                    std::cout << "Keypress c" << std::endl;
                }
            
                
                if (cameraMode == CameraType::THIRD_PERSON) {
                    camera.target = plane.position;
                    camAngle.y = Clamp(camAngle.y, -1.5f, 1.5f);
                    camera.up = Basis::j;
                    camera.position = plane.position + anglesInCoordinateSystem(-camAngle.x, camAngle.y, Basis::i, Basis::j, Basis::k, -cameraDistance);
                } else if (cameraMode == CameraType::THIRD_PERSON_LOCKED) {
                    camera.target = plane.position;
                    camAngle.y = Clamp(camAngle.y, -1.5f, 1.5f);
                    camera.position = plane.position + anglesInCoordinateSystem(camAngle.x, camAngle.y, plane.right(), plane.up, plane.front, -cameraDistance);
                    camera.up = plane.up;
                } else if (cameraMode == CameraType::FIRST_PERSON) {
                    camAngle.x = Clamp(camAngle.x, -PI,PI);
                    camAngle.y = Clamp(camAngle.y, -.7f, 1.5f);
                    camera.up = plane.up;
                    camera.position = plane.position + Vector3Scale(plane.up, .7f) + Vector3Scale(plane.front, .65f);
                    camera.target = camera.position + anglesInCoordinateSystem(camAngle.x, camAngle.y, plane.right(), plane.up, plane.front);
                }
                if (IsKeyPressed(KEY_R)) {
                    std::cout << "Keypress r" << std::endl;
                    cleanup();
                    setup();
                } else if (IsKeyPressed(KEY_M)) {
                    std::cout << "Keypress m" << std::endl;
                    EnableCursor();
                    state = GameState::MENU;
                    cleanup();
                    setup();
                } else {
                    renderGame();
                }
            } else {
                if (IsCursorHidden()) {
                    EnableCursor();
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    state = GameState::PLAYING;
                }
                if (IsKeyPressed(KEY_R)) {
                    cleanup();
                    setup();
                } else if (IsKeyPressed(KEY_M)) {
                    EnableCursor();
                    state = GameState::MENU;
                    cleanup();
                    setup();
                } else {
                    renderGame();
                }
            }

            
            
        } else if (state == GameState::MENU) {
            BeginDrawing();
                ClearBackground(RAYWHITE);
                
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
                for (const Button& button: menuButtons) {
                    button.render(GetMousePosition());
                }
            EndDrawing();
        }
    }
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(Game::screenWidth, Game::screenHeight, "Raylib Plane Sim");
    rlDisableBackfaceCulling();
    rlSetClipPlanes(0.1f, 10000.0f);
    ChangeDirectory(GetApplicationDirectory());
    // Load render texture with a depth texture attached
    // Load depth shader and get depth texture shader location
    
    if (GLSL_VERSION == 100) Game::depthShader = LoadShader(TextFormat("src/assets/lighting_100.vs", GLSL_VERSION), TextFormat("src/assets/lighting_100.fs", GLSL_VERSION));
    else if (GLSL_VERSION == 330) Game::depthShader = LoadShader(TextFormat("src/assets/lighting_330.vs", GLSL_VERSION), TextFormat("src/assets/lighting_330.fs", GLSL_VERSION));
    Game::depthLoc = GetShaderLocation(Game::depthShader, "depthTexture");
    Game::colorLoc = GetShaderLocation(Game::depthShader, "colorTexture");
    
    int flipTextureLoc = GetShaderLocation(Game::depthShader, "flipY");
    SetShaderValue(Game::depthShader, flipTextureLoc, (int[]){ 1 }, SHADER_UNIFORM_INT); // Flip Y texture
    
    SetExitKey(KEY_GRAVE);
    SetTargetFPS(60);

    Game::setup();
    Game::state = GameState::MENU;

    Game::genMap.materials[0].shader = Game::depthShader;


    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(Game::mainLoop, 0, 1);
    #endif
    #if defined(PLATFORM_DESKTOP)
        while (!WindowShouldClose()) {
            Game::mainLoop();
        }
    #endif

    Game::cleanup();
    
    UnloadShader(Game::depthShader);      // Unload shader

    CloseWindow();
    
    return 0;
}
