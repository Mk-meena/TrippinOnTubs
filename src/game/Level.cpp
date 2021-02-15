#include "Level.h"
#include "Ground.h"
#include "PacingObject.h"
#include "Bird.h"
#include "Ball.h"
#include "WingedTub.h"
#include "RunningClock.h"
#include "Layer.h"
#include "engine/Convert.h"

void trippin::Level::setWindowSize(trippin::Point<int> ws) {
    windowSize = ws;
}

void trippin::Level::setConfiguration(Configuration *conf) {
    configuration = conf;
}

void trippin::Level::setScale(trippin::Configuration::Scale *sc) {
    scale = sc;
}

void trippin::Level::setSpriteManager(trippin::SpriteManager *sm) {
    spriteManager = sm;
}

void trippin::Level::initMap() {
    map.load(configuration->map);
    map.rescale(scale->multiplier);
    map.convert(configuration->tickPeriod);
}

void trippin::Level::initCamera() {
    camera.setViewport({0, 0, windowSize.x, windowSize.y});
    camera.setUniverse({0, 0, map.universe.x, map.universe.y});
}

void trippin::Level::initEngine() {
    engine.setTickPeriod(configuration->tickPeriod);
    engine.setPlatformCollision(&platformCollision);
    engine.setObjectCollision(&objectCollision);
    engine.addListener(&spirit);

    for (auto &layer : map.layers) {
        auto uptr = std::make_unique<Layer>();
        uptr->init(*spriteManager, layer);
        objects.push_back(std::move(uptr));
    }

    for (auto &obj : map.objects) {
        if (obj.type == "goggin") {
            goggin.setDust(spriteManager->get("dust"));
            goggin.setDustBlast(spriteManager->get("dust_blast"));
            goggin.init(*configuration, obj, spriteManager->get(obj.type));
            engine.add(&goggin);
        } else if (obj.type.rfind("ground_melt_", 0) == 0 || obj.type.rfind("platform", 0) == 0) {
            auto uptr = std::make_unique<Ground>();
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            uptr->setSpirit(&spirit);
            uptr->setActivation(&activation);
            engine.add(uptr.get());
            objects.push_back(std::move(uptr));
        } else if (obj.type == "winged_tub") {
            auto uptr = std::make_unique<WingedTub>();
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            uptr->setGoggin(&goggin);
            uptr->setScore(&score);
            uptr->setActivation(&activation);
            engine.addListener(uptr.get());
            objects.push_back(std::move(uptr));
        } else if (obj.type == "running_clock") {
            auto uptr = std::make_unique<RunningClock>();
            uptr->setGoggin(&goggin);
            uptr->setSpirit(&spirit);
            uptr->setActivation(&activation);
            uptr->setScore(&score);
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            engine.add(uptr.get());
            objects.push_back(std::move(uptr));
        } else if (obj.type == "zombie" || obj.type == "rat") {
            auto uptr = std::make_unique<PacingObject>();
            uptr->setActivation(&activation);
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            engine.add(uptr.get());
            objects.push_back(std::move(uptr));
        } else if (obj.type == "bird") {
            auto uptr = std::make_unique<Bird>();
            uptr->setActivation(&activation);
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            engine.add(uptr.get());
            objects.push_back(std::move(uptr));
        } else if (obj.type == "ball") {
            auto uptr = std::make_unique<Ball>();
            uptr->setActivation(&activation);
            uptr->init(*configuration, obj, spriteManager->get(obj.type));
            engine.add(uptr.get());
            objects.push_back(std::move(uptr));
        }
    }

    score.setSprite(spriteManager->get("digits"));
    score.setMargin(map.meterMargin);
    score.setPointsPerTick(configuration->pointsPerTick());
    score.init();
    engine.addListener(&score);

    spirit.setTicksPerSecond(configuration->ticksPerSecond());
    spirit.setVelocity(goggin.terminalVelocity.x);
    spirit.delay(configuration->spiritSecondsBehind);

    auto &timerSprite = spriteManager->get("clock_timer");
    spiritClock.setGoggin(goggin);
    spiritClock.setSpirit(spirit);
    spiritClock.setPosition({map.meterMargin, map.meterMargin});
    spiritClock.init(*configuration, timerSprite);
    engine.addListener(&spiritClock);

    jumpMeter.setSprite(spriteManager->get("winged_foot"));
    jumpMeter.setGoggin(goggin);
    jumpMeter.setPosition({timerSprite.getSize().x + map.meterMargin * 2, map.meterMargin});
    jumpMeter.init();
    engine.addListener(&jumpMeter);

    activation.setUniverse(map.universe);
    activation.setActivationProximity(toInt(configuration->activationProximity * scale->multiplier));
    activation.setDeactivationProximity(toInt(configuration->deactivationProximity * scale->multiplier));
    activation.setGoggin(&goggin);
}

void trippin::Level::init() {
    initMap();
    initCamera();
    initEngine();
}

void trippin::Level::render(Input input) {
    if (input.spaceKeyDown) {
        goggin.onJumpCharge();
    }
    if (input.spaceKeyUp) {
        goggin.onJumpRelease();
    }
    if (input.downKeyDown) {
        goggin.onDuckStart();
    }
    if (input.downKeyUp) {
        goggin.onDuckEnd();
    }

    goggin.centerCamera(camera);
    for (auto &obj : objects) {
        obj->render(camera);
    }

    goggin.render(camera);
    spiritClock.render(camera);
    jumpMeter.render(camera);
    score.render(camera);
}

void trippin::Level::start() {
    engine.start();
}


