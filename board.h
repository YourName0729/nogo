/**
 * Framework for NoGo and similar games (C++ 11)
 * board.h: Define the game state and basic operations of the game of NoGo
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <array>
#include <list>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <cmath>
#include <random>
#include <queue>
#include <optional>
#include "uint128.h"

/**
 * definition for the 9x9 board
 * note that there is no column 'I'
 *
 *   A B C D E F G H J
 * 9 + + + + + + + + + 9
 * 8 + + + +   + + + + 8
 * 7 + + + +   + + + + 7
 * 6 + + + + + + + + + 6
 * 5 +     + + +     + 5
 * 4 + + + + + + + + + 4
 * 3 + + + +   + + + + 3
 * 2 + + + +   + + + + 2
 * 1 + + + + + + + + + 1
 *   A B C D E F G H J
 *
 * GTP style is operated as (move):
 *   "A1", "B3", ..., "H4", ..., "J9"
 * 1-d array style is operated as (i):
 *   (0) == "A1", (11) == "B3", ..., (66) == "H4", (80) == "J9"
 * 2-d array style is operated as [x][y]:
 *   [0][0] == "A1", [1][2] == "B3", [7][3] == "H4", [8][8] == "J9"
 *
 * for 9x9 Hollow NoGo, the empty locations are hollow but not empty, cannot be counted as liberty,
 * i.e., there are also borders at the center of the board
 */
class board {
public:
	enum size { size_x = 9u, size_y = 9u, hollow_x = 3u, hollow_y = 3u };
	enum piece_type { empty = 0u, black = 1u, white = 2u, hollow = 3u, unknown = -1u };
	// typedef uint32_t cell;
	// typedef std::array<cell, size_y> column;
	// typedef std::array<column, size_x> grid;
	struct data {
		piece_type who_take_turns;
	};
	typedef uint64_t score;
	typedef int reward;
	typedef uint128 bitboard;

public:
	board() : attr({piece_type::black}) { 
		brds[1] = brds[2] = 0;
		avl[1] = avl[2] = board_mask;
	}
	// board(const grid& b, const data& d) : stone(b), attr(d) {}
	board(const board& b) = default;
	board& operator =(const board& b) = default;

	struct point {
		int x, y, i;
		point(int i = -1) : x(i != -1 ? i / size_y : -1), y(i != -1 ? i % size_y : -1), i(i) {}
		point(int x, int y) : x(x), y(y), i(x != -1 && y != -1 ? x * size_y + y : -1) {}
		point(const std::string& name) : point(
			name.size() >= 2 && name != "PASS" ? name[0] - (name[0] > 'I' ? 'B' : 'A') : -1,
			name.size() >= 2 && std::isdigit(name[1]) ? std::stoul(name.substr(1)) - 1 : -1) {}
		point(const char* name) : point(std::string(name)) {}
		point(const point&) = default;
		operator std::string() const {
			if (i == -1) return "PASS";
			if (x >= size_x || y >= size_y) return "??";
			return std::string(1, x + (x < 8 ? 'A' : 'B')) + std::to_string(y + 1);
		}
	};

	// operator grid&() { return stone; }
	// operator const grid&() const { return stone; }
	// column& operator [](unsigned x) { return stone[x]; }
	// const column& operator [](unsigned x) const { return stone[x]; }
	// cell& operator ()(unsigned i) { point p(i); return stone[p.x][p.y]; }
	// const cell& operator ()(unsigned i) const { point p(i); return stone[p.x][p.y]; }
	// cell& operator ()(const std::string& move) { point p(move); return stone[p.x][p.y]; }
	// const cell& operator ()(const std::string& move) const { point p(move); return stone[p.x][p.y]; }

	// operator grid&() { return stone; }
	// operator const grid&() const { return stone; }
	// column& operator [](unsigned x) { return stone[x]; }
	// const column& operator [](unsigned x) const { return stone[x]; }
	// cell& operator ()(unsigned i) { point p(i); return stone[p.x][p.y]; }
	// const cell& operator ()(unsigned i) const { point p(i); return stone[p.x][p.y]; }
	// cell& operator ()(const std::string& move) { point p(move); return stone[p.x][p.y]; }
	// const cell& operator ()(const std::string& move) const { point p(move); return stone[p.x][p.y]; }

