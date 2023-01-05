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
		// assign("T", T);
		assign("k", k);
		assign("reserve", reserve);
		assign("time", time);
		assign("c_time", c_time);
		assign("max_ply", max_ply);
		if (meta.find("demo") != meta.end()) demo = true;
		if (meta.find("stat_time") != meta.end()) stat_time = true;


		/*
			initializa parellel objects
		*/
		gens.resize(thread_size);
		std::random_device rd;
		for (auto& gen : gens) gen.seed(rd());

		bufs.resize(thread_size);
		for (auto& buf : bufs) buf.reserve(reserve);
		buf_main.reserve(reserve);
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
			auto av = available();
			for (auto i = 0u; i < child.size(); ++i, av = reset(av)) {
				if (child[i] == nullptr) {
					board brd = *this;
					brd.place(lsb(av));
					// auto sz = buf.capacity();
					buf.push_back(node(brd));
					// if (buf.capacity() != sz) std::cout << "cap!" << buf.size() << std::endl;
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
			return *std::max_element(child.begin(), child.end(), [&](node* a, node* b) {
				return a->score(visit, c, k) < b->score(visit, c, k);
			});
		}

		float score(int par_visit, float c = 0.1, float k = 10.0) const {
			float exploit = float(win) / visit;
			float rave_exploit = float(rave_win) / rave_visit;
			float beta = std::sqrt(k / (3 * visit + k));
			float explore = std::sqrt(std::log(par_visit) / visit);
			// return -exploit + c * explore; 
			return (beta - 1) * exploit - beta * rave_exploit + c * explore;
		}

		std::optional<action> find_best_order(std::size_t ith, float k = 10.0) const {
			/*
				sort the moves from root
			*/
			if (child.size() <= ith) return std::nullopt;
			std::vector<std::pair<int, int>> con;
			for (auto i = 0u; i < child.size(); ++i) con.push_back({child[i] == nullptr? 0 : -child[i]->visit, i});
			// for (auto i = 0u; i < child.size(); ++i) con.push_back({child[i] == nullptr? 1 : float(child[i]->win) / child[i]->visit, i});
			// for (auto i = 0u; i < child.size(); ++i) con.push_back({child[i] == nullptr? 0 : -child[i]->raved_visit(visit, k), i});
			std::partial_sort(con.begin(), con.begin() + ith, con.end());
			if (child[con[ith].second] == nullptr) return std::nullopt;
			return action::place(bit_scan(find_move(*child[con[ith].second])), info().who_take_turns);
		}

		std::size_t get_index(action mv) const {
			for (auto i = 0u; i < child.size(); ++i) {
				board brd = *this;
				mv.apply(brd);
				if (brd == static_cast<board>(*child[i])) return i;
			}
			return child.size();
		}

		board::piece_type simulate(std::default_random_engine& gen, rave_array& ra) const {
			board brd = *this;
			while (auto mv = brd.random_action(gen)) {
				ra[brd.info().who_take_turns - 1][*mv] = true;
				brd.place(*mv);
			}
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

	public:
		void run_mcts(std::size_t N, std::default_random_engine& gen, std::vector<node>& buf, float c, float k) {
			for (auto i = 0u; i < N; ++i) {
			// while (alive) {
				auto path{select_expend(buf, c, k)};
				node::rave_array ra;
				update(path, path.back()->simulate(gen, ra), ra);

				/*
					EARLY-C
				*/
				if (8 * i > N && i % 100 == 0) {
					auto mv1 = root->find_best_order(0), mv2 = root->find_best_order(1);
					if (!mv1 || !mv2) return;
					auto idx1 = root->get_index(*mv1), idx2 = root->get_index(*mv2);
					// auto vst1 = root->child[idx1]->raved_visit(root->visit, k), vst2 = root->child[idx2]->raved_visit(root->visit, k);
					auto vst1 = root->child[idx1]->visit, vst2 = root->child[idx2]->visit;
					// if (vst2 + (N - i - 1) * 0.4 < vst1) {
					// 	// std::cout << i << '\n';
					// 	auto c1 = root->child[idx1], c2 = root->child[idx2];
					// 	std::cout << vst1 << '\t' << vst2 << '\t' << i << '\n';
					// 	std::cout << c1->win << '\t' << c2->win << '\n';
					// 	std::cout << float(c1->win) / vst1 << '\t' << float(c2->win) / vst2 << '\n';
					// 	std::cout << c1->rave_win << '\t' << c2->rave_win << '\n';
					// 	std::cout << c1->rave_visit << '\t' << c2->rave_visit << '\n';
					// 	std::cout << float(c1->rave_win) / c1->rave_visit << '\t' << float(c2->rave_win) / c2->rave_visit << '\n'; 
					// }
					if (vst2 + (N - i - 1) * 0.4 < vst1) return;
					// if (i + 150 >= N - 1) std::cout << vst1 << '\t' << vst2 << '\t' << i << " safe\n";
				}
			}
		}

		/*
			in opponent's thinking time, run this
		*/
		void run_mcts_after(bool& alive, action mv, std::default_random_engine& gen, std::vector<node>& buf, float c, float k) {
			/*
				find the child actioned by mv
			*/
			board brd = *root;
			mv.apply(brd);
			node* nd = nullptr;
			for (auto& ch : root->child) {
				if (ch == nullptr) continue;
				if (static_cast<board>(*ch) == brd) {
					nd = ch;
					break;
				}
			}

			/*
				main loop
			*/
			while (alive) {
				auto path{select_expend(buf, c, k, nd)};
				node::rave_array ra;
				update(path, path.back()->simulate(gen, ra), ra);
			}
		}

	public:
		void initialze(const board& state, std::vector<node>& buf) {
			buf.push_back(node(state));
			root = &buf.back();
		}

		bool empty() const { return root == nullptr; }
		void clear() { root = nullptr; }

	protected:
		/*
			move all node instances to buf
			**ENSURE** buf has reserved enough space
		*/
		node* move(node* cur, std::vector<node>& buf) {
			buf.push_back(*cur);
			node* ncur = &buf.back();
			for (auto i = 0u; i < cur->child.size(); ++i) {
				if (cur->child[i] == nullptr) continue;
				ncur->child[i] = move(cur->child[i], buf);
			}
			return ncur;
		}

	public:
		bool move(const board& state, std::vector<node>& buf) {
			for (auto i = 0u; i < root->child.size(); ++i) {
				auto ch = root->child[i];
				if (ch == nullptr) continue;
				for (auto j = 0u; j < ch->child.size(); ++j) {
					auto ch2 = ch->child[j];
					if (ch2 == nullptr) continue;
					if (static_cast<board>(*ch2) == state) {
						root = move(ch2, buf);
						return true;
					}
				}
			}
			return false;
		}

		// #ifdef DEMO
		int size(node* nd) const {
			int re = 0;
			for (auto& ch : nd->child) if (ch != nullptr) re += size(ch);
			return re + 1;
		}
		int size() const {
			return size(root);
		}
		// #endif

		// void plot(node* nd, int sp) {
		// 	std::cout << "plot" << std::endl;
		// 	std::cout << "> ";
		// 	for (auto i = 0; i < sp; ++i) std::cout << "    ";
		// 	std::cout << nd->win << ' ' << nd->visit << '\n';
		// 	for (auto& ch : nd->child) if (ch != nullptr) plot(ch, sp + 1);
		// }

	protected:
		/*
			assigned_child is a child of root
			means we only search the subtree rooted from it
		*/
		std::vector<node*> select_expend(std::vector<node>& buf, float c = 0.05, float k = 10.0, node* assigned_child = nullptr) {
			std::vector<node*> path = {root};
			++root->visit;
			if (assigned_child != nullptr) {
				path.push_back(assigned_child);
				++assigned_child->visit;
			}
			while (path.back()->proceedable() && path.back()->fully_visited()) {
				auto nd = path.back()->select(c, k);
				path.push_back(nd);
				/*
					virtual loss
				*/
				++nd->visit;
				++nd->rave_visit;
			}
			if (auto res = path.back()->expend(buf)) path.push_back(*res), ++(*res)->visit, ++(*res)->rave_visit;;
			// else path.back() is a terminal node

			return path;
		}

		void update(std::vector<node*>& path, board::piece_type win, node::rave_array& ra) {
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
						++ch->rave_visit;
						if (win == who && chwho == who)      ++ch->rave_win;
						else if (win != who && chwho != who) ++ch->rave_win;
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

	void end_after_mcts() {
		is_thread_alive = false;
		for (auto& thrs : afters) thrs.join();
		afters.clear();
	}

public:
	action take_action(const board& state) override {
		/*
			first record time stamp
		*/
		std::chrono::steady_clock::time_point begin;
		begin = std::chrono::steady_clock::now();
		++move_count;

		/*
			terminate after mcts
		*/
		end_after_mcts();

		/*
			reuse reallocation
		*/
		std::vector<node> buf_tmp;
		buf_tmp.reserve(reserve);
		if (tre.empty()) tre.initialze(state, buf_tmp);
		else if (!tre.move(state, buf_tmp)) tre.initialze(state, buf_tmp);
		buf_main = std::move(buf_tmp);
		for (auto& buf : bufs) buf.clear();
		// std::cout << "size = " << tre.size() << '\n';

		/*
			calculating remaining time (Enhanced) and # of mcts can do
		*/
		std::size_t time_rem = time * 1000 - time_elp;
		// std::cout << "time remaining = " << time_rem / 1000 << '\n';
		std::size_t T = time_rem / (c_time + std::max(max_ply - move_count, 0)) * mcts_per_ms;
		// std::cout << "T = " << T << '\n';

		/*
			generate threads for mcts
		*/
		std::vector<std::thread> thrs;
		for (auto i = 0u; i < thread_size; ++i) {
			thrs.push_back(std::thread(&tree::run_mcts, tre, T, std::ref(gens[i]), std::ref(bufs[i]), c, k));
		}
		for (auto& thr : thrs) thr.join();

		/*
			maintain time elp
		*/
		auto end = std::chrono::steady_clock::now();
		time_elp += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();


		if (auto re = tre.root->find_best_order(0, k)) {
			/*
				generate threads for after mcts
			*/
			if (meta.find("skip") != meta.end()) return *re;
			is_thread_alive = true;
			for (auto i = 0u; i < thread_size; ++i) {
				afters.push_back(std::thread(&tree::run_mcts_after, tre, std::ref(is_thread_alive), *re, std::ref(gens[i]), std::ref(bufs[i]), c, k));
			}
			return *re;
		}
		return action();
	}

	virtual void close_episode(const std::string& flag = "") override {
		/*
			after mcts
		*/
		end_after_mcts();

		/*
			clear buffers for parellel
		*/
		for (auto& buf : bufs) buf.clear();
		buf_main.clear();
		tre.clear();

		/*
			show and clear statistic data
		*/
		if (stat_time) {
			std::cout << move_count << " moves in " << time_elp << " ms\n";
			std::cout << float(time_elp) / move_count << " ms/move\n";
			// std::cout << float(time_elp) / move_count / T / thread_size << " mcts/ms\n";
		}
		move_count = 0;
		time_elp = 0;
	}

protected:
	/*
		parameters in MCTS
	*/
	float c = 0.14; // explore rate
	float k = 10.0; // rave 

	/*
		time management
	*/
	// std::size_t T = 10000; // number of MCTS
	std::size_t time = 60; // total available time
	float c_time = 15; // dividing factor on time
	int max_ply = 15; // the move require most time
	float mcts_per_ms = 300;
	
	/*
		MCTS tree
	*/
	tree tre;

	/*
		objects in parellel
	*/
	std::size_t thread_size = 14; // # of thread used
	bool is_thread_alive = false; // switch to kill while-true thread
	std::size_t reserve = 4000000; // buffer size aka expected number of mcts per move
	std::vector<std::default_random_engine> gens; // random generator for each thread
	std::vector<std::vector<node>> bufs; // node buffer for each thread
	std::vector<node> buf_main; // node buffer for inherence
	std::vector<std::thread> afters; // after mcts

	/*
		statistics
	*/
	bool demo = false;
	bool stat_time = false;
	int move_count = 0;
	int time_elp = 0;
};