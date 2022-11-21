/**
 * Framework for Threes! and its variants (C++ 11)
 * weight.h: Lookup table template for n-tuple network
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include "board.h"
#include <iostream>
#include <vector>
#include <utility>
#include <fstream>

class weight {
public:
	typedef float type;
	typedef std::vector<board::cell> pattern;

public:
	weight() {}
	weight(size_t len) : value(len) {}
	weight(weight&& f) : isos(std::move(f.isos)), value(std::move(f.value)) {}
	weight(const weight& f) = default;
	weight(const std::vector<board::cell>& vec) {
		set_pattern(vec);
		value.resize(1 << (vec.size() << 1));
		std::fill(value.begin(), value.end(), 0.0);
	}

	void check() {
		auto it = std::max_element(value.begin(), value.end());
		std::cout << "max " << it - value.begin() << ' ' << *it << '\n';
		it = std::min_element(value.begin(), value.end());
		std::cout << "min " << it - value.begin() << ' ' << *it << '\n';
		// std::cout << "value size " << value.size() << '\n';
		// std::cout << "iso size " << isos.size() << '\n';
		// // std::cout << "see see non-zero\n";
		// for (unsigned int i = 0; i < value.size(); ++i) {
		// 	// if (value[i] != 0) std::cout << i << ' ' << value[i] << '\n';
		// 	if (value[i] > 1e9) std::cout << "inf!!!!";
		// }
		// std::cout << '\n';
	}

	weight& operator =(const weight& f) = default;
	type& operator[] (size_t i) { return value[i]; }
	const type& operator[] (size_t i) const { return value[i]; }
	size_t size() const { return value.size(); }

protected:
	std::size_t get_index(const board& brd, const pattern& pat) const {
		std::size_t re = 0;
		for (unsigned int i = 0; i < pat.size(); ++i) {
			re |= brd(pat[i]) << (i << 1);
		}
		return re;
	}
public:
	void set_pattern(const std::vector<board::cell>& vec) {
		isos.clear();
		board brd = board::index_board();
		for (int i = 0; i < 4; ++i) {
			pattern cur;
			for (auto v : vec) {
				cur.push_back(brd(v));
			}
			isos.push_back(cur);
			brd.rotate_right();
		}
		brd.anti_transpose();
		for (int i = 0; i < 4; ++i) {
			pattern cur;
			for (auto v : vec) {
				cur.push_back(brd(v));
			}
			isos.push_back(cur);
			brd.rotate_right();
		}
	}

	type get_weight(const board& brd) const {
		type val = 0;
		for (const auto& pat : isos) {
			val += value[get_index(brd, pat)];
		}
		return val / static_cast<float>(isos.size());
	}

	void update(const board& brd, type err) {
		// if (err != 0) {
		// 	std::cout << "wei update yes " << err << " new " << static_cast<type>(isos.size()) << ' ' << err / static_cast<type>(isos.size()) << '\n';
		// }
		type nerr = err / static_cast<type>(isos.size());
		for (const auto& pat : isos) {
			value[get_index(brd, pat)] += nerr;
			// if (value[get_index(brd, pat)])
			// std::cout << "update non-zero value " << value[get_index(brd, pat)] << '\n';
		}
	}

public:
	friend std::ostream& operator <<(std::ostream& out, const weight& w) {
		auto& value = w.value;
		uint64_t size = value.size();
		out.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
		out.write(reinterpret_cast<const char*>(value.data()), sizeof(type) * size);
		return out;
	}
	friend std::istream& operator >>(std::istream& in, weight& w) {
		auto& value = w.value;
		uint64_t size = 0;
		in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
		value.resize(size);
		in.read(reinterpret_cast<char*>(value.data()), sizeof(type) * size);
		return in;
	}

protected:

	// pattern pat; // the n-tuple
	std::vector<pattern> isos; // 8 isomorphisms
	std::vector<type> value; // type 4 bits per cell to store the index
};
