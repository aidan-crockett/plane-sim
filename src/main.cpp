#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "rcamera.h"

#include "physics.h"

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

void resetToZero(float& input, float step) {
    if (std::abs(input) < step) input = 0;
    else if (input < 0) input += step;
    else input -= step;
}

//--------------------------------------------------------------------------------------
// Module Functions Declaration
//--------------------------------------------------------------------------------------
// Load custom render texture with depth texture attached
static RenderTexture2D LoadRenderTextureDepthTex(int width, int height);

// Unload render texture from GPU memory (VRAM)
static void UnloadRenderTextureDepthTex(RenderTexture2D target);

namespace Game {
    const int screenWidth = 1280;
    const int screenHeight = 800;
    bool paused;
    Camera camera;
    Vector2 camAngle;
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
    RenderTexture2D target;
    Shader depthShader;
    int depthLoc, colorLoc;
    Mesh ringAimerMesh;
    Model ringAimer;
    PerlinNoise<8> PerlinMap{};
    PerlinNoise<40> PerlinMapFiner{};
    float heights [80][80] {};
    

    void mainLoop() {
        if (!paused) {
            if (!IsCursorHidden() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                DisableCursor();
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                paused = true;
            }

            Vector2 mouseMovement = GetMouseDelta();
            
            if (hasGotMouseInput && (true || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsCursorHidden())) {
                mouseMomentum += mouseMovement * 0.3;
            }
            camAngle.x += mouseMomentum.x*0.002;
            camAngle.y -= mouseMomentum.y*0.002;
            camAngle.y = Clamp(camAngle.y, -3.14/2, 3.14/2);
            if (!hasGotMouseInput && (mouseMovement.x != 0 || mouseMovement.y != 0)) hasGotMouseInput = true;
            mouseMomentum *= 0.8;

            mouseWheelMomentum += GetMouseWheelMove() * 0.02;
            camera.fovy = Clamp(camera.fovy * (1-mouseWheelMomentum), 10, 100);
            mouseWheelMomentum *= 0.9;
            
            
            bool pitching = false, rolling = false, yawing = false;
            float elevatorPitchRate = 0.01, aileronPitchRate = 0.015, rudderPitchRate = 0.01;
            
            if (IsKeyDown(KEY_A)) {
                plane.rudderDeflection -= rudderPitchRate;
                yawing = true;
            }
            if (IsKeyDown(KEY_D)) {
                plane.rudderDeflection += rudderPitchRate;
                yawing = true;
            }
            if (!yawing) {
                resetToZero(plane.rudderDeflection, rudderPitchRate);
            } else plane.rudderDeflection = Clamp(plane.rudderDeflection, -DEG2RAD*20, DEG2RAD*20);
            
            if (IsKeyDown(KEY_S)) {
                plane.elevatorDeflection += elevatorPitchRate;
                pitching = true;
            }
            if (IsKeyDown(KEY_W)) {
                plane.elevatorDeflection -= elevatorPitchRate;
                pitching = true;
            }
            if (!pitching) {
                resetToZero(plane.elevatorDeflection, elevatorPitchRate);
            } else plane.elevatorDeflection = Clamp(plane.elevatorDeflection, -DEG2RAD*10, DEG2RAD*20);
            
            if (IsKeyDown(KEY_E)) {
                plane.aileronDeflection += aileronPitchRate;
                rolling = true;
            }
            if (IsKeyDown(KEY_Q)) {
                plane.aileronDeflection -= aileronPitchRate;
                rolling = true;
            } 
            if (!rolling) {
                resetToZero(plane.aileronDeflection, aileronPitchRate);
            } else plane.aileronDeflection = Clamp(plane.aileronDeflection, -DEG2RAD*20, DEG2RAD*20);
            
            plane.update();
            enemy.attack(plane);
            enemy.update();
            
            if (IsKeyPressed(KEY_C)) {
                if (cameraMode == CameraType::THIRD_PERSON) {
                    //cameraMode = CameraType::THIRD_PERSON_LOCKED;
                    cameraMode = CameraType::FIRST_PERSON;
                    camAngle = {0, 0};
                } else if (cameraMode == CameraType::THIRD_PERSON_LOCKED) {
                    cameraMode = CameraType::FIRST_PERSON;
                    camAngle = {0, 0};
                } else if (cameraMode == CameraType::FIRST_PERSON) {
                    cameraMode = CameraType::THIRD_PERSON;
                }
            }
        
            //transform matrix can be formed directly from orientation vectors as columns (orientation vectors form x, y, z axes)
            //as opposed to using MatrixRotateXYZ from euler angles (suffers from gimbal lock)
            
            float cameraDistance = 10;
            
            if (cameraMode == CameraType::THIRD_PERSON) {
                camera.target = plane.position;
                camera.up = {0, 1.0f, 0};
                camera.position = Vector3Add(plane.position, Vector3Scale({sin(-camAngle.x)*cos(camAngle.y), sin(camAngle.y), cos(-camAngle.x)*cos(camAngle.y)}, -cameraDistance));
            } else if (cameraMode == CameraType::THIRD_PERSON_LOCKED) {
                camera.target = plane.position;
                camera.position = plane.position + Vector3Scale(
                    Vector3Scale(plane.right(), sin(camAngle.x)*cos(camAngle.y)) + 
                    Vector3Scale(plane.up, sin(camAngle.y)) + 
                    Vector3Scale(plane.front, cos(camAngle.x)*cos(camAngle.y)), 
                    -cameraDistance);
                camera.up = plane.up;
            } else if (cameraMode == CameraType::FIRST_PERSON) {
                camAngle.x = Clamp(camAngle.x, -0.95*PI, 0.95*PI);
                camAngle.y = Clamp(camAngle.y, -.7, 1.55);
                camera.up = plane.up;
                camera.position = plane.position + Vector3Scale(plane.up, .7) + Vector3Scale(plane.front, .65);
                camera.target = camera.position + Vector3Scale(
                    Vector3Scale(plane.right(), sin(camAngle.x)*cos(camAngle.y)) + 
                    Vector3Scale(plane.up, sin(camAngle.y)) + 
                    Vector3Scale(plane.front, cos(camAngle.x)*cos(camAngle.y)), 
                    10);
            }
            if (plane.elevatorDeflection != 0) {
                Vector3 offset = Vector3Scale({randomFloat()-0.5f, randomFloat()-0.5f, randomFloat()-0.5f}, plane.elevatorDeflection * 0.03);
                if (cameraMode == CameraType::FIRST_PERSON) {
                    camera.position = camera.position + offset;
                    camera.target = camera.target + offset;
                }
                else if (cameraMode == CameraType::THIRD_PERSON || cameraMode == CameraType::THIRD_PERSON_LOCKED) plane.position = plane.position + offset;
            }
        } else {
            if (IsCursorHidden()) {
                EnableCursor();
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                paused = false;
            }
        }

        BeginTextureMode(target); //writing to depth/color texture buffer first
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
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
                    
                    
                    if (angleToEnemy < 0.4) {
                        DrawSphere(pipperInFront, .04, RED);
                        DrawModel(ringAimer, pipperWithLead, 1.0f, RED);
                    } else {
                        DrawSphere(pipperInFront, .04, RED);
                        DrawModel(ringAimer, pipperInFront, 1.0f, RED);
                    }
                    
                    //DrawSphere(enemy.position + enemyLead, .4, RED);
                } else {
                    DrawModel(*plane.model, plane.position, 1.0f, WHITE);
                }

