#ifndef TRIPPIN_ENGINE_H
#define TRIPPIN_ENGINE_H

#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "SDL.h"
#include "engine/Object.h"
#include "engine/Collisions.h"

namespace trippin {
    // Engine handles the movement and interaction of objects.
    class Engine {
    public:
        // Add an object to the engine. This is a weak reference.
        // The object ought to out-live the engine.
        void add(Object *object);

        // Add non-object engine listener.
        void addListener(Listener *listener);

        // Sets the default type of collision between platform and non-platform objects.
        // This collision type can be overridden by individual objects.
        void setPlatformCollision(std::function<void(Object &, Object &, const Sides &)> collision);

        // Sets the default type of collision between objects.
        // This collision type can be overridden by individual objects.
        void setObjectCollision(std::function<void(Object &, Object &, const Sides &)> collision);

        // Set the engine ticks per rendered frame
        // The engine will synchronize its update thread with the render thread
        void setTicksPerFrame(double ticksPerFrame);

        // Advance the engine simulation by one tick. In each tick:
        // (1) apply motion to objects and snap to grid
        // (2) detect intersections and snap objects
        // (3) detect and react to collisions
        void tick(Uint32 engineTicks);

        void runEngineLoop();

        void start();
        void stop();
        void join();
        int getTicks() const;

        // Notifies engine that render has occurred so engine thread can advance accordingly
        void onRender();
    private:
        std::vector<Object *> inactive;
        std::vector<Object *> platforms;
        std::vector<Object *> objects;
        std::vector<Listener *> listeners;

        std::function<void(Object &, Object &, const Sides &)> platformCollision = onAbsorbentCollision;
        std::function<void(Object &, Object &, const Sides &)> objectCollision = onInelasticCollisionDefault;

        std::thread thread;
        std::atomic_bool stopped{};
        std::atomic_int ticks{};
        int renders{};
        std::mutex mutex{};
        std::condition_variable cv{};
        double ticksPerFrame{};

        void beforeTick(Uint32 engineTicks);
        void afterTick(Uint32 engineTicks);
        void promoteActive();
        void removeExpired();
        void applyMotion();
        void snapObjects();
        Object *nextObjectToSnap();
        static bool hasHigherSnapPriorityThan(Object *left, Object *right);
        void snapToPlatform(Object *obj);
        void applyPhysics();
        static void snapTo(Object &obj, const Object &p, const trippin::Rect<int> &overlap);
        void applyPlatformCollision(Object &object, Object &platform, const Sides &sides);
        void applyObjectCollision(Object &left, Object &right, const Sides &sides);

        static bool sameLane(Object *left, Object *right);
    };
}

#endif