	data info() const { return attr; }
	data info(data dat) { data old = attr; attr = dat; return old; }

public:
	// bool operator ==(const board& b) const { return stone == b.stone; }
	// bool operator < (const board& b) const { return stone <  b.stone; }
	// bool operator !=(const board& b) const { return !(*this == b); }
	// bool operator > (const board& b) const { return b < *this; }
	// bool operator <=(const board& b) const { return !(b < *this); }
	// bool operator >=(const board& b) const { return !(*this < b); }

	bool operator ==(const board& b) const { return brds[1] == b.brds[1] && brds[2] == b.brds[2]; }
	bool operator < (const board& b) const { return (brds[1] == b.brds[1])? (brds[2] < b.brds[2]) : (brds[1] < b.brds[1]); }
	bool operator !=(const board& b) const { return !(*this == b); }
	bool operator > (const board& b) const { return b < *this; }
	bool operator <=(const board& b) const { return !(b < *this); }
	bool operator >=(const board& b) const { return !(*this < b); } 

public:
	// basic uint128 bitoperation 
	static constexpr uint128 lsb(uint128 v) { return (v & -v); }
	static constexpr uint128 reset(uint128 v) { return (v & (v - 1)); }

	// the bit index, ensure v has only one 1 bit
	static int bit_scan(uint128 v) {
		int re = 0, step = 64;
		while (step) {
			if (v >> step) re += step, v >>= step;
			step >>= 1;
		}
		return re;
	}

	static int bit_count(uint128 v) {
		const constexpr uint128 flt1 = make_uint128(0x5555555555555555, 0x5555555555555555);
		const constexpr uint128 flt2 = make_uint128(0x3333333333333333, 0x3333333333333333);
		const constexpr uint128 flt3 = make_uint128(0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f);
		const constexpr uint128 mul  = make_uint128(0x0101010101010101, 0x0101010101010101);
		// const constexpr uint128 flt4 = make_uint128(0x00ff00ff00ff00ff, 0x00ff00ff00ff00ff);
		// const constexpr uint128 flt5 = make_uint128(0x0000ffff0000ffff, 0x0000ffff0000ffff);
		v = ((v & flt1)) + ((v >> 1) & flt1);
		v = ((v & flt2)) + ((v >> 2) & flt2);
		v = ((v & flt3)) + ((v >> 4) & flt3);
		return (v * mul) >> 120;
	}

private:
	// helper functions of uint128
	static constexpr uint128 shift_up(uint128 v) { return v >> 9; }
	static constexpr uint128 shift_down(uint128 v) { return (v << 9) & board_mask; }
	static constexpr uint128 shift_right(uint128 v) { return (v << 1) & ~left_mask & board_mask; }
	static constexpr uint128 shift_left(uint128 v) { return (v >> 1) & ~right_mask; }

	// flow to U, D, L, R one step
	static constexpr uint128 flow1(uint128 v) { return v | shift_down(v) | shift_left(v) | shift_up(v) | shift_right(v); }

	// i-th bit as bool
	static constexpr bool bit_ith(uint128 v, int idx) { return (v >> idx) & 1u;}

	// connected components of center v, boundry brd
	static uint128 cnnt_comp(uint128 v, uint128 brd) {
		uint128 v2 = v;
		do {
			v = v2;
			v2 = flow1(v) & brd;
		} while (v != v2);
		return v2;
	}



	// libreties of connected component cc, boundry brd(hollow or oppnent)
	static uint128 libreties(uint128 cc, uint128 brd) { return ((flow1(cc) ^ cc) | brd) ^ brd; }
	// return true when v has exactly one 1 bit
	static bool count_one(uint128 v) { return v == lsb(v); }

