#pragma once

#include <memory>
#include <sstream>
#include "agent.h"

class agent_factory {
public:
    static std::shared_ptr<agent> produce(const std::string& args, const std::string& role) {
        std::stringstream ss(args);
        std::string name, nargs;
        for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
            if (key == "name") name = value;
            else nargs += key + '=' + value + ' ';
		}

        std::string oargs = "name=" + role + " " + nargs + " role=" + role;
        if (name == "random") return std::make_shared<random_player>(oargs);
        if (name == "mcts") return std::make_shared<mcts>(oargs);
        if (name == "monkey") return std::make_shared<monkey>(oargs);

        return std::make_shared<random_player>(oargs);
    }

};