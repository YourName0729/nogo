#pragma once

#include "weight.h"
#include "agent.h"

/**
 * base agent for agents with weight tables and a learning rate
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args), alpha(0), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
        std::cout << "all param\n";
        for (auto& [a, b] : meta) {
            std::cout << a << ' ' << static_cast<std::string>(b) << '\n';
        }
        
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end())
			save_weights(meta["save"]);
        if (meta.find("save") != meta.end())
			std::cout << "yes!\n";
        else std::cout << "no\n";
	}

protected:
	virtual void init_weights(const std::string& info) {
		std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		for (char& ch : res)
			if (!std::isdigit(ch)) ch = ' ';
		std::stringstream in(res);
		for (size_t size; in >> size; net.emplace_back(size));
	}
	virtual void load_weights(const std::string& path) {
		// std::cout << "load weight\n";
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
		// std::cout << "load weight check\n";
		// for (auto& wei : net) {
		// 	wei.check();
		// }
	}
	virtual void save_weights(const std::string& path) {
		// std::cout << "save weight\n";
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
		// std::cout << "save weight check\n";
		// for (auto& wei : net) {
		// 	wei.check();
		// }
	}

public:
	weight::type get_potential(const board& brd) const {
		weight::type val = 0;
		for (const auto& wei : net) {
			val += wei.get_weight(brd);
		}
		return val / static_cast<float>(net.size());
	}

	void update(const board& brd, weight::type err) {
		// std::cout << "overall err is " << err << '\n';
		weight::type nerr = err / static_cast<weight::type>(net.size());
		for (auto& wei : net) {
			// if (nerr != 0)
			// 	std::cout << "wei.update " << nerr << '\n';
			wei.update(brd, nerr);
		}
	}

protected:
	std::vector<weight> net;
	float alpha;
	board::piece_type who;
};

class ntuple : public weight_agent {
public:
	ntuple(const std::vector<weight::pattern>& pats, const std::string& args = "") : weight_agent("name=ntuple " + args) {
		// std::cout << "n_tuple_slider constructor\n";
		if (net.size()) {
			// std::cout << "only set pattern\n";
			for (unsigned int i = 0; i < net.size(); ++i) {
				net[i].set_pattern(pats[i]);
			}
		}
		else {
			// std::cout << "init new weight\n";
			for (const auto& pat : pats) {
				net.push_back(weight(pat));
			}	
		}
		// std::cout << "n_tuple const check\n";
		// for (auto& wei : net) {
		// 	wei.check();
		// }
	}

	virtual void close_episode(const std::string& flag = "") override {
		if (meta.find("learn") != meta.end() && property("learn") == "no_learn") {
			stats.clear();
			return;
		}
		learn(flag == name());
	}

	virtual action take_action(const board& before) override {
		// std::cout << "net size " << net.size() << '\n';
		// for (auto& wei : net) {
		// 	wei.check();
		// }
		weight::type best_value = -1e9;
		char best_drct = 5;

		// std::cout << "take action\n";

		stats.push_back({board(), 0});
		for (int i = 0; i < board::size_x * board::size_y; ++i) {
			board after = board(before);
            auto mv = action::place(i, who);
            if (mv.apply(after) != board::legal) continue;
			weight::type pot = get_potential(after);
			// std::cout << "drct " << i << " reward " << rew << " pot " << pot << '\n';
			if (best_drct == 5 || pot > best_value) {
				// std::cout << "new best " << pot << ' ' << rew << '\n';
				best_value = pot, best_drct = i;
				stats.back() = {after, pot};
			}
		}
		// std::cout << "best si drct " << best_drct << " with value " << best_value << '\n';

		// no valid move
		if (best_drct == 5) {
			return action();
		}
		// std::cout << "best is " << (int)best_drct << '\n';
		return action::place(best_drct, who);
	}

	void learn(bool win) {
		// if (stats[stats.size() - 2].after.max() <= 3) {
		// 	std::cout << "bad??\n";
		// 	for (auto& st : stats) {
		// 		st.after.show();
		// 		std::cout << '\n';
		// 	}
		// }
        float newv = static_cast<float>(win);
		for (stats.pop_back(); stats.size(); stats.pop_back()) {
			
			auto& cur = stats.back();
			// std::cout << get_potential(cur.after) << '\t';
			// if (new_pot != 0)
			// std::cout << new_pot << '\t';
			// std::cout << "the diff is " << new_pot - cur.pot << '\n';
			// if (new_pot - cur.pot > 0) {
			// 	std::cout << " + \t";
			// }
			// else {
			// 	std::cout << " - \t";
			// }
			update(cur.after, alpha * (newv - cur.pot));
			// std::cout << get_potential(cur.after) << '\n';
		}
	}

protected:
	struct stat {
		board after;
		weight::type pot;
	};

	std::vector<stat> stats;
};


class tuple3x3 : public ntuple {
public:
	tuple3x3(const std::string& args = "") : ntuple({
		{1, 2, 3, 10, 11, 12, 19, 20, 21},
        {2, 3, 4, 11, 12, 13, 20, 21, 22},
        {3, 4, 5, 12, 13, 14, 21, 22, 23},
        {4, 5, 6, 13, 14, 15, 22, 23, 24},
        {11, 12, 13, 20, 21, 22, 29, 30, 31},
        {12, 13, 14, 21, 22, 23, 30, 31, 32},
        {13, 14, 15, 22, 23, 24, 31, 32, 33},
        {21, 22, 23, 30, 31, 32, 39, 40, 41},
        {22, 23, 24, 31, 32, 33, 40, 41, 42},
        {31, 32, 33, 40, 41, 42, 49, 50, 51}
	}, "name=tuple3x3 " + args) {}
};