	void update_librety(bitboard bb, int who) {
		// 3 types of removing available
		// 1. remove bb itself from both avl
		// 2. the connected component of who containing bb remains one librety, say lib
		//     remove lib from avl[opp]
		// 	   remove lib from avl[who] if fill it will be dead
		// 3. the connected component of opp besides bb remains one librety, say lib
		//     remove lib from avl[who]
		//	   remove lib from avl[opp] if fill it will be dead
		// 4. the empty besides bb becomes a last librety of opp(eye or last librety)
		// 	   remove it from avl[opp]

		// type 1
		avl[1] ^= (avl[1] & bb);
		avl[2] ^= (avl[2] & bb);
		// type 2
		int opp = opponent(who);
		{
			uint128 cc = cnnt_comp(bb, brds[who]);
			uint128 lib = libreties(cc, brds[opp] | hollow_mask);
			if (count_one(lib)) {
				avl[opp] ^= (avl[opp] & lib);
				if (!libreties(cnnt_comp(cc | lib, brds[who] | lib), brds[opp] | hollow_mask)) {
					avl[who] ^= (avl[who] & lib);
				}
			}
		}
		// type 3 and 4
		uint128 emps = (board_mask ^ brds[1]) ^ brds[2];
		auto upd = [&](uint128 bb) {
			if (bb & brds[opp]) {
				uint128 cc = cnnt_comp(bb, brds[opp]);
				uint128 lib = libreties(cc, brds[who] | hollow_mask);
				if (count_one(lib)) {
					avl[who] ^= (avl[who] & lib);
					if (!libreties(cnnt_comp(cc | lib, brds[opp] | lib), brds[who] | hollow_mask)) {
						avl[opp] ^= (avl[opp] & lib);
					} 
				} 
			}
			if ((bb & emps) && (bb & avl[opp])) {
				if (!libreties(cnnt_comp(bb, brds[opp] | bb), brds[who] | hollow_mask)) {
					avl[opp] ^= (avl[opp] & bb);
				}
			}
		};
		if (!(bb & up_mask)) {
			uint128 bb_sft = shift_up(bb);
			upd(bb_sft);
		}
		if (!(bb & down_mask)) {
			uint128 bb_sft = shift_down(bb);
			upd(bb_sft);
		} 
		if (!(bb & left_mask)) {
			uint128 bb_sft = shift_left(bb);
			upd(bb_sft);
		} 
		if (!(bb & right_mask)) {
			uint128 bb_sft = shift_right(bb);
			upd(bb_sft);
		}

	}

public:
	enum nogo_move_result {
		legal = reward(0),
		illegal_turn = reward(-1),
		illegal_pass = reward(-2),
		illegal_out_of_range = reward(-3),
		illegal_not_empty = reward(-4),
		illegal_suicide = reward(-5),
		illegal_take = reward(-6),
	};

	piece_type at(int x, int y) const {
		int idx = x * size_y + y;
		bitboard bb = 1;
		bb <<= idx;
		if (brds[1] & brds[2]) std::cout << "overlap!\n";
		if (brds[1] & hollow_mask) std::cout << "overlap black hollow!\n";
		if (brds[2] & hollow_mask) std::cout << "overlap white hollow!\n";
		if (bb & brds[1]) return piece_type::black;
		if (bb & brds[2]) return piece_type::white;
		if (bb & hollow_mask) return piece_type::hollow;
		return piece_type::empty;
	}

	void set(int x, int y, piece_type who) {

	}

	// index of move
	std::optional<int> random_action(std::default_random_engine& gen) {
		auto av = avl[info().who_take_turns];
		if (!av) return std::nullopt;
		auto idx = std::uniform_int_distribution<>(0, bit_count(av) - 1)(gen);
		while (idx--) av = reset(av);
		return bit_scan(lsb(av));
	}

