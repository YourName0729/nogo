#pragma once

#include <chrono>
#include <thread>
#include <unordered_set>
#include <thread>
#include <string_view>
#include <memory>
#include <fstream>
#include <iomanip>
#include <algorithm>

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
		assign("reserve_main", reserve_main);
		assign("time", time);
		assign("c_time", c_time);
		assign("max_ply", max_ply);
		assign("max_ply_mul", max_ply_mul);
		if (meta.find("demo") != meta.end()) demo = true;
		if (meta.find("stat") != meta.end()) {
			stat = true;
			stat_out.open(meta["stat"], std::ios_base::app);
		}


		/*
			initializa parellel objects
		*/
		gens.resize(thread_size);
		std::random_device rd;
		for (auto& gen : gens) gen.seed(rd());

		bufs.resize(thread_size);
		for (auto& buf : bufs) buf.reserve(reserve);
		// buf_main.reserve(reserve);

		/*
			simulation balancing
		*/
		// if (meta.find("load") != meta.end()) tre.load_weight(meta["load"]);
	}

	~mcts() {
		/*
			simulation balancing
		*/
		// if (meta.find("save") != meta.end()) tre.save_weight(meta["save"]);

		/*
			stat
		*/
		if (meta.find("stat") != meta.end()) stat_out.close();
	}