                DrawModel(*enemy.model, enemy.position, 1.0f, WHITE);
                
                
                //DrawModel(map, {0, 0, 0}, 1.0f, WHITE);

                float scale = 1000.0f;

                for (int i = 0; i < 79; ++i) {
                    for (int j = 0; j < 80; ++j) {
                        rlBegin(RL_QUADS);
                            if (heights[i][j] > 30) rlColor4ub(255, 255, 255, 255); else rlColor4ub(heights[i][j]+100, 255, 0, 255); // Green
                            rlVertex3f(i*0.1*scale, heights[i][j], j*0.1*scale);   // Top vertex
                            
                            if (heights[i][j+1] > 30) rlColor4ub(255, 255, 255, 255); else rlColor4ub(heights[i][j+1]+100, 255, 0, 255); // Green
                            rlVertex3f(i*0.1*scale, heights[i][j+1], (j+1)*0.1*scale); // Bottom-left vertex

                            if (heights[i+1][j+1] > 30) rlColor4ub(255, 255, 255, 255); else rlColor4ub(heights[i+1][j+1]+100, 255, 0, 255); // Green
                            rlVertex3f((i+1)*0.1*scale, heights[i+1][j+1], (j+1)*0.1*scale);   // Top vertex
                            
                            if (heights[i+1][j] > 30) rlColor4ub(255, 255, 255, 255); else rlColor4ub(heights[i+1][j]+100, 255, 0, 255); // Green
                            rlVertex3f((i+1)*0.1*scale, heights[i+1][j], (j)*0.1*scale);
                        rlEnd();

                        
                    }
                }//*/
                
