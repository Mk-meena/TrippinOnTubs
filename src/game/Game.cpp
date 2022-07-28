#include <iostream>
#include <sstream>
#include <map>
#include <chrono>
#include "SDL_syswm.h"
#include "SDL_mixer.h"
#include "Game.h"
#include "net/DbSynchronizer.h"
#include "UserInput.h"

void trippin::Game::init() {
    initConfiguration();
    initSdl();
    initAppId();
    initDbSynchronizer();
    initLogger();
    initScale();
    initSpriteManager();
    initAutoPlay();
    initOverlays();
    initLevel();
    initClock();
}

void trippin::Game::initSdl() {
    sdlSystem = std::make_unique<SdlSystem>(SDL_Point{configuration.windowSize.x, configuration.windowSize.y});
    windowSize = convertPoint(sdlSystem->getWindowSize());
    rendererSize = convertPoint(sdlSystem->getRendererSize());
}

void trippin::Game::initAppId() {
    auto now = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "T";
    ss << dur;
    ss << "N";
    ss << random.next();

    appId = ss.str();
    gameId = 0;

    SDL_Log("appId=%s", appId.c_str());
}

void trippin::Game::initLogger() {
    logger = std::make_unique<Logger>(*stagingArea, appId);
    auto renName = getRendererName(sdlSystem->getRenderer());
    auto sysName = getSystemName(sdlSystem->getWindow());
    auto sysRam = SDL_GetSystemRAM();
    std::stringstream ss;
    ss << "op=init";
    ss << ", rendererName=" << renName;
    ss << ", systemName=" << sysName;
    ss << ", systemRam=" << sysRam;
    ss << ", frameRate=" << sdlSystem->getRefreshRate();
    ss << ", winSize=(" << windowSize.x << "," << windowSize.y << ")";
    ss << ", renSize=(" << rendererSize.x << "," << rendererSize.y << ")";
    logger->log(ss.str());
}

void trippin::Game::initDbSynchronizer() {
    stagingArea = std::make_shared<StagingArea>();
    Transport transport(configuration.db.host, configuration.db.port);
    DbSynchronizer::startAddScoresThread(transport, stagingArea);
    DbSynchronizer::startAddLogEventsThread(transport, stagingArea);
    DbSynchronizer::startQueryScoresThread(std::move(transport), stagingArea);
}

void trippin::Game::initConfiguration() {
    configuration.load(configName);
}

void trippin::Game::initScale() {
    auto &cs = configuration.scales;
    auto sc = &cs[0];
    for (auto it = cs.rbegin(); it != cs.rend(); it++) {
        if (windowSize.x >= it->minWidth) {
            sc = &(*it);
            break;
        }
    }
    // using LCM to derive the engine factor ensures that conversions between base, device, and engine
    // scales in any direction always occur with whole numbers and facilitates simple int arithmetic
    auto lcm = std::accumulate(
            configuration.scales.begin(),
            configuration.scales.end(),
            1,
            [](int a, const Configuration::Scale &b) { return std::lcm(a, b.multiplier); });
    scale = std::make_unique<Scale>(sc->name, sc->multiplier, lcm);
    SDL_Log("width=%d, height=%d, scale=%s, deviceFactor=%d, engineFactor=%d, deviceEngineFactor=%d",
            windowSize.x, windowSize.y, sc->name.c_str(), sc->multiplier, lcm, scale->getDeviceEngineFactor());
    configuration.rescale(*scale);
}

void trippin::Game::initSpriteManager() {
    spriteLoader = std::make_unique<SpriteLoader>(*scale);
    spriteLoadTask = std::make_unique<SpriteLoadTask>(*spriteLoader, configuration.prefetchSprites);
    spriteManager = std::make_unique<SpriteManager>(sdlSystem->getRenderer(), *spriteLoader, configuration.msPerTick());
}

void trippin::Game::initAutoPlay() {
    autoPlay.load(configuration.loadAutoPlay);
}

void trippin::Game::initLevel() {
    levelIndex = 0;
    level = nextLevel();
    spriteLoadTask->start();
}

