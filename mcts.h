#pragma once

#include <chrono>
#include <thread>
#include <unordered_set>
#include <thread>
#include <string_view>
#include <memory>

#include "agent.h"

// #define DEMO

class mcts : public agent {
public:
	mcts(const std::string& args = "") : agent("role=unknown " + args + " name=mcts") {
		assign("thread_size", thread_size);
		assign("c", c);
		assign("T", T);
		assign("k", k);
		if (meta.find("demo") != meta.end()) demo = true;
		if (meta.find("time") != meta.end()) time = true;


		/*
			initializa parellel objects
		*/
		gens.resize(thread_size);
		std::random_device rd;
		for (auto& gen : gens) gen.seed(rd());
		// std::generate(gens.begin(), gens.end(), []() {
		// 	return std::default_random_engine();
		// });

		bufs.resize(thread_size);
		for (auto& buf : bufs) buf.reserve(40 * T);
		buf_roots.reserve(40);
	}

protected:
	class node : public board {
	public:
		node(const board& state) : board(state), child(bit_count(state.available()), nullptr) {}

		using rave_array = std::array<std::array<bool, 81>, 2>;

	public:
		/*
			return the nullptr node to expend 
			return std::nullopt if all children are visited
		*/
		std::optional<node*> expend(std::vector<node>& buf) {
			#ifdef DEMO
			std::cout << "expend\n";
			#endif
			auto av = available();
			for (auto i = 0u; i < child.size(); ++i, av = reset(av)) {
				if (child[i] == nullptr) {
					board brd = *this;
					brd.place(lsb(av));
					buf.push_back(node(brd));
					return child[i] = &buf.back();
				}
			}
			return std::nullopt;
		}

		bool proceedable() const {
			return child.size() != 0;
		}

		bool fully_visited() const {
			return child.back() != nullptr;
		}

		node* select(float c = 0.1, float k = 10.0) {
			#ifdef DEMO
			auto res = std::max_element(child.begin(), child.end(), [&](node* a, node* b) {
				return a->score(visit, c, k) < b->score(visit, c, k);
			});
			std::cout << (info().who_take_turns > 1? "white" : "black");
			std::cout << "\t" << std::distance(child.begin(), res) << '\t' << (*res)->visit << '\t' << float((*res)->win) / (*res)->visit << '\n';
			#endif
			return *std::max_element(child.begin(), child.end(), [&](node* a, node* b) {
				return a->score(visit, c, k) < b->score(visit, c, k);
			});
		}

		float score(int par_visit, float c = 0.1, float k = 10.0) const {
			float exploit = float(win) / visit;
			float rave_exploit = float(rave_win) / rave_visit;
			float beta = std::sqrt(k / (par_visit + k));
			float explore = std::sqrt(std::log(par_visit) / visit);
			// return -exploit + c * explore; 
			return (beta - 1) * exploit - beta * rave_exploit + c * explore;
		}

		action find_best() const {
			auto best = std::max_element(child.begin(), child.end(), [](node* a, node* b) {
				if (a == nullptr) return true;
				if (b == nullptr) return false;
				return a->visit < b->visit;
			});
			if (best == child.end()) return action();
			#ifdef DEMO
			std::cout << bit_scan(find_move(**best)) << ' ' << (*best)->visit << ' ' << float((*best)->win) / (*best)->visit << '\n';
			#endif
			return action::place(bit_scan(find_move(**best)), info().who_take_turns);
		}

		board::piece_type simulate(std::default_random_engine& gen, rave_array& ra) const {
			board brd = *this;
			while (auto mv = brd.random_action(gen)) {
				ra[brd.info().who_take_turns - 1][*mv] = true;
				brd.place(*mv);
			}
			// #ifdef DEMO
			// std::cout << (brd.info().who_take_turns == board::white? "black" : "white") << " wins\n";
			// #endif
			return brd.info().who_take_turns == board::white? board::black : board::white;
		}

	public:
		int win = 0, visit = 0;
		int rave_win = 0, rave_visit = 0;
		std::vector<node*> child;
	};

	class tree {
	public:
		tree() = default;
		tree(const board& state) : root(new node(state)) {}

		tree& operator=(const tree& t) { root = t.root; return *this; }

	public:
		void run_mcts(std::size_t N, std::default_random_engine& gen, std::vector<node>& buf, float c, float k) {
			#ifdef DEMO
			std::cout << "run mcts" << '\n';
			#endif
			for (auto i = 0u; i < N; ++i) {
				auto path{select_expend(buf, c, k)};
				node::rave_array ra;
				update(path, path.back()->simulate(gen, ra), ra);
			}
			// #ifdef DEMO
			// std::cout << "size: " << size() << '\n';
			// #endif
			// return root->find_best();
		}

		bool empty() const {
			return root == nullptr;
		}

	public:
		void initialze(const board& state, std::vector<node>& buf) {
			buf.push_back(node(state));
			root = &buf.back();
		}

		void clear() { root = nullptr; }

