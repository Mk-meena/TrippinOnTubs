#include "RunningClock.h"

void trippin::RunningClock::init(const Configuration &config, const Map::Object &obj, const Sprite &spr) {
    SpriteObject::init(config, obj, spr);
    lane = -1;
    inactive = true;
    runningAcceleration = obj.runningAcceleration;
    frame = 0;
    channel.set({roundedPosition, frame});
    playedSound = false;
    sound = soundManager->getEffect("chime1");
}

void trippin::RunningClock::beforeTick(Uint32 engineTicks) {
    if (inactive && activation->shouldActivate(roundedBox)) {
        inactive = false;
    }
}

void trippin::RunningClock::afterTick(Uint32 engineTicks) {
    // early exit if not activated yet
    if (inactive) {
        return;
    }

    if (activation->shouldDeactivate(roundedBox)) {
        expired = true;
        return;
    }

    // Case #1: Goggin contact
    if (!hitGoggin && roundedBox.intersect(goggin->roundedBox)) {
        hitGoggin = true;
        hitTicks = 0;
        frame = FRAME_CLOUD_FIRST;
        spirit->delay(1);
        score->add(50);
    }

    // Case #2: Advance dust cloud
    else if (hitGoggin) {
        hitTicks++;
        if (hitTicks % (sprite->getFramePeriodTicks() * 2) == 0) {
            frame++;
        }
        if (frame == sprite->getFrames()) {
            expired = true;
        }
    }

    // Case #3: Advance flapping wings cycle
    else if (platformCollisions.testBottom() || objectCollisions.testBottom()) {
        acceleration.x = runningAcceleration;
        if (engineTicks % sprite->getFramePeriodTicks() == 0) {
            frame = (frame + 1) % FRAME_CLOUD_FIRST;
        }
    } else {
        acceleration.x = 0;
    }

    channel.set({roundedPosition, frame});
}

void trippin::RunningClock::setGoggin(const Goggin *g) {
    goggin = g;
}

void trippin::RunningClock::setSpirit(Spirit *sp) {
    spirit = sp;
}

void trippin::RunningClock::render(const trippin::Camera &camera) {
    auto ch = channel.get();
    if (ch.frame < sprite->getFrames()) {
        sprite->render(ch.roundedPosition, ch.frame, camera);
        if (ch.frame == FRAME_CLOUD_FIRST && !playedSound) {
            Mix_PlayChannel( -1, sound, 0 );
            playedSound = true;
        }
    }
}

void trippin::RunningClock::setActivation(const Activation *act) {
    activation = act;
}

void trippin::RunningClock::setScore(trippin::Score *sc) {
    score = sc;
}

void trippin::RunningClock::setSoundManager(trippin::SoundManager &sm) {
    soundManager = &sm;
}
