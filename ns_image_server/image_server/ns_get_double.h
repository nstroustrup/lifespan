#ifndef NS_GET_DOUBLE_H
#define NS_GET_DOUBLE_H
#include <iostream>
#include <string>
#include <stdlib.h>
class ns_get_int{
	public:
	inline char operator()(std::istream & in, std::string & d){
		d.resize(0);
		while(true){
			char a = in.get();
			if (a == ',' || a == '\n' || in.fail())
				return a;
			else d+=a;
		}
	}
	template<class T>
		inline char operator()(std::istream & in,T & i){
			char a((*this)(in,tmp));
			i = static_cast<T>(atol(tmp.c_str()));
			return a;
		}
       
	private:
	std::string tmp;
};
template<>
inline char ns_get_int::operator()(std::istream & in, ns_64_bit & i) {
  char a((*this)(in, tmp));
  i = static_cast<ns_64_bit>(ns_atoi64(tmp.c_str()));
  return a;
}
template<>
inline char ns_get_int::operator()(std::istream & in, ns_s64_bit & i) {
  char a((*this)(in, tmp));
  i = static_cast<ns_s64_bit>(ns_atois64(tmp.c_str()));
  return a;
}

class ns_get_double{
	public:
	inline char operator()(std::istream & in, std::string & d){
		d.resize(0);
		while(true){
			char a = in.get();
			if (a == ',' || a == '\n' || in.fail())
				return a;
			else d+=a;
		}
	}
	template<class T>
		inline char operator()(std::istream & in,T & i){
			char a((*this)(in,tmp));
			i = atof(tmp.c_str());
			return a;
		}
	private:
	std::string tmp;
};
#endif