		bool move(const board& state) {
			// std::cout << "start move\n";
			for (auto i = 0u; i < root->child.size(); ++i) {
				auto ch = root->child[i];
				if (ch == nullptr) continue;
				for (auto j = 0u; j < ch->child.size(); ++j) {
					auto ch2 = ch->child[j];
					if (ch2 == nullptr) continue;
					if (static_cast<board>(*ch2) == state) {
						root = ch2;
						return true;
					}
				}
			}
			return false;
		}

		// #ifdef DEMO
		// int size(node* nd) const {
		// 	int re = 0;
		// 	for (auto& ch : nd->child) if (ch != nullptr) re += size(ch);
		// 	return re + 1;
		// }
		// int size() const {
		// 	return size(root);
		// }
		// #endif

		// void plot(node* nd, int sp) {
		// 	std::cout << "plot" << std::endl;
		// 	std::cout << "> ";
		// 	for (auto i = 0; i < sp; ++i) std::cout << "    ";
		// 	std::cout << nd->win << ' ' << nd->visit << '\n';
		// 	for (auto& ch : nd->child) if (ch != nullptr) plot(ch, sp + 1);
		// }

	protected:
		std::vector<node*> select_expend(std::vector<node>& buf, float c = 0.05, float k = 10.0) {
			#ifdef DEMO
			std::cout << "select and expend" << '\n';
			#endif
			std::vector<node*> path = {root};
			++root->visit;
			while (path.back()->proceedable() && path.back()->fully_visited()) {
				auto nd = path.back()->select(c, k);
				path.push_back(nd);
				/*
					virtual loss
				*/
				++nd->visit;
				++nd->rave_visit;
			}
			// #ifdef DEMO
			// if (auto res = path.back()->expend()) std::cout << "can expend" << '\n';
			// else std::cout << "can't expend" << '\n';
			// #endif
			if (auto res = path.back()->expend(buf)) path.push_back(*res), ++(*res)->visit;
			// else path.back() is a terminal node

			return path;
		}

		void update(std::vector<node*>& path, board::piece_type win, node::rave_array& ra) {
			#ifdef DEMO
			std::cout << "update" << '\n';
			#endif
			auto who = root->info().who_take_turns;
			for (auto& nd : path) {
				/*
					mornal update
				*/
				auto ndwho = nd->info().who_take_turns;
				if (win == who && ndwho == who)      ++nd->win, ++nd->rave_win;
				else if (win != who && ndwho != who) ++nd->win, ++nd->rave_win;

				/*
					rave update
				*/
				for (auto& ch : nd->child) {
					if (ch == nullptr) continue;
					auto chwho = ch->info().who_take_turns;
					if (ra[chwho - 1][nd->find_move_index(*ch)]) {
						if (win == who && chwho == who)      ++ch->rave_win;
						else if (win != who && ndwho != who) ++ch->rave_win;
						++ch->rave_visit;
					}
				} 
			}
		}

	public:
		node* root = nullptr;
	};

protected:
	template<typename T>
	bool assign(const std::string& name, T& buf) {
		auto b = meta.find(name) != meta.end();
		if (b) buf = meta[name];
		return b;
	}

public:
	action take_action(const board& state) override {
		std::chrono::steady_clock::time_point begin;
		if (time) {
			begin = std::chrono::steady_clock::now();
			++move_count;
		}

		if (tre.empty()) tre.initialze(state, buf_roots);
		else if (!tre.move(state)) tre.initialze(state, buf_roots);

		std::vector<std::thread> thrs;
		for (auto i = 0u; i < thread_size; ++i) {
			thrs.push_back(std::thread(&tree::run_mcts, tre, T, std::ref(gens[i]), std::ref(bufs[i]), c, k));
		}
		for (auto& thr : thrs) thr.join();
		auto re = tre.root->find_best();

		if (time) {
			auto end = std::chrono::steady_clock::now();
			time_elp += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
		}
		return re;
	}

	virtual void open_episode(const std::string& flag = "") override { 
		#ifdef DEMO
		std::cout << "-------------------\n"; 
		#endif
	}
	virtual void close_episode(const std::string& flag = "") override {
		/*
			clear buffers for parellel
		*/
		for (auto& buf : bufs) buf.clear();
		buf_roots.clear();
		tre.clear();

		/*
			show and clear statistic data
		*/
		if (time) {
			std::cout << move_count << " moves in " << time_elp << " ms\n";
			std::cout << float(time_elp) / move_count << " ms/move\n";
			std::cout << float(time_elp) / move_count / T / thread_size << " ms/mcts\n";
			move_count = 0;
			time_elp = 0;	
		}
		
		#ifdef DEMO
		std::cout << "-------------------\n";
		#endif
	}

protected:
	/*
		parameters in MCTS
	*/
	float c = 0.14; // explore rate
	float k = 10.0; // rave 
	std::size_t T = 10000; // number of MCTS
	
	/*
		objects in parellel
	*/
	std::size_t thread_size = 1;
	tree tre;
	std::vector<std::default_random_engine> gens;
	std::vector<std::vector<node>> bufs;
	std::vector<node> buf_roots;

	/*
		statistics
	*/
	bool demo = false;
	bool time = false;
	int move_count = 0;
	int time_elp = 0;
};