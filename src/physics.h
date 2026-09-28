#ifndef PHYSICS
#define PHYSICS

#include "raylib.h"
#include "raymath.h"
#include <random>
#include <iostream>

Vector3 rotateVectorAroundAxis(const Vector3& vector, Vector3 axis, double radians) {
    axis = Vector3Normalize(axis);
    double theta = Vector3Angle(axis, vector);
    double radius = Vector3Length(vector);
    Vector3 vertical = Vector3Scale(axis, radius * cos(theta));
    Vector3 flat = vector - vertical;
    Vector3 flatPerpendicular = Vector3CrossProduct(axis, flat);
    Vector3 resultant = Vector3Scale(flat, cos(radians)) + Vector3Scale(flatPerpendicular, sin(radians)) + vertical;
    return resultant;
}

void resetToZero(float& input, float step) {
    if (std::abs(input) < step) input = 0;
    else if (input < 0) input += step;
    else input -= step;
}

class ControlSurface {
    public: 
    float deflection = 0.0f;
    float maxDeflection = 0.0f;
    ControlSurface(float max): maxDeflection(max) {}
    void turn(float direction) {
        deflection += maxDeflection/35.0f * static_cast<float>(direction);
        deflection = Clamp(deflection, -maxDeflection, maxDeflection);
    }
    void reset() {
        resetToZero(deflection, maxDeflection/35.0f);
    }
};

class Plane {
    public:
    Vector3 position;
    Vector3 velocity;
    Vector3 front {0, 0, 1};
    Vector3 up {0, 1, 0};
    Model* model;
    Model* cockpitModel;

    ControlSurface elevator {DEG2RAD*20.0f};
    ControlSurface aileron {DEG2RAD*240.0f};
    ControlSurface rudder {DEG2RAD*9.0f};

    Plane(Model* model, Model* cockpitModel): model(model), cockpitModel(cockpitModel) {
        velocity = {0, 0, 1.0f};
        position = (Vector3){ 0.0f, 20.0f, 0.0f };
    }
    Plane(Model* model): Plane(model, nullptr) {}
    Plane(): Plane(nullptr, nullptr) {}
    
    void normalizeOrientationVectors() {
        front = Vector3Normalize(front);
        up = Vector3Normalize(up);
    }
    void update() {
        velocity = Vector3Scale(front, Vector3Length(velocity));
        position = position + velocity;

        pitch(elevator.deflection / 60.0f);
        roll(aileron.deflection / 60.0f);
        yaw(rudder.deflection / 60.0f);

        //transform matrix can be formed directly from orientation vectors as columns (orientation vectors form x, y, z axes)
        //as opposed to using MatrixRotateXYZ from euler angles (suffers from gimbal lock)

        model->transform = {
            right().x, up.x, front.x, 0.0f,
            right().y, up.y, front.y, 0.0f,
            right().z, up.z, front.z, 0.0f,
            0.0f,      0.0f, 0.0f,    1.0f,
        };
        if (cockpitModel) cockpitModel->transform = model->transform;
    }
    void attack(const Plane& target) {
        float rollSpeed = 0.06f, pitchSpeed = DEG2RAD*16.0f/60.0f;
        Vector3 relative = target.position - position;
        float angleToTarget = Vector3Angle(relative, front);
        if (angleToTarget < 0.01f) return;
        float targetAngleToSelf = Vector3Angle(Vector3Negate(relative), target.front);
        float horizontal = Vector3DotProduct(relative, right());
        float vertical = Vector3DotProduct(relative, up);
        float forward = Vector3DotProduct(relative, front);
        float horizontalAngle = atan2(horizontal, vertical);
        float verticalAngle = atan2(vertical, forward);
        if ((abs(angleToTarget) < PI*0.85f && targetAngleToSelf > angleToTarget / 5.0f) || Vector3Length(relative) < Vector3Length(target.velocity)*60.0f) { //check that he isnt on our 6 to engage in 1 circle, otherwise we ditch out to 2 circle and try again
            if (abs(horizontalAngle) <= rollSpeed) { //is our front-vertical plane aligned with him to where we can start pitching towards him
                roll(horizontalAngle);
                pitch(std::min(verticalAngle, pitchSpeed));
            } else {
                if (horizontal > 0) {//roll towards him without pitching
                    roll(rollSpeed);
                } else (roll(-rollSpeed));
            }
        } else {
            pitch(pitchSpeed);
        }
        
    }
    Vector3 right() const {
        return Vector3CrossProduct(front, up);
    }
    void roll(double angle) {
        up = rotateVectorAroundAxis(up, front, angle);
        normalizeOrientationVectors();
    }
    void pitch(double angle) {
        Vector3 rightVector = right();
        front = rotateVectorAroundAxis(front, rightVector, angle);
        up = rotateVectorAroundAxis(up, rightVector, angle);
        normalizeOrientationVectors();
    }
    void yaw(double angle) {
        front = rotateVectorAroundAxis(front, up, -angle);
        normalizeOrientationVectors();
    }
};

template<int N>
class PerlinNoise {
    std::vector<std::vector<Vector2>> GVA {};//gradient vector angles
    public:
    PerlinNoise() {
        std::random_device rd;
        std::mt19937 gen(rd());
        
        std::uniform_real_distribution<float> dis(0.0f, 2.0f*PI);
        
        for (int i = 0; i < N; ++i) {
            GVA.push_back(std::vector<Vector2>{});
            for (int j = 0; j < N; ++j) {
                float angle = dis(gen);
                GVA[i].push_back(Vector2{cos(angle), sin(angle)});
            }
        }
    }
    float value(float x, float y) {
        int left = static_cast<int>(floor(x)) % N, right = static_cast<int>((floor(x)+1)) % N, top = static_cast<int>(floor(y)) % N, bottom = static_cast<int>((floor(y)+1)) % N;
        x -= floor(x);
        y -= floor(y);
        float topLeftInfluence = Vector2DotProduct(GVA[top][left], {x, y});
        float topRightInfluence = Vector2DotProduct(GVA[top][right], {x-1.0f, y});
        float bottomLeftInfluence = Vector2DotProduct(GVA[bottom][left], {x, y-1.0f});
        float bottomRightInfluence = Vector2DotProduct(GVA[bottom][right], {x-1.0f, y-1.0f});
        float topInfluence = Lerp(topLeftInfluence, topRightInfluence, x);
        float bottomInfluence = Lerp(bottomLeftInfluence, bottomRightInfluence, x);
        float finalValue = Lerp(topInfluence, bottomInfluence, y) * sqrt(2.0f);
        return finalValue;
    }
};

//returns the location of the collision relative to where the target is right now
Vector3 leadAngleCalculation(Vector3 relative, Vector3 vTarget, float bulletSpeed) {
    float relativeVelocityAngle = Vector3Angle(relative, vTarget);
    float launchAngle = asin((Vector3Length(vTarget)/bulletSpeed) * sin(relativeVelocityAngle));
    float collisionAngle = PI-relativeVelocityAngle-launchAngle;
    return Vector3Normalize(vTarget) * Vector3Length(relative) * sin(launchAngle)/sin(collisionAngle);
}

#endif