void trippin::Game::initOverlays() {
    auto scrollPixelsPerMs = -configuration.scrollPixelsPerSecond / 1'000.0;
    TitleOverlay::Options titleOptions{scrollPixelsPerMs, 3'000};
    titleOverlay = std::make_unique<TitleOverlay>(rendererSize, titleOptions, *spriteManager, renderClock);
    titleMenu = std::make_unique<TitleMenu>(rendererSize, *spriteManager, renderClock);
    endMenu = std::make_unique<EndMenu>(rendererSize, *spriteManager, renderClock);
    nameForm = std::make_unique<NameForm>(rendererSize, *spriteManager);
    scoreMenu = std::make_unique<ScoreMenu>(rendererSize, *spriteManager, renderClock);
    topScoreBoard = std::make_unique<ScrollingScoreBoard>(rendererSize, scrollPixelsPerMs, *spriteManager, renderClock);
    todayScoreBoard = std::make_unique<ScrollingScoreBoard>(rendererSize, scrollPixelsPerMs, *spriteManager, renderClock);
    levelOverlay = std::make_unique<LevelOverlay>(rendererSize, *spriteManager, renderClock);
    exitOverlay = std::make_unique<ExitOverlay>(rendererSize, configuration.meterMargin, *spriteManager, renderClock);
}

void trippin::Game::initClock() {
    renderClock.init();
    timer = std::make_unique<Timer>([this](int tps) {
        SDL_Log("timer=renderer, fps=%d, tpf=%s", tps, format(ticksPerFrame).c_str());
        ticksPerFrame.clear();
    });
}

std::unique_ptr<trippin::Level> trippin::Game::nextLevel() {
    auto lvl = std::make_unique<Level>();
    lvl->setWindowSize(rendererSize);
    lvl->setConfiguration(&configuration);
    lvl->setScale(scale.get());
    lvl->setSpriteManager(spriteManager.get());
    lvl->setSoundManager(&soundManager);
    lvl->setRenderClock(renderClock);
    lvl->setScore(score);
    lvl->setExtraLives(extraLives);
    if (state == State::TITLE || state == State::START_MENU) {
        lvl->setMapName(configuration.loadMap);
        lvl->setAutoPlay(autoPlay.events);
    } else if (state == State::TRAINING) {
        lvl->setMapName(configuration.trainMap);
        lvl->setTraining(true);
    } else if (state == State::REPLAY) {
        replayAutoPlay = convertEvents(replayScore.events[replayOffset]); // ensure extended lifetime for replay vec
        lvl->setAutoPlay(replayAutoPlay);
        lvl->setMapName(configuration.maps[levelIndex]);
        transferSurfaces();
    } else {
        lvl->setMapName(configuration.maps[levelIndex]);
        transferSurfaces();
    }
    lvl->init();
    return lvl;
}

void trippin::Game::advanceLevel() {
    lastTicks = -1; // represents no-last-ticks
    level->stop();
    level = nextLevel();
    level->start();
}

void trippin::Game::transferSurfaces() {
    if (spriteLoadTask->started() && !spriteLoadTask->joined()) {
        spriteLoadTask->join();
        spriteManager->setSurfaces(spriteLoadTask->take());
    }
}

trippin::Game::Game(std::string configName) : configName(std::move(configName)) {

}

void trippin::Game::start() {
    level->start();
    renderLoop();
}

void trippin::Game::renderLoop() {
    state = State::TITLE;

    UserInput ui(rendererSize);
    while (state != State::EXIT) {
        auto event = ui.pollEvent();
        if (!event) {
            continue;
        }

        if (event.quit) {
            level->stop();
            break;
        }

        auto gogginInput = event.asGogginInput();
        if (gogginInput) {
            level->onInput(gogginInput, state == State::REPLAY);
        }

        if (!renderClock.isPaused() && (event.pKeyDown || event.focusLost)) {
            renderClock.pause();
            level->pause();
        }

        if (renderClock.isPaused() && (event.rKeyDown || event.focusGained)) {
            renderClock.resume();
            level->resume();
        }

        if (renderClock.isPaused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // avoid spinning in a tight loop
        } else {
            handle(event);
        }

        if (event.render) {
            render();
        }
    }
}

std::string trippin::Game::format(const std::map<int, int> &map) {
    std::stringstream ss;
    ss << "[";
    for (auto i = map.begin(); i != map.end();) {
        auto cur = i++;
        ss << cur->first << "=" << cur->second;
        if (i != map.end()) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

const char *trippin::Game::getSystemName(SDL_Window *window) {
    SDL_SysWMinfo info;

    // snippet from https://wiki.libsdl.org/SDL_GetWindowWMInfo
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) {
        switch (info.subsystem) {
            case SDL_SYSWM_WINDOWS:
                return "MicrosoftWindows";
            case SDL_SYSWM_X11:
                return "XWindowSystem";
            case SDL_SYSWM_WINRT:
                return "WinRT";
            case SDL_SYSWM_DIRECTFB:
                return "DirectFB";
            case SDL_SYSWM_COCOA:
                return "AppleOSX";
            case SDL_SYSWM_UIKIT:
                return "UIKit";
            case SDL_SYSWM_WAYLAND:
                return "Wayland";
            case SDL_SYSWM_MIR:
                return "Mir";
            case SDL_SYSWM_ANDROID:
                return "Android";
            case SDL_SYSWM_VIVANTE:
                return "Vivante";
        }
    }

    return "unknown";
}

const char *trippin::Game::getRendererName(SDL_Renderer *renderer) {
    SDL_RendererInfo ri;
    if (SDL_GetRendererInfo(renderer, &ri) == 0) {
        return ri.name;
    }
    return "unknown";
}

void trippin::Game::logStateChange(const char *prev, const char *next) {
    using namespace std::string_literals;
    logger->log("op=state_change, prev="s + prev + ", next=" + next);
}

void trippin::Game::render() {
    renderClock.update();

    SDL_SetRenderDrawColor(sdlSystem->getRenderer(), 244, 251, 255, 255);
    SDL_RenderClear(sdlSystem->getRenderer());

    level->render();

    if (state == State::TITLE) {
        titleOverlay->render();
    } else if (state == State::START_MENU) {
        titleMenu->render();
    } else if (state == State::SCORE_MENU) {
        scoreMenu->render();
    } else if (state == State::ALL_TIME_SCORES) {
        topScoreBoard->render();
    } else if (state == State::TODAY_SCORES) {
        todayScoreBoard->render();
    } else if (state == State::LEVEL_TRANSITION) {
        levelOverlay->render();
    } else if (state == State::END_MENU) {
        endMenu->render();
    } else if (state == State::NAME_FORM) {
        nameForm->render();
    } else if (state == State::REPLAY) {
        exitOverlay->render();
    }

    SDL_RenderPresent(sdlSystem->getRenderer());

    auto ticks = level->getTicks();
    if (lastTicks != -1) {
        auto diff = ticks - lastTicks;
        ticksPerFrame[diff]++;
    }
    lastTicks = ticks;

    timer->next();
}

void trippin::Game::handle(UserInput::Event &event) {
    if (state == State::TITLE) {
        if (!titleOverlay->hasScores() && stagingArea->bothSet()) {
            titleOverlay->setScores(stagingArea->getTodayScores(15), stagingArea->getTopScores(15));
        }
        if (event.anythingPressed()) {
            titleMenu->reset();
            state = State::START_MENU;
            logStateChange("TITLE", "START_MENU");
        }
    } else if (state == State::START_MENU) {
        if (titleMenu->startClicked(event.touchPoint)) {
            extraLives = 1;
            levelIndex = 0;
            score = 0;
            gameId++;
            inputEvents.clear();
            state = State::PLAYING;
            advanceLevel();
            logStateChange("START_MENU", "PLAYING");
        } else if (titleMenu->trainClicked(event.touchPoint)) {
            score = 0;
            state = State::TRAINING;
            advanceLevel();
            logStateChange("START_MENU", "TRAINING");
        } else if (titleMenu->exitClicked(event.touchPoint)) {
            level->stop();
            state = State::EXIT;
        } else if (titleMenu->highScoreClicked(event.touchPoint)) {
            state = State::SCORE_MENU;
            scoreMenu->reset();
            logStateChange("START_MENU", "SCORE_MENU");
        }
    } else if (state == State::SCORE_MENU) {
        if (scoreMenu->exitClicked(event.touchPoint)) {
            titleMenu->reset();
            state = State::START_MENU;
            logStateChange("SCORE_MENU", "START_MENU");
        } else if (scoreMenu->allTimeClicked(event.touchPoint)) {
            state = State::ALL_TIME_SCORES;
            topScoreBoard->reset();
            topScoreBoard->setScores(stagingArea->getTopScores(25));
            logStateChange("SCORE_MENU", "ALL_TIME_SCORES");
        } else if (scoreMenu->todayClicked(event.touchPoint)) {
            state = State::TODAY_SCORES;
            todayScoreBoard->reset();
            todayScoreBoard->setScores(stagingArea->getTodayScores(25));
            logStateChange("SCORE_MENU", "TODAY_SCORES");
        }
    } else if (state == State::ALL_TIME_SCORES) {
        if (event.anythingPressed()) {
            auto scoreClicked = topScoreBoard->onClick(event.touchPoint);
            if (scoreClicked) {
                replayScore = *scoreClicked;
                replayOffset = 0;
                levelIndex = 0;
                score = 0;
                state = State::REPLAY;
                exitOverlay->reset();
                advanceLevel();
                logStateChange("ALL_TIME_SCORES", "REPLAY");
            } else {
                titleMenu->reset();
                state = State::START_MENU;
                logStateChange("ALL_TIME_SCORES", "START_MENU");
            }
        }
    } else if (state == State::TODAY_SCORES) {
        if (event.anythingPressed()) {
            titleMenu->reset();
            state = State::START_MENU;
            logStateChange("TODAY_SCORES", "START_MENU");
        }
    } else if (state == State::REPLAY) {
        auto endFn = [this] {
            titleMenu->reset();
            state = State::START_MENU;
            logStateChange("REPLAY", "START_MENU");
        };
        if (event.anythingPressed()) {
            if (exitOverlay->exitClicked(event.touchPoint)) {
                endFn();
            }
        }
        if (level->ended()) {
            score = level->getScore(); // capture score to carry into next level
            if (level->completed()) {
                if (levelIndex < configuration.maps.size() - 1 && replayOffset < replayScore.events.size() - 1) {
                    replayOffset++; // advance replay events on level change
                    levelIndex++;
                    advanceLevel();
                } else {
                    endFn();
                }
            } else {
                if (extraLives > 0) {
                    replayOffset++; // advance replay events on level restart
                    extraLives--;
                    advanceLevel();
                } else {
                    endFn();
                }
            }
        }
    } else if (state == State::PLAYING) {
        if (level->ended()) {
            score = level->getScore();
            inputEvents.push_back(level->takeInputEvents());
            if (level->completed()) {
                state = State::LEVEL_TRANSITION;
                levelOverlay->setLevel(static_cast<int>(levelIndex));
                logStateChange("PLAYING", "LEVEL_TRANSITION");
            } else {
                if (extraLives > 0) {
                    extraLives--;
                    state = State::EXTRA_LIFE_DELAY;
                    auto now = std::chrono::steady_clock::now();
                    extraLifeTime = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    logStateChange("PLAYING", "EXTRA_LIFE_DELAY");
                } else {
                    state = State::END_MENU;
                    endMenu->reset();
                    logStateChange("PLAYING", "END_MENU");
                }
            }
        }
    } else if (state == State::EXTRA_LIFE_DELAY) {
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
        if (now - extraLifeTime >= std::chrono::seconds{1}) {
            state = State::PLAYING;
            advanceLevel();
            logStateChange("EXTRA_LIFE_DELAY", "PLAYING");
        }
    } else if (state == State::TRAINING) {
        if (level->completed()) {
            score = 0;
            state = State::START_MENU;
            advanceLevel();
            logStateChange("TRAINING", "START_MENU");
        }
    } else if (state == State::LEVEL_TRANSITION) {
        if (event.anythingPressed()) {
            if (levelIndex < configuration.maps.size() - 1) {
                levelIndex++;
                state = State::PLAYING;
                advanceLevel();
                logStateChange("LEVEL_TRANSITION", "PLAYING");
            } else {
                state = State::END_MENU;
                endMenu->reset();
                logStateChange("LEVEL_TRANSITION", "END_MENU");
            }
        }
    } else if (state == State::END_MENU) {
        if (endMenu->exitClicked(event.touchPoint)) {
            titleMenu->reset();
            state = State::START_MENU;
            logStateChange("END_MENU", "START_MENU");
        } else if (endMenu->saveClicked(event.touchPoint)) {
            state = State::NAME_FORM;
            nameForm->reset();
            logStateChange("END_MENU", "NAME_FORM");
        }
    } else {
        nameForm->onClick(event.touchPoint);
        if (nameForm->nameEntered()) {
            stagingArea->addScore({appId, gameId, score, nameForm->getName(), convertInputEvents()});
            state = State::START_MENU;
            titleMenu->reset();
            logStateChange("NAME_FORM", "START_MENU");
        }
    }
}

std::vector<std::vector<trippin::Score::InputEvent>> trippin::Game::convertInputEvents() const {
    std::vector<std::vector<Score::InputEvent>> result;
    for (auto &vec: inputEvents) {
        std::vector<Score::InputEvent> v;
        for (auto &e: vec) {
            v.push_back(Score::InputEvent::fromFlags(e.tick, e.jumpCharge, e.jumpRelease, e.duckStart, e.duckEnd));
        }
        result.push_back(v);
    }
    return result;
}

std::vector<trippin::GogginInputTick> trippin::Game::convertEvents(const std::vector<Score::InputEvent> &events) {
    std::vector<GogginInputTick> result;
    for (auto &e: events) {
        result.push_back({{e.jumpCharge(), e.jumpRelease(), e.duckStart(), e.duckEnd()}, e.tick});
    }
    return result;
}