protected:
	class node : public board {
	public:
		node(const board& state) : board(state), child(bit_count(state.available()), nullptr) {}

	public:
		/*
			return the nullptr node to expend 
			return std::nullopt if all children are visited
		*/
		std::optional<node*> expend(std::vector<node>& buf) {
			if (buf.capacity() == buf.size()) return std::nullopt;
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
			for (auto i = 0u; i < child.size(); ++i) con.push_back({(child[i] == nullptr? 0 : -child[i]->visit), i});
			// for (auto i = 0u; i < child.size(); ++i) con.push_back({child[i] == nullptr? 1 : float(child[i]->win) / child[i]->visit, i});
			// for (auto i = 0u; i < child.size(); ++i) con.push_back({child[i] == nullptr? 0 : -child[i]->raved_visit(visit, k), i});
			std::partial_sort(con.begin(), con.begin() + ith + 1, con.end());
			// std::sort(con.begin(), con.end());
			if (child[con[ith].second] == nullptr) return std::nullopt;
			// std::cout << "fbo " << ith << ' ' << con[ith].first << ' ' << con[ith].second << '\n';
			return action::place(board::bit_scan(find_move(*child[con[ith].second])), info().who_take_turns);
		}

		std::size_t get_index(action mv) const {
			for (auto i = 0u; i < child.size(); ++i) {
				if (child[i] == nullptr) continue;
				board brd = *this;
				mv.apply(brd);
				if (brd == static_cast<board>(*child[i])) return i;
			}
			return child.size();
		}

	public:
		int win = 0, visit = 0;
		int rave_win = 0, rave_visit = 0;
		std::vector<node*> child;
	};

	class tree {
	public:
		tree() = default;

		using rave_array = std::array<std::array<bool, 81>, 2>;

	public:
		void run_mcts(std::size_t N, std::default_random_engine& gen, std::vector<node>& buf, float c, float k) {
			for (auto i = 0u; i < N; ++i) {
			// while (alive) {
				auto path{select_expend(buf, c, k)};
				rave_array ra;
				update(path, simulate(*path.back(), gen, ra), ra);
				// update(path, simulate_weight(*path.back(), gen, ra), ra);
				/*
					EARLY-C
				*/
				if (8 * i > N && i % 100 == 0) {
					auto mv1 = root->find_best_order(0), mv2 = root->find_best_order(1);
					if (!mv1 || !mv2) return;
					auto idx1 = root->get_index(*mv1), idx2 = root->get_index(*mv2);
					// auto vst1 = root->child[idx1]->raved_visit(root->visit, k), vst2 = root->child[idx2]->raved_visit(root->visit, k);
					auto vst1 = root->child[idx1]->visit, vst2 = root->child[idx2]->visit;
					// if (vst2 + (N - i - 1) * 0.5 < vst1) {
					// 	for (auto i = 0u; i < root->child.size(); ++i) {
					// 		if (root->child[i] != nullptr) std::cout << root->child[i]->visit << '\n';
					// 	}
					// 	std::cout << "vst " << vst1 << ' ' << vst2 << '\n';
					// 	std::cout << "N-i=" << (N - i - 1) << '\n';
					// 	std::cout << "----------\n";	
					// }
					
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
					if (vst2 + (N - i - 1) * 0.5 < vst1) return;
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
			// board brd = *root;
			// mv.apply(brd);
			// node* nd = nullptr;
			// for (auto& ch : root->child) {
			// 	if (ch == nullptr) continue;
			// 	if (static_cast<board>(*ch) == brd) {
			// 		nd = ch;
			// 		break;
			// 	}
			// }
			auto nd = root->child[root->get_index(mv)];

			/*
				main loop
			*/
			while (alive) {
				auto path{select_expend(buf, c, k, nd)};
				rave_array ra;
				// update(path, path.back()->simulate(gen, ra), ra);
				update(path, simulate(*path.back(), gen, ra), ra);
				// update(path, simulate_weight(*path.back(), gen, ra), ra);
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
			buf.push_back(std::move(*cur));
			node* ncur = &buf.back();
			for (auto i = 0u; i < ncur->child.size(); ++i) {
				if (ncur->child[i] == nullptr) continue;
				if (buf.capacity() == buf.size()) {
					/*
						forget child[i]
					*/
					// ncur->visit -= ncur->child[i]->visit;
					// ncur->win -= (ncur->child[i]->visit - ncur->child[i]->win);
					ncur->child[i] = nullptr;
				}
				else ncur->child[i] = move(ncur->child[i], buf);
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
						// auto s = size(ch2);
						// std::cout << "size1 = " << s << '\n';
						
						root = move(ch2, buf);
						return true;
					}
				}
			}
			return false;
		}

		void move_after(action mv, std::vector<node>& buf) {
			auto idx = root->get_index(mv);
			buf.push_back(*root);
			root = &buf.back();
			for (auto i = 0u; i < root->child.size(); ++i) {
				if (root->child[i] == nullptr) continue;
				if (i != idx) {
					/*
						forget child[i]
					*/
					// root->visit -= root->child[i]->visit;
					// root->win -= (root->child[i]->visit - root->child[i]->win);
					root->child[i] = nullptr;
				} 
				else root->child[i] = move(root->child[i], buf);
			}
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
			++root->visit, ++root->rave_visit;
			if (assigned_child != nullptr) {
				path.push_back(assigned_child);
				++assigned_child->visit, ++assigned_child->rave_visit;
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

		void update(std::vector<node*>& path, board::piece_type win, rave_array& ra) {
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

		board::piece_type simulate(const board& state, std::default_random_engine& gen, rave_array& ra) const {
			board brd = state;
			while (auto mv = brd.random_action(gen)) {
				ra[brd.info().who_take_turns - 1][*mv] = true;
				brd.place(*mv);
			}
			return brd.info().who_take_turns == board::white? board::black : board::white;
		}

	/*
		this part is about weighted simulation
	*/
	// public:
	// 	void load_weight(const std::string& fname) {
	// 		// std::cout << "load weight\n";
	// 		std::ifstream fin(fname);
	// 		if (!fin) throw std::invalid_argument("file not exists");
	// 		uint64_t size = 0;
	// 		fin.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
	// 		weight.resize(size);
	// 		fin.read(reinterpret_cast<char*>(weight.data()), sizeof(float) * size);
	// 		// return out;
	// 		// float tmp;
	// 		// while (fin >> tmp) weight.push_back(tmp);
	// 		fin.close();
	// 	}

	// 	void save_weight(const std::string& fname) {
	// 		// std::cout << "save weight\n";
	// 		std::ofstream fout(fname);
	// 		if (!fout) throw std::invalid_argument("file not exists");
	// 		uint64_t size = weight.size();
	// 		fout.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
	// 		fout.write(reinterpret_cast<const char*>(weight.data()), sizeof(float) * size);
	// 		// fout << std::fixed << std::setprecision(6);
	// 		// for (auto& v : weight) fout << v << '\n';
	// 		fout.close();
	// 	}

	// 	void run_mcts_balancing(std::size_t N, std::size_t move_count, std::default_random_engine& gen, std::vector<node>& buf, float c, float k, float alpha) {
	// 		for (auto i = 0u; i < N; ++i) {
	// 			auto path{select_expend(buf, c, k)};
	// 			rave_array ra;
	// 			// std::cout << i << '\t';
	// 			// std::cout << path.size() << ' ';
	// 			simulation_balancing(10 * move_count, *path.back(), gen, c, k, alpha);
	// 			update(path, simulate_weight(*path.back(), gen, ra), ra);
	// 		}
	// 	}

	// 	std::optional<int> weight_action(const board& state, std::default_random_engine& gen) const {
	// 		auto av = state.available(state.info().who_take_turns);
	// 		if (!av) return std::nullopt;
	// 		std::vector<std::pair<float, int>> mvs;
	// 		double sm = 0;
	// 		for (auto i = 0u; av; ++i, av = reset(av)) {
	// 			int mv = board::bit_scan(lsb(av));
	// 			mvs.push_back({std::exp(score(get_weight(state, mv))), mv});
	// 			sm += mvs.back().first;
	// 		}
	// 		auto ch = std::uniform_real_distribution<float>(0, sm)(gen);
	// 		for (auto i = 0u; i < mvs.size(); ++i) {
	// 			if (ch < mvs[i].first) return mvs[i].second;
	// 			ch -= mvs[i].first;
	// 		}
	// 		return std::nullopt;
	// 	}

	// 	board::piece_type simulate_weight(const board& state, std::default_random_engine& gen, rave_array& ra) const {
	// 		board brd = state;
	// 		while (auto mv = weight_action(brd, gen)) {
	// 			ra[brd.info().who_take_turns - 1][*mv] = true;
	// 			brd.place(*mv);
	// 		}
	// 		return brd.info().who_take_turns == board::white? board::black : board::white;
	// 	}

	// 	/*
	// 		training the weight
	// 		1. run mcts on the state M times, calc the win rate V*, assuming the god solution
	// 		2. run simulation_weight M times, calc the win rate V
	// 		3. run simulation_weight N times, sum the whole weight, and average, called g
	// 		4. train the weight by alpha * (V* - V) * g

	// 		that is, if V is overestimated, then decrease g
	// 		otherwise, if V is underestimated, then increase g
	// 	*/
	// 	void simulation_balancing(std::size_t N, const board& state, std::default_random_engine& gen, float c, float k, float alpha = 0.1) {
	// 		// std::size_t N = 500;
	// 		/*
	// 			step 1. calc the V*
	// 		*/
	// 		// std::cout << "calc V*\n";
	// 		if (!state.available()) return;
	// 		// std::cout << "turn " << int(state.info().who_take_turns) << ' ';
	// 		tree tre;
	// 		std::vector<node> buf;
	// 		buf.reserve(N + 100);
	// 		tre.initialze(state, buf);
	// 		tre.run_mcts(N, gen, buf, c, k);
	// 		float v_star = float(tre.root->win) / tre.root->visit;

	// 		/*
	// 			step 2 and 3. calc the V and get the current weight vector
	// 		*/
	// 		// std::cout << "calc V and g\n";
	// 		float v = 0;
	// 		// std::size_t move_count = 0;
	// 		std::vector<float> cur_weight(weight.size()), opp_weight(weight.size());
	// 		for (auto i = 0u; i < N; ++i) {
	// 			board brd = state;
	// 			while (auto mv = weight_action(brd, gen)) {
	// 				auto same = brd.info().who_take_turns == state.info().who_take_turns;
	// 				if (same) for (auto& [code, v] : get_weight(brd, *mv)) cur_weight[code] += v;
	// 				else      for (auto& [code, v] : get_weight(brd, *mv)) opp_weight[code] += v;
	// 				// cur_weight[encode(state, *mv)] += 1;
	// 				// ++move_count;
	// 				brd.place(*mv);
	// 			}
	// 			if (brd.info().who_take_turns != state.info().who_take_turns) v += 1;
	// 		}
	// 		v /= N;
	// 		auto norm = [](const std::vector<float>& vec) {
	// 			float re = 0;
	// 			for (auto v : vec) re += v * v;
	// 			return re;
	// 		};
	// 		auto normalize = [](std::vector<float>& vec) {
	// 			float acc = std::accumulate(vec.begin(), vec.end(), 0.0);
	// 			// std::cout << acc;
	// 			for (auto& v : vec) v /= acc;
	// 			// std::cout << " " << std::accumulate(vec.begin(), vec.end(), 0.0) << '\n';
	// 		};

	// 		normalize(cur_weight);
	// 		/*
	// 			opponent at least one move
	// 		*/
	// 		if (norm(opp_weight) > 1e-5) {
	// 			float nwei = norm(weight);
	// 			normalize(opp_weight);	
	// 			for (auto i = 0u; i < weight.size(); ++i) cur_weight[i] += (weight[i] / nwei - opp_weight[i]);
	// 			for (auto& v : cur_weight) v /= 2.f;
	// 		}

	// 		/*
	// 			step 4. train
	// 		*/
	// 		// std::cout << "train\n";
	// 		float sm = 0, mx = 0, v3 = 0;
	// 		for (auto i = 0u; i < weight.size(); ++i) {
	// 			v3 += weight[i] * cur_weight[i];
	// 			weight[i] += alpha * (v_star - v) * cur_weight[i];
	// 			sm += std::abs(alpha * (v_star - v) * cur_weight[i]);
	// 			mx = std::max(mx, std::abs(alpha * (v_star - v) * cur_weight[i]));
	// 		}
	// 		// std::cout << "train=" << sm << "    \t" << "mx=" << mx << "     \t";
	// 		std::cout << std::fixed << std::setprecision(5) << "V*=" << v_star << " V=" << v << " V3= " << v3 <<'\n';
	// 	}

	// protected:
	// 	std::vector<std::pair<std::size_t, float>> get_weight(const board& state, int mv) const {
	// 		auto get_weight_single = [&](const board& state, int mv) {
	// 			std::vector<std::pair<std::size_t, float>> re;
	// 			int x = mv / 9, y = mv % 9;
	// 			for (auto i = -1; i <= 1; ++i) {
	// 				for (auto j = -1; j <= 1; ++j) {
	// 					// if (i == j && i == 0) continue;
	// 					int nx = x + i, ny = y + j, idx = nx * 9 + ny;
	// 					if (nx < 0 || ny < 0 || nx >= 9 || ny >= 9) continue;
	// 					auto p = state(nx, ny);
	// 					if (p == state.info().who_take_turns) re.push_back({encode(state, idx), 1});
	// 					else if (p == 3 - state.info().who_take_turns) re.push_back({encode(state, idx), -1});
	// 				}
	// 			}
	// 			return re;
	// 		};

	// 		auto org = get_weight_single(state, mv);
	// 		board brd = state;
	// 		brd.place(mv);
	// 		auto cur = get_weight_single(brd, mv);
	// 		std::vector<std::pair<std::size_t, float>> re;
	// 		for (auto& [code, v] : org) re.push_back({code, -v});
	// 		for (auto& [code, v] : cur) re.push_back({code, -v});
	// 		return re;
	// 	}

	// 	float score(const std::vector<std::pair<std::size_t, float>>& wei) const {
	// 		float re = 0;
	// 		for (auto& [code, v] : wei) re += v * weight[code];
	// 		return re;
	// 	}

	// 	std::size_t encode(const board& state, int mv) const {
	// 		std::size_t re = 0;
	// 		int x = mv / 9, y = mv % 9;
	// 		for (auto i = -1; i <= 1; ++i) {
	// 			for (auto j = -1; j <= 1; ++j) {
	// 				if (i == j && i == 0) continue;
	// 				int nx = x + i, ny = y + j;
	// 				if (nx < 0 || ny < 0 || nx >= 9 || ny >= 9) continue;
	// 				auto p = state(nx, ny);
	// 				re <<= 2;
	// 				if (p == board::piece_type::hollow) continue;
	// 				if (p == board::piece_type::empty) re += 3;
	// 				else if (p == state.info().who_take_turns) re += 1;
	// 				else re += 2;
	// 			}
	// 		}
	// 		return re;
	// 	}

	public:
		node* root = nullptr;
		// std::vector<float> weight;
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

	void reallocate(const board& state) {
		// std::cout << "reallocation\n";
		std::vector<node> buf_tmp;
		buf_tmp.reserve(reserve_main);
		if (tre.empty()) tre.initialze(state, buf_tmp);
		else if (!tre.move(state, buf_tmp)) tre.initialze(state, buf_tmp);
		buf_main = std::move(buf_tmp);
		for (auto& buf : bufs) buf.clear();
		// std::cout << "size = " << tre.size() << '\n';
	}

	void reallocate_after(action mv) {
		std::vector<node> buf_tmp;
		buf_tmp.reserve(reserve_main);
		tre.move_after(mv, buf_tmp);
		buf_main = std::move(buf_tmp);
		for (auto& buf : bufs) buf.clear();
	}

	void update_time(std::chrono::steady_clock::time_point& begin) {
		auto end = std::chrono::steady_clock::now();
		time_elp += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	}

public:
	action take_action(const board& state) override {
		if (stat) {
			if (move_count == 0) 
				stat_out << "======== New Game ========\n";
			stat_out << "------ move " << move_count << " ------\n";
		}

		/*
			first record time stamp
		*/
		std::chrono::steady_clock::time_point begin;
		begin = std::chrono::steady_clock::now();
		++move_count;

		/*
			simulation balancing
		*/
		// if (meta.find("balancing") != meta.end()) {
		// 	std::cout << "balancing action\n";
		// 	float alpha = 0.1;
		// 	assign("alpha", alpha);
		// 	buf_main.clear();
		// 	tre.initialze(state, buf_main);
		// 	tre.run_mcts_balancing(500, move_count, gens[0], buf_main, c, k, alpha);
		// 	/*
		// 		maintain time elp
		// 	*/
		// 	auto end = std::chrono::steady_clock::now();
		// 	time_elp += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

		// 	if (meta.find("save") != meta.end()) tre.save_weight(meta["save"]);

		// 	if (auto mv = tre.root->find_best_order(0)) return *mv;
		// 	return action();
		// }

		/*
			terminate after mcts
		*/
		end_after_mcts();
		// if (stat) {
		// 	// stat_out << "after:\n";
		// 	// stat_out << "main: " << buf_main.size() << '\n';
		// 	auto mx = bufs[0].size();
		// 	for (auto i = 0u; i < thread_size; ++i) {
		// 		mx = std::max(mx, bufs[i].size());
		// 		// stat_out << "thr " << i << ": " << bufs[i].size() << '\n';
		// 	}
		// 	stat_out << "after: " << buf_main.size() << ' ' << mx << std::endl;
		// }

		/*
			reuse reallocation
		*/
		reallocate(state);
		

		/*
			calculating remaining time (Enhanced) and # of mcts can do
		*/
		std::size_t time_rem = time * 1000 - time_elp;
		// std::cout << "time remaining = " << time_rem / 1000 << '\n';
		std::size_t T = time_rem / (c_time + max_ply_mul * std::max(max_ply - move_count, 0)) * mcts_per_ms;
		// std::cout << "T = " << T << '\n';
		// T=100;
		if (stat) {
			stat_out << "T       : " << T << std::endl;
			stat_out << "Time    : " << time_rem / 1000 << std::endl;
		}

		/*
			generate threads for mcts
		*/
		std::vector<std::thread> thrs;
		for (auto i = 0u; i < thread_size; ++i) {
			thrs.push_back(std::thread(&tree::run_mcts, tre, T, std::ref(gens[i]), std::ref(bufs[i]), c, k));
		}
		for (auto& thr : thrs) thr.join();

		// if (stat) {
		// 	// stat_out << "main: " << buf_main.size() << '\n';
		// 	// for (auto i = 0u; i < thread_size; ++i) {
		// 	// 	stat_out << "thr " << i << ": " << bufs[i].size() << '\n';
		// 	// }
		// 	auto mx = bufs[0].size();
		// 	for (auto i = 0u; i < thread_size; ++i) {
		// 		mx = std::max(mx, bufs[i].size());
		// 		// stat_out << "thr " << i << ": " << bufs[i].size() << '\n';
		// 	}
		// 	stat_out << "mcts: " << buf_main.size() << ' ' << mx << std::endl;
		// }
		

		if (auto re = tre.root->find_best_order(0, k)) {
			if (meta.find("skip") != meta.end()) {
				update_time(begin);
				return *re;
			}

			if (stat) {
				auto nd = tre.root->child[tre.root->get_index(*re)];
				stat_out << "win rate: " << 1.0 - float(nd->win) / nd->visit << std::endl;
			}

			// if (stat) {
			// 	auto& r = tre.root;
			// 	auto v1 = r->child[r->get_index(*r->find_best_order(0))]->visit;
			// 	auto v2 = r->child[r->get_index(*r->find_best_order(1))]->visit;
			// 	stat_out << v1 << ' ' << v2 << std::endl;
			// }
			
			/*
				reuse preallocation for after
			*/
			reallocate_after(*re);

			/*
				generate threads for after mcts
			*/
			is_thread_alive = true;
			for (auto i = 0u; i < thread_size; ++i) {
				afters.push_back(std::thread(&tree::run_mcts_after, tre, std::ref(is_thread_alive), *re, std::ref(gens[i]), std::ref(bufs[i]), c, k));
			}

			/*
				calc time elp
			*/
			update_time(begin);
			return *re;
		}
		update_time(begin);
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
		if (stat && move_count) {
			stat_out << move_count << " moves in " << time_elp << " ms\n";
			stat_out << float(time_elp) / move_count << " ms/move\n";
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
	float time = 60; // total available time
	float c_time = 10; // dividing factor on time
	float max_ply_mul = 1.3;
	int max_ply = 14; // the move require most time
	float mcts_per_ms = 210;
	// float mcts_per_ms = 3;
	
	/*
		MCTS tree
	*/
	tree tre;

	/*
		objects in parellel
	*/
	std::size_t thread_size = 14; // # of thread used
	bool is_thread_alive = false; // switch to kill while-true thread
	std::size_t reserve = 2000000; // buffer size aka expected number of mcts per move
	std::size_t reserve_main = 15000000;
	std::vector<std::default_random_engine> gens; // random generator for each thread
	std::vector<std::vector<node>> bufs; // node buffer for each thread
	std::vector<node> buf_main; // node buffer for inherence
	std::vector<std::thread> afters; // after mcts

	/*
		statistics
	*/
	bool demo = false;
	bool stat = false;
	int move_count = 0;
	int time_elp = 0;
	std::ofstream stat_out;
};
