#ifndef TRIPPIN_TRANSPORT_H
#define TRIPPIN_TRANSPORT_H

#include <string>
#include <vector>
#include "Score.h"
#include "LogEvent.h"
#include "nlohmann/json.hpp"

namespace trippin {
    class Transport {
    public:
        struct Scores {
            std::vector<Score> scores;
            bool ok{};
        };

        Transport(std::string host, int port);
        bool addScore(const Score &score) const;
        bool addLogEvent(const LogEvent &event) const;
        Scores topScores() const;
        Scores todayScores() const;
    private:
        std::string host;
        int port;
        Scores sendRequest(const std::string &uri) const;
    };

    void from_json(const nlohmann::json &j, Score &score);
    void from_json(const nlohmann::json &j, Score::InputEvent &evt);
    void to_json(nlohmann::json &j, const Score::InputEvent &evt);
}

#endif
