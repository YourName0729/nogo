#pragma once

#include <chrono>
#include <thread>
#include <unordered_set>
#include <thread>

#include "agent.h"

class mcts : public agent {
public:
	mcts(const std::string& args = "") : agent("name=mcts role=unknown " + args), who(board::empty), T(100000), c(1.5), k(10), thread(1), demo(false) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		if (meta.find("T") != meta.end()) T = static_cast<int>(meta["T"]);
		if (meta.find("c") != meta.end()) c = static_cast<float>(meta["c"]);
		if (meta.find("k") != meta.end()) k = static_cast<float>(meta["k"]);
		if (meta.find("thread") != meta.end()) thread = static_cast<unsigned>(meta["thread"]);
		if (meta.find("demo") != meta.end()) demo = true;

		for (unsigned i = 0; i < thread; ++i) {
			sims.push_back({random_player("name=white role=white"), random_player("name=black role=black"), random_player("name=white role=white")});
		}
	}

private:
	struct node {
		node() : q(0), n(1e-6), qs(0), ns(1e-6) {}
		float q, n, qs, ns;
		std::vector<std::pair<action::place, node*>> chs;
	};

	struct hasher {
		std::size_t operator()(const action::place& mv) const {
			return static_cast<unsigned>(mv);
		}
	};

	struct equaler {
		bool operator()(const action::place& a, const action::place& b) const {
			return static_cast<unsigned>(a) == static_cast<unsigned>(b);
		}
	};

	using action_set = std::unordered_set<action::place, hasher, equaler>;
	using mnpair = std::pair<action::place, node*>;
	using mnpair_ptr = std::vector<mnpair>::iterator;

