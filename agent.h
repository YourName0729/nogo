/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include "board.h"
#include "action.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
		else
			engine.seed(std::random_device()());
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random player for both side
 * put a legal piece randomly
 */
class random_player : public random_agent {
public:
	random_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) override {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};


class monkey : public random_agent {
public:
	monkey(const std::string& args = "") : random_agent("name=monkey role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) override {
		std::shuffle(space.begin(), space.end(), engine);
		return space[0];
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};

class mcts : public agent {
public:
	mcts(const std::string& args = "") : agent("name=mcts role=unknown " + args), who(board::empty), timeout(1000),
		sim{random_player("name=white role=white"), random_player("name=black role=black"), random_player("name=white role=white")} {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		if (meta.find("timeout") != meta.end()) timeout = static_cast<int64_t>(meta["timeout"]);
	}

private:
	struct node {
		node() : q(0), n(1e-6), qs(0), ns(1e-6) {}
		float q, n, qs, ns;
		std::vector<std::pair<action::place, node*>> chs;
	};

	struct hasher {
		std::size_t operator()(const action::place& mv) const {
			return std::hash<unsigned>()(static_cast<unsigned>(mv));
		}
	};

	using action_set = std::unordered_map<action::place, unsigned, hasher>;
	using mnpair = std::pair<action::place, node*>;
	using mnpair_ptr = std::vector<mnpair>::iterator;

public:
	void make_node(node** cur, const board& state, board::piece_type rl) {
		// std::cout << "make node\n";
		*cur = new node();
		for (unsigned int i = 0; i < board::size_x * board::size_y; ++i) {
			action::place mv = action::place(i, rl);
			board after = state;
			if (mv.apply(after) == board::legal) (*cur)->chs.emplace_back(mv, nullptr);
		}
	}

	void delete_node(node* cur) {
		for (auto& [mv, nd] : cur->chs) if (nd != nullptr) delete_node(nd);
		delete cur;
	}

	inline board::piece_type switch_role(board::piece_type r) const {
		return static_cast<board::piece_type>(static_cast<unsigned int>(r) ^ 3u);
	}

	mnpair_ptr branch(node* cur, board::piece_type rl) {
		// std::cout << "branch\n";
		auto& vec = cur->chs;
		if (vec.empty()) return vec.end();
		float scl = (rl == who)? 1.f : -1.f;

		// find maximum
		float mx = -1e9;
		auto mxnd = vec.begin();
		for (auto it = vec.begin(); it != vec.end(); it++) {
			node* nd = it->second;
			if (it->second == nullptr) return it;
			float beta = std::sqrt(10.f / (nd->n + 10.f));
			
			float score = scl * ((1 - beta) * nd->q + beta * nd->qs) + std::sqrt(std::log(cur->n) / nd->n);
			// float score = scl * (nd->q) + std::sqrt(std::log(cur->n) / nd->n);
			if (mx == -1e9 || mx < score) mx = score, mxnd = it;
		}
		return mxnd;

		return vec.end();
	}

	board::piece_type simulation(const board& state, action_set& rave) {
		// std::cout << "simulation\n";
		// random simulation, return the win
		board after = state;
		action::place mv;
		board::piece_type rl;
		do {
			rl = after.info().who_take_turns;
			mv = sim[static_cast<unsigned>(rl)].take_action(after);
			if (rl == who) ++rave[mv];
		} while (mv.apply(after) == board::legal);
		// after.info().who_take_turns loses
		return switch_role(after.info().who_take_turns);
	}

	void backtrack(float win, const std::vector<mnpair_ptr>& ancs, action_set& rave, node* root) {
		// std::cout << "backtrack\n";
		auto update_rave = [&](node* cur, node* avd) {
			for (auto [mv, nd] : cur->chs) {
				if (nd != nullptr && nd != avd && rave.find(mv) != rave.end()) {
					nd->ns += rave[mv];
					nd->qs += rave[mv] * (win - nd->qs) / nd->ns;
				}
			}
		};

		for (const auto& ptr : ancs) {
			auto cur = ptr->second;
			++cur->n;
			cur->q += (win - cur->q) / cur->n;
		}
		++root->n;
		root->q += (win - root->q) / root->n;
		for (int i = static_cast<int>(ancs.size()) - 2; i >= 0; --i) {
			update_rave(ancs[i]->second, ancs[i + 1]->second);
			++rave[ancs[i + 1]->first];
		}
		if (ancs.size()) update_rave(root, ancs.front()->second);
	}

	virtual action take_action(const board& state) override {
		// std::cout << "mcts take action\n";
		// state.show();
		node* root = nullptr;
		auto begin = std::chrono::steady_clock::now();
		// int i = 0, T = 100;
		do {
			node** cur = &root;
			std::vector<mnpair_ptr> ancs;
			board::piece_type rl = who;
			board after = state;
			// std::string path = ">";
			while ((*cur) != nullptr) {
				// std::cout << "dep\n";
				// find a best child
				// std::cout << path << " " << (*cur)->n << ' ' << (*cur)->q << '\n';
				// path += '>';
				auto br = branch(*cur, rl);
				ancs.push_back(br);
				if (br == (*cur)->chs.end()) break;
				cur = &br->second;
				rl = switch_role(rl);
				br->first.apply(after);
			}
			// std::cout << path << '\n';

			if (*cur != nullptr) {
				// std::cout << "terminal node\n";
				// cur is a termminal node (lose)
				ancs.pop_back();
				action_set rave;
				backtrack(rl != who, ancs, rave, root);
				continue;
			}
			
			// create node for cur
			make_node(cur, after, rl);
			// simulation from cur
			action_set rave;
			auto win = simulation(after, rave);
			// if (win == who) std::cout << "root win!\n";
			// else std::cout << "root loses...\n";
			backtrack(win == who, ancs, rave, root);
		// } while (++i < T);
		} while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() <= timeout);


		auto mx = std::max_element(root->chs.begin(), root->chs.end(), [](const mnpair& a, const mnpair& b) {
			if (a.second == nullptr) return true; // a < b
			if (b.second == nullptr) return false; // b < a
			return a.second->q < b.second->q;
		});
		if (mx == root->chs.end() || mx->second == nullptr) {
			delete_node(root);
			return action();
		}
		// std::cout << "my action is " << mx->first.position() << ' ' << mx->second->q << ' ' << mx->second->n << '\n';
		// board tmp = state;
		// mx->first.apply(tmp);
		// tmp.show();
		// std::this_thread::sleep_for(std::chrono::seconds(1));
		auto mv = mx->first;
		delete_node(root);
		// std::cout << (" BW"[(int)who]) << " -> " << mx->first.position() << '\n';
		return mv;
	}

private:
	
	board::piece_type who;
	int64_t timeout;
	random_player sim[3];
};