            EndMode3D();
        EndTextureMode();

        // Draw into screen using depth/color texture buffer 
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                rlDisableDepthMask();
                DrawModel(skybox, plane.position, 1.0f, WHITE); //skybox is independent of depth buffer and is regarded as "behind" everything else
                rlEnableDepthMask();
                
            EndMode3D();

            BeginShaderMode(depthShader);
                SetShaderValueTexture(depthShader, colorLoc, target.texture); //send color buffer to shader
                SetShaderValueTexture(depthShader, depthLoc, target.depth); //send depth buffer to shader
                
                DrawTexture(target.texture, 0, 0, WHITE);
            EndShaderMode();
        EndDrawing();
    }
}

using namespace Game;

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenWidth, screenHeight, "Raylib Plane Sim");
    rlDisableBackfaceCulling();
    rlSetClipPlanes(0.1, 10000.0);
    ChangeDirectory(GetApplicationDirectory());
    
    SetExitKey(KEY_GRAVE);

    paused = false;
    camera = { 0 };
    camAngle = {0, -.1};
    cameraMode = CameraType::THIRD_PERSON;
    mouseWheelMomentum = 0;
    mouseMomentum = {0, 0};
    hasGotMouseInput = false;
    f16 = LoadModel("src/assets/plane.obj");
    enemyPlane = LoadModel("src/assets/plane.obj");
    f16Cockpit = LoadModel("src/assets/cockpit.obj");
    map = LoadModel("src/assets/landscape.obj");
    skybox = LoadModel("src/assets/skybox.obj");
    plane = Plane(&f16, &f16Cockpit);
    enemy = Plane(&enemyPlane);
    target = LoadRenderTextureDepthTex(screenWidth, screenHeight);
    ringAimerMesh = GenMeshTorus(0.1, 0.5, 6, 12);
    ringAimer = LoadModelFromMesh(ringAimerMesh);


    
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Load render texture with a depth texture attached
    

    // Load depth shader and get depth texture shader location
    
    if (GLSL_VERSION == 100) depthShader = LoadShader(0, TextFormat("src/assets/depth_render_100.fs", GLSL_VERSION));
    else if (GLSL_VERSION == 330) depthShader = LoadShader(0, TextFormat("src/assets/depth_render_330.fs", GLSL_VERSION));
    depthLoc = GetShaderLocation(depthShader, "depthTexture");
    colorLoc = GetShaderLocation(depthShader, "colorTexture");
    
    int flipTextureLoc = GetShaderLocation(depthShader, "flipY");
    SetShaderValue(depthShader, flipTextureLoc, (int[]){ 1 }, SHADER_UNIFORM_INT); // Flip Y texture
    
    
    plane.position = {4000.0f, 300.0f, 4000.0f};

    
    enemy.position = {4000.0f, 300.0f, 4100.0f};
    enemy.front = {0.0f, 0.0f, -1.0f};
    
    for (int i = 0; i < 80; i += 1) {
        for (int j = 0; j < 80; j += 1) {
            heights[i][j] = PerlinMap.value(i*0.1, j*0.1) * 1000.0 + PerlinMapFiner.value(i, j) * 200.0;
        }
    }
    
    SetTargetFPS(60);

    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(Game::mainLoop, 0, 1);
    #endif
    #if defined(PLATFORM_DESKTOP)
        while (!WindowShouldClose()) {
            Game::mainLoop();
        }
    #endif

    UnloadModel(f16);
    UnloadModel(enemyPlane);
    UnloadModel(f16Cockpit);
    UnloadModel(map);
    UnloadModel(skybox);
    UnloadModel(ringAimer);
    
    UnloadRenderTextureDepthTex(target);
    UnloadShader(depthShader);      // Unload shader

    CloseWindow();
    
    return 0;
}


//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------
// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(0, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        // Create depth texture buffer (instead of raylib default renderbuffer)
        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       // DEPTH_COMPONENT_24BIT: Not defined in raylib
        target.depth.mipmaps = 1;

        // Attach color texture and depth texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Unload render texture from GPU memory (VRAM)
void UnloadRenderTextureDepthTex(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // Color texture attached to FBO is deleted
        rlUnloadTexture(target.texture.id);
        rlUnloadTexture(target.depth.id);

        // NOTE: Depth texture is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}