	/**
	 * place a stone to the specific position
	 * who == piece_type::unknown indicates automatically play as the next side
	 * return nogo_move_result::legal if the action is valid, or nogo_move_result::illegal_* if not
	 */
	// reward place(int x, int y, unsigned who = piece_type::unknown) {
	// 	if (who == -1u) who = attr.who_take_turns;
	// 	if (who != attr.who_take_turns) return nogo_move_result::illegal_turn;
	// 	if (x == -1 && y == -1) return nogo_move_result::illegal_pass;
	// 	point p_min(0, 0), p_max(size_x - 1, size_y - 1);
	// 	if (x < p_min.x || x > p_max.x || y < p_min.y || y > p_max.y) return nogo_move_result::illegal_out_of_range;
	// 	if (board::initial()[x][y] == piece_type::hollow)             return nogo_move_result::illegal_out_of_range;
	// 	board test = *this;
	// 	if (test[x][y] != piece_type::empty) return nogo_move_result::illegal_not_empty;
	// 	test[x][y] = who; // try put a piece first
	// 	if (test.check_liberty(x, y, who) == 0) return nogo_move_result::illegal_suicide;
	// 	unsigned opp = 3u - who;
	// 	if (x > p_min.x && test.check_liberty(x - 1, y, opp) == 0) return nogo_move_result::illegal_take;
	// 	if (x < p_max.x && test.check_liberty(x + 1, y, opp) == 0) return nogo_move_result::illegal_take;
	// 	if (y > p_min.y && test.check_liberty(x, y - 1, opp) == 0) return nogo_move_result::illegal_take;
	// 	if (y < p_max.y && test.check_liberty(x, y + 1, opp) == 0) return nogo_move_result::illegal_take;
	// 	stone[x][y] = who; // is legal move!
	// 	attr.who_take_turns = static_cast<piece_type>(opp);
	// 	return nogo_move_result::legal;
	// }
	// reward place(const point& p, unsigned who = piece_type::unknown) {
	// 	return place(p.x, p.y, who);
	// }

	static unsigned opponent(unsigned who) {
		return who ^ 0x3u;
	}

	reward place(uint128 bb, unsigned who = piece_type::unknown) {
		// to check correct role moving
		if (who == -1u) who = attr.who_take_turns;
		if (who != attr.who_take_turns) return nogo_move_result::illegal_turn;
		// to check bb is only one bit
		// to check bb is on the legal board(on the board and not occupied)
		if (!(bb & board_mask)) return nogo_move_result::illegal_out_of_range;
		if ((bb & brds[1]) || (bb & brds[2])) return nogo_move_result::illegal_not_empty;
		// to check available(not suicide or take)
		if (!(bb & avl[who])) return nogo_move_result::illegal_suicide;
		// real place
		brds[who] |= bb;
		// TODO update avl
		update_librety(bb, who);
		attr.who_take_turns = static_cast<piece_type>(opponent(who));
		return nogo_move_result::legal;
	}

	reward place(int i, unsigned who = piece_type::unknown) {
		return place(point(i), who);
	}

	reward place(int x, int y, unsigned who = piece_type::unknown) {
		bitboard bb = 1;
		bb <<= (x * size_y + y);

		return place(bb, who);
	}

	reward place(const point& p, unsigned who = piece_type::unknown) {
		return place(p.x, p.y, who);
	}

	uint128 available() const { return avl[info().who_take_turns]; }

	uint128 available(unsigned who) const {
		return avl[who];
	}

	uint128 find_move(const board& b) const {
		auto who = info().who_take_turns;
		return b.brds[who] ^ brds[who];
	}

	int find_move_index(const board& b) const {
		return bit_scan(find_move(b));
	}

	/**
	 * calculate the liberty of the block of piece at [x][y]
	 * return >= 0 if [x][y] is placed by who; otherwise return -1
	 */
	// int check_liberty(int x, int y, unsigned who) const {
	// 	grid test = stone;
	// 	if (test[x][y] != who) return -1;

	// 	int liberty = 0;
	// 	std::list<point> check;
	// 	for (check.emplace_back(x, y); check.size(); check.pop_front()) {
	// 		int x = check.front().x, y = check.front().y;
	// 		test[x][y] = piece_type::unknown; // prevent recalculate

	// 		point p_min(0, 0), p_max(size_x - 1, size_y - 1);

	// 		cell near_l = x > p_min.x ? test[x - 1][y] : -1u; // left
	// 		if (near_l == piece_type::empty) liberty++;
	// 		else if (near_l == who) check.emplace_back(x - 1, y);

	// 		cell near_r = x < p_max.x ? test[x + 1][y] : -1u; // right
	// 		if (near_r == piece_type::empty) liberty++;
	// 		else if (near_r == who) check.emplace_back(x + 1, y);

