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
			i = ns_atoi64(tmp.c_str());
			return a;
		}
	private:
	std::string tmp;
};

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
