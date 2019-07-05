#pragma once
#include <fstream>
#include "gzstream.h"
//allow either compressed or uncompressed file io
struct ns_istream {
	ns_istream(std::ifstream* s) : s1(s), s2(0) {}
	ns_istream(igzstream* s) : s1(0), s2(s) {}
	std::istream& operator()() {
		if (s1 != 0) return *s1;
		else		 return *s2;
	}
	~ns_istream() { destroy(); }
private:
	std::ifstream* s1;
	igzstream* s2;

	void destroy() {
		if (s1 != 0) {
			s1->close();
			ns_safe_delete(s1);
		}
		if (s2 != 0)
			ns_safe_delete(s2);
	}
};

struct ns_ostream {
	ns_ostream(std::ofstream* s) : s1(s), s2(0) {}
	ns_ostream(ogzstream* s) : s1(0), s2(s) {}
	std::ostream& operator()() {
		if (s1 != 0) return *s1;
		else		 return *s2;
	}
	~ns_ostream() { destroy(); }
private:
	std::ofstream* s1;
	ogzstream* s2;

	void destroy() {
		if (s1 != 0) {
			s1->close();
			ns_safe_delete(s1);
		}
		if (s2 != 0)
			ns_safe_delete(s2);
	}
};