	// 		cell near_d = y > p_min.y ? test[x][y - 1] : -1u; // down
	// 		if (near_d == piece_type::empty) liberty++;
	// 		else if (near_d == who) check.emplace_back(x, y - 1);

	// 		cell near_u = y < p_max.y ? test[x][y + 1] : -1u; // up
	// 		if (near_u == piece_type::empty) liberty++;
	// 		else if (near_u == who) check.emplace_back(x, y + 1);
	// 	}
	// 	return liberty;
	// }

	// void transpose() {
	// 	for (int x = 0; x < size_x; x++) {
	// 		for (int y = x + 1; y < size_y; y++) {
	// 			std::swap(stone[x][y], stone[y][x]);
	// 		}
	// 	}
	// }

	// void anti_transpose() {
	// 	for (int r = 0; r < size_x; ++r) {
	// 		for (int c = 0; c < size_y - r; ++c) {
	// 			std::swap(stone[r][c], stone[9 - c][9 - r]);
	// 		}
	// 	}
	// }

	// void reflect_horizontal() {
	// 	for (int y = 0; y < size_y; y++) {
	// 		for (int x = 0; x < size_x / 2; x++) {
	// 			std::swap(stone[x][y], stone[size_x - 1 - x][y]);
	// 		}
	// 	}
	// }

	// void reflect_vertical() {
	// 	for (int x = 0; x < size_x; x++) {
	// 		for (int y = 0; y < size_y / 2; y++) {
	// 			std::swap(stone[x][y], stone[x][size_y - 1 - y]);
	// 		}
	// 	}
	// }

	// /**
	//  * rotate the board clockwise by given times
	//  */
	// void rotate(int r = 1) {
	// 	switch (((r % 4) + 4) % 4) {
	// 	default:
	// 	case 0: break;
	// 	case 1: rotate_right(); break;
	// 	case 2: reverse(); break;
	// 	case 3: rotate_left(); break;
	// 	}
	// }

	// void rotate_right() { transpose(); reflect_vertical(); } // clockwise
	// void rotate_left() { transpose(); reflect_horizontal(); } // counterclockwise
	// void reverse() { reflect_horizontal(); reflect_vertical(); }
public:
	void show() const {
		for (unsigned int i = 0; i < size_x; ++i) {
			for (unsigned int j = 0; j < size_y; ++j) {
				std::cout << (".Ox "[at(i, j)]) << ' ';
			}
			std::cout << '\n';
		}
	}

	static void show(uint128 v) {
		for (unsigned int i = 0; i < size_x; ++i) {
			for (unsigned int j = 0; j < size_y; ++j) {
				int idx = i * size_y + j;
				int val = (v >> idx) & 1;
				std::cout << (".O"[val]);
			}
			std::cout << '\n';
		}
	}

	void debug() {
	
		std::cout << "b avl\n";
		show(avl[1]);
		std::cout << "w avl\n";
		show(avl[2]);
	}

	std::array<std::array<int, 9>, 9> to_array() {
		std::array<std::array<int, 9>, 9> re;
		for (int i = 0; i < size_x; ++i) {
			for (int j = 0; j < size_y; ++j) {
				re[i][j] = at(i, j);
			}
		}
		return re;
	}

	using barr = std::array<std::array<int, 9>, 9>;
	int check_lib(const barr& arr, int sx, int sy) {
		int dx[] = {1, 0, -1, 0}, dy[] = {0, 1, 0, -1};
		auto a = arr;
		std::queue<std::pair<int, int>> que;
		que.push({sx, sy});
		int who = a[sx][sy];
		a[sx][sy] = piece_type::unknown;
		int lib = 0;
		while (que.size()) {
			auto [x, y] = que.front();
			// std::cout << x << ' ' << y << '\n';
			que.pop();
			for (int i = 0; i < 4; ++i) {
				int nx = x + dx[i], ny = y + dy[i];
				if (0 <= nx && nx < size_x && 0 <= ny && ny < size_y) {
					if (a[nx][ny] == piece_type::empty) {
						++lib;
						a[nx][ny] = piece_type::unknown;
					}
					else if (a[nx][ny] == who) {
						que.push({nx, ny});
						// std::cout << "push " << nx << ' ' << ny << ' ' << who << '\n';
						a[nx][ny] = piece_type::unknown;
					}
				}
			}
			// for (int i = 0; i < size_x; ++i) {
			// 	for (int j = 0; j < size_y; ++j) {
			// 		std::cout << "=.OX "[arr[i][j] + 1];
			// 	}
			// 	std::cout << '\n';
			// }
		}
		// std::cout << "lib " << sx << ' ' << sy << ' ' << who << " = " << lib << '\n';
		return lib;
	}