public:
	virtual void close_episode(const std::string& flag = "") override {
	    round = 0;
		if (demo) {
			std::cout << name() << " do " << cnt << " moves in " << telp << " ms\n";
			telp = 0;
			cnt = 0;	
		}
	}
	// virtual void close_episode(const std::string& flag = "") override {
	// 	std::cout << cnt << " moves in " << telp << " ms\n";
	// 	telp = 0;
	// 	cnt = 0;
	// }

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
			float beta = std::sqrt(k / (nd->n + k));
			
			// float score = scl * ((1 - beta) * nd->q + beta * nd->qs) + c * std::sqrt(std::log(cur->n) / nd->n);
			float v = nd->q * (1 - nd->q) + std::sqrt(2 * std::log(cur->n) / nd->n);
			float score = scl * ((1 - beta) * nd->q + beta * nd->qs) + c * std::sqrt(std::log(cur->n) / nd->n * std::min(0.75f, v));
			// float score = scl * (nd->q) + std::sqrt(std::log(cur->n) / nd->n);
			if (mx == -1e9 || mx < score) mx = score, mxnd = it;
		}
		return mxnd;

		return vec.end();
	}

	board::piece_type simulation(const board& state, action_set& rave, std::vector<random_player>& sim) {
		// std::cout << "simulation\n";
		// random simulation, return the win
		board after = state;
		action::place mv;
		board::piece_type rl;
		do {
			rl = after.info().who_take_turns;
			mv = sim[static_cast<unsigned>(rl)].take_action(after);
			//if (rl == who) ++rave[mv];
			rave.insert(mv);
		} while (mv.apply(after) == board::legal);
		// after.info().who_take_turns loses
		return switch_role(after.info().who_take_turns);
	}

	void backtrack(float win, const std::vector<mnpair_ptr>& ancs, action_set& rave, node* root) {
		// std::cout << "backtrack\n";
		auto update_rave = [&](node* cur, node* avd) {
			for (auto [mv, nd] : cur->chs) {
				if (nd != nullptr && nd != avd && rave.find(mv) != rave.end()) {
					// nd->ns += rave[mv];
					// nd->qs += rave[mv] * (win - nd->qs) / nd->ns;
					nd->ns += 1.0f;
					nd->qs += (win - nd->qs) / nd->ns;
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
			// ++rave[ancs[i + 1]->first];
			rave.insert(ancs[i + 1]->first);
		}
		if (ancs.size()) update_rave(root, ancs.front()->second);
	}

	void mcts_tree(unsigned id, const board& state, std::vector<random_player>& sim) {
		int t = T * static_cast<int>(- round / 40 + 1);
		do {
			node** cur = &roots[id];
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
				backtrack(rl != who, ancs, rave, roots[id]);
				continue;
			}
			
			// create node for cur
			make_node(cur, after, rl);
			// simulation from cur
			action_set rave;
			auto win = simulation(after, rave, sim);
			// if (win == who) std::cout << "root win!\n";
			// else std::cout << "root loses...\n";
			backtrack(win == who, ancs, rave, roots[id]);	
		} while (--t);
	}

	virtual action take_action(const board& state) override {
		// std::cout << role() << " take action\n";
		// state.show();
		std::chrono::steady_clock::time_point begin;
		if (demo) {
			++cnt;	
			begin = std::chrono::steady_clock::now();
		}
		
		// node* root = nullptr;
		
		// // int i = 0, T = 100;
		// int t = T * static_cast<int>(- round / 40 + 1);
		// ++round;, {random_player("name=white role=white"), random_player("name=black role=black"), random_player("name=white role=white")}
		// do {
		// 	node** cur = &root;
		// 	std::vector<mnpair_ptr> ancs;
		// 	board::piece_type rl = who;
		// 	board after = state;
		// 	// std::string path = ">";
		// 	while ((*cur) != nullptr) {
		// 		// std::cout << "dep\n";
		// 		// find a best child
		// 		// std::cout << path << " " << (*cur)->n << ' ' << (*cur)->q << '\n';
		// 		// path += '>';
		// 		auto br = branch(*cur, rl);
		// 		ancs.push_back(br);
		// 		if (br == (*cur)->chs.end()) break;
		// 		cur = &br->second;
		// 		rl = switch_role(rl);
		// 		br->first.apply(after);
		// 	}
		// 	// std::cout << path << '\n';

		// 	if (*cur != nullptr) {
		// 		// std::cout << "terminal node\n";
		// 		// cur is a termminal node (lose)
		// 		ancs.pop_back();
		// 		action_set rave;
		// 		backtrack(rl != who, ancs, rave, root);
		// 		continue;
		// 	}

		roots.resize(thread);
		std::fill(roots.begin(), roots.end(), nullptr);
		
		std::vector<std::thread> thrs;
		for (unsigned i = 0; i < thread; ++i) {
			thrs.push_back(std::thread(&mcts::mcts_tree, this, i, std::ref(state), std::ref(sims[i])));
		}
		for (unsigned i = 0; i < thread; ++i) {
			thrs[i].join();
		}
			
		// 	// create node for cur
		// 	make_node(cur, after, rl);
		// 	// simulation from cur
		// 	action_set rave;
		// 	auto win = simulation(after, rave);
		// 	// if (win == who) std::cout << "root win!\n";
		// 	// else std::cout << "root loses...\n";
		// 	backtrack(win == who, ancs, rave, root);	
		// } while (--t);

		
		
		std::unordered_map<action::place, float, hasher> mp;
		for (unsigned i = 0; i < thread; ++i) {
			for (auto& [mv, ch] : roots[i]->chs) {
				if (ch != nullptr) mp[mv] += ch->n;
			}
		}
		std::vector<std::pair<action::place, float>> stat;
		for (auto& [mv, cnt] : mp) {
			stat.push_back({mv, cnt});
		}
		for (unsigned i = 0; i < thread; ++i) delete_node(roots[i]);
		
		if (demo) {
			auto ed = std::chrono::steady_clock::now();
			telp += std::chrono::duration_cast<std::chrono::milliseconds>(ed - begin).count();	
		}
		

		if (stat.empty()) return action();

		auto mx = std::max_element(stat.begin(), stat.end(), [](const std::pair<action::place, float>& a, const std::pair<action::place, float>& b) {
			return a.second < b.second;
		});
		// std::cout << "my action is " << mx->first.position() << ' ' << mx->second->q << ' ' << mx->second->n << '\n';
		// board tmp = state;
		// mx->first.apply(tmp);
		// tmp.show();
		// std::this_thread::sleep_for(std::chrono::seconds(1));
		// std::cout << (" BW"[(int)who]) << " -> " << mx->first.position() << '\n';
		return mx->first;
	}

private:
	long long telp = 0;
	int cnt = 0;
	board::piece_type who;
	int T;
	float c, k;
	unsigned thread;
	bool demo;

	std::vector<std::vector<random_player>> sims;
	// random_player sim[3];
	std::vector<node*> roots;
	
	int round = 1;
};
