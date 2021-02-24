#include "SpiritClock.h"
#include "lock/AutoGuard.h"
#include "engine/Convert.h"

void trippin::SpiritClock::init(const Configuration &config, const Sprite &spr) {
    engineTicksPerClockBar = config.engineTicksPerSpiritClockTick();
    sprite = &spr;
    gChannel.set({sprite->getFrames() - 1});
}

void trippin::SpiritClock::setSpirit(const trippin::Spirit &sp) {
    spirit = &sp;
}

void trippin::SpiritClock::setGoggin(const trippin::Goggin &go) {
    goggin = &go;
}

void trippin::SpiritClock::setPosition(trippin::Point<int> pos) {
    position = pos;
}

void trippin::SpiritClock::render(const trippin::Camera &camera) {
    sprite->render(position, gChannel.get().frame);
}

void trippin::SpiritClock::afterTick(Uint32 engineTicks) {
    auto distanceAway = goggin->position.x - spirit->getPosition();
    auto ticksAway = distanceAway / spirit->getVelocity();
    auto numClockBars = sprite->getFrames() - 1;
    auto barsAway = toInt(ticksAway / engineTicksPerClockBar);
    gChannel.set({std::min(numClockBars, std::max(0, barsAway))});
}