	bool placable(const barr& arr, int x, int y, int who) {
		if (arr[x][y] != piece_type::empty) return false;
		auto arr2 = arr;
		arr2[x][y] = who;
		if (!check_lib(arr2, x, y)) return false;
		int opp = opponent(who);
		if (x < size_x - 1 && arr2[x + 1][y] == opp && !check_lib(arr2, x + 1, y)) return false;
		if (x != 0 && arr2[x - 1][y] == opp && !check_lib(arr2, x - 1, y)) return false;
		if (y < size_x - 1 && arr2[x][y + 1] == opp && !check_lib(arr2, x, y + 1)) return false;
		if (y != 0 && arr2[x][y - 1] == opp && !check_lib(arr2, x, y - 1)) return false;
		return true;
	}

	void round(int x, int y) {
		unsigned who = info().who_take_turns;
		std::cout << " BW"[who] << ' ' << who << " take turn\n";
		auto arr = to_array();
		for (int i = 0; i < size_x; ++i) {
			for (int j = 0; j < size_y; ++j) {
				std::cout << ".OX "[arr[i][j]];
			}
			std::cout << '\n';
		}
		debug();
		std::cout << " BW"[who] << " place " << x << ' ' << y << '\n';
		
		if (arr[x][y] != piece_type::empty) {
			std::cout << "nonempty!!!!!!!" << arr[x][y] << "\n";
		}

		place(x, y, who);
	}

	void test() {
		round(0, 0);
		round(0, 1);
		round(1, 1);
	}

	void start() {
		std::random_device rd;
		std::default_random_engine gen(rd());
		while (avl[info().who_take_turns]) {
			std::cout << "------------------------------\n";
			std::vector<int> vec;
			unsigned who = info().who_take_turns;
			std::cout << " BW"[who] << ' ' << who << " take turn\n";
			auto arr = to_array();
			for (int i = 0; i < size_x; ++i) {
				for (int j = 0; j < size_y; ++j) {
					std::cout << ".OX "[arr[i][j]];
				}
				std::cout << '\n';
			}
			debug();
			for (int i = 0; i < size_x; ++i) {
				for (int j = 0; j < size_y; ++j) {
					if (placable(arr, i, j, 1) != bit_ith(avl[1], i * size_y + j)) {
						std::cout << "WRONG PLACABLE!!! " << i << ' ' << j << ' ' << 'B' << '\n';
						exit(0);
					}
					if (placable(arr, i, j, 2) != bit_ith(avl[2], i * size_y + j)) {
						std::cout << "WRONG PLACABLE!!! " << i << ' ' << j << ' ' << 'W' << '\n';
						exit(0);
					}
				}
			}
			for (uint128 v = avl[who]; v; v = reset(v)) {
				vec.push_back(bit_scan(v));
			}
			std::shuffle(vec.begin(), vec.end(), gen);
			uint128 bb = 1;
			bb <<= vec[0];
			int x = vec[0] / size_y, y = vec[0] % size_y;
			std::cout << " BW"[who] << " place " << x << ' ' << y << '\n';
			
			if (arr[x][y] != piece_type::empty) {
				std::cout << "nonempty!!!!!!!" << arr[x][y] << "\n";
			}

			place(bb, who);
		}
	}

	// static board index_board() {
	// 	board re;
	// 	for (int i = 0; i < size_x * size_y; ++i) {
	// 		re(i) = i;
	// 	}
	// 	return re;
	// }

public:
	

