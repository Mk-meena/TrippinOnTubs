#include "Ground.h"
#include "lock/AutoGuard.h"

void trippin::Ground::init(const Configuration &config, const Map::Object &obj, const Sprite &spr) {
    SpriteObject::init(config, obj, spr);
    channel = {roundedPosition, 0, false};
    gChannel.set(channel);
    melting = false;
    inactive = true;
}

void trippin::Ground::beforeTick(Uint32 engineTicks) {
    if (inactive && activation->shouldActivate(roundedBox)) {
        inactive = false;
    }
}

void trippin::Ground::afterTick(Uint32 engineTicks) {
    AutoGuard<Channel> ag{channel, gChannel};

    ticks++;
    if (!melting && position.x <= spirit->getPosition()) {
        melting = true;
        meltingTick = ticks;
    }
    if (melting) {
        auto diff = ticks - meltingTick;
        if (diff % sprite->getFramePeriodTicks() == 0 && channel.frame < sprite->getFrames()) {
            channel.frame++;
        }
        if (channel.frame == sprite->getFrames() - 1) {
            channel.expired = expired = true;
        }
    }
}

void trippin::Ground::render(const trippin::Camera &camera) {
    auto ch = gChannel.get();
    if (!ch.expired) {
        sprite->render(ch.roundedPosition, ch.frame, camera);
    }
}

void trippin::Ground::setSpirit(const trippin::Spirit *sp) {
    spirit = sp;
}

void trippin::Ground::setActivation(const Activation *act) {
    activation = act;
}