	friend std::ostream& operator <<(std::ostream& out, const board& b) {
		std::cout << "outputboard\n";
		std::ios ff(nullptr);
		ff.copyfmt(out); // make a copy of the original print format

		const char* axis_x_label = "ABCDEFGHJKLMNOPQRST?";
		int width_y = size_y < 10 ? 1 : 2;

		out << std::setw(width_y) << ' ';
		for (int x = 0; x < size_x; x++)
			out << ' ' << axis_x_label[std::min(x, 19)];
		out << ' ' << std::setw(width_y) << ' ' << std::endl;

		// for displaying { space, black, white }
		const char* print[] = {"\u00B7" /* or \u00A0 */, "\u25CF", "\u25CB", "\u00A0", "?"};
		for (int y = size_y - 1; y >= 0; y--) {
			out << std::right << std::setw(width_y) << (y + 1);
			for (int x = 0; x < size_x; x++)
				out << ' ' << print[std::min(static_cast<uint32_t>(b.at(x, y)), 4u)];
			out << ' ' << std::left << std::setw(width_y) << (y + 1) << std::endl;
		}

		out << std::setw(width_y) << ' ';
		for (int x = 0; x < size_x; x++)
			out << ' ' << axis_x_label[std::min(x, 19)];
		out << ' ' << std::setw(width_y) << ' ' << std::endl;

		out.copyfmt(ff); // restore print format
		return out;
	}
	// friend std::istream& operator >>(std::istream& in, board& b) {
	// 	std::string token;
	// 	for (int x = 0; x < size_x; x++) in >> token; /* skip X */
	// 	for (int y = size_y - 1; y >= 0 && in >> token /* skip Y */; in >> token /* skip Y */, y--) {
	// 		for (int x = 0; x < size_x && in >> token /* read a piece */; x++) {
	// 			const char* print[] = {"\u00B7" /* or \u00A0 */, "\u25CF", "\u25CB", "\u00A0"};
	// 			int type = -1;
	// 			for (int i = 0; type == -1 && i < 4; i++) {
	// 				if (token == print[i]) type = i;
	// 			}
	// 			if (type != -1) {
	// 				b[x][y] = static_cast<piece_type>(type);
	// 			} else {
	// 				in.setstate(std::ios_base::failbit);
	// 				return in;
	// 			}
	// 		}
	// 	}
	// 	for (int x = 0; x < size_x; x++) in >> token; /* skip X */
	// 	return in;
	// }
	friend std::ostream& operator <<(std::ostream& out, const point& p) {
		return out << std::string(p);
	}
	friend std::istream& operator >>(std::istream& in, point& p) {
		std::string name;
		if (in >> name) p = point(name);
		return in;
	}

protected:
	// static const grid& initial() { static grid stone; return stone; }

	// static __attribute__((constructor)) void init_initial_scheme() {
	// 	grid& stone = const_cast<grid&>(initial());
	// 	stone[4][1] = piece_type::hollow;
	// 	stone[4][2] = piece_type::hollow;
	// 	stone[4][6] = piece_type::hollow;
	// 	stone[4][7] = piece_type::hollow;
	// 	stone[1][4] = piece_type::hollow;
	// 	stone[2][4] = piece_type::hollow;
	// 	stone[6][4] = piece_type::hollow;
	// 	stone[7][4] = piece_type::hollow;
	// }
private:
	static constexpr bitboard up_mask     = make_uint128(0,0x1ff); // 1 wehn uppest row
	static constexpr bitboard down_mask   = make_uint128(0x1ff00,0); // 1 when downest row
	static constexpr bitboard left_mask   = make_uint128(0x100,0x8040201008040201); // 1 when leftest column
	static constexpr bitboard right_mask  = make_uint128(0x10080,0x4020100804020100); // 1 when rightest column

	static constexpr bitboard board_mask  = make_uint128(0x1FFF7,0xFBFFF39FFFBFDFFF); // 1 when placable
	static constexpr bitboard hollow_mask = make_uint128(0x8,0x04000C6000402000); // 1 when hollow

	bitboard brds[3]; // [1]: black, [2]: white
	bitboard avl[3]; // [1]: black's available moves, [2]: white's available moves
	// grid stone;
	data attr;
};
