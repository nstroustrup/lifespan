#ifndef NS_VECTOR_H
#define NS_VECTOR_H
#include "ns_ex.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>

#define ns_pi 3.141592653589796

template <class T>
class ns_swap{
public:
	inline void operator()(T & l, T & r){
		temp = l;
		l = r;
		r = temp;
	}
	T temp;
};


template<class ns_component>
class ns_vector_2{
public:
	typedef ns_component component_type;

	ns_vector_2(ns_component _x, ns_component _y):x(_x),y(_y){}
	ns_vector_2(){}
	template<class ns_component_2>
	ns_vector_2(const ns_vector_2<ns_component_2> & r):x(r.x),y(r.y){}
	ns_component x,y;
	const inline ns_component squared() const{
		return x*x + y*y;
	}	
	const inline double mag() const{
		return sqrt((double)squared());
	}
	double angle() const{
		return atan2(y,x);
	}

	template<class ns_component2>
	const ns_vector_2<ns_component> operator/(const ns_component2 & div) const{
		return ns_vector_2<ns_component>((ns_component)(x/div),(ns_component)(y/div));
	}			
	template<class ns_component2>
	const ns_vector_2<ns_component> operator*(const ns_component2 & div) const{
		return ns_vector_2<ns_component>((ns_component)(x*div),(ns_component)(y*div));
	}
	template<class vector_t>
	const ns_vector_2<ns_component> operator+(const vector_t & r) const{
		return ns_vector_2<ns_component>(x+(ns_component)r.x,y+(ns_component)r.y);
	}	
	template<class vector_t>
	const ns_vector_2<ns_component> operator-(const vector_t & r) const{
		return ns_vector_2<ns_component>(x-(ns_component)r.x,y-(ns_component)r.y);
	}		
	template<class vector_t>
	const ns_vector_2<ns_component> & operator+=(const vector_t & r){
		x+=(ns_component)r.x;
		y+=(ns_component)r.y;
		return *this;
	}
	template<class vector_t>
	const ns_vector_2<ns_component> & operator-=(const vector_t & r){
		x-=(ns_component)r.x;
		y-=(ns_component)r.y;
		return *this;
	}

	template<class ns_component2>
	const ns_vector_2<ns_component> operator/=(const ns_component2 & div){
		x/=div;
		y/=div;
		return *this;
	}	

	template<class ns_component2>
	const ns_vector_2<ns_component> operator*=(const ns_component2 & div){
		x*=(ns_component)div;
		y*=(ns_component)div;
		return *this;
	}

	template<class ns_component2>
	ns_vector_2<ns_component> element_multiply(const ns_vector_2<ns_component2> & v) const{
		return ns_vector_2<ns_component>(x*(ns_component)v.x,y*(ns_component)v.y);
	}

};
template<class ns_component>
bool operator < (const ns_vector_2<ns_component> &l, const ns_vector_2<ns_component> &r){
	return (l.x < r.x) && (l.y < r.y);
}
template<class ns_component>
bool operator == (const ns_vector_2<ns_component> &l, const ns_vector_2<ns_component> &r){
	return (l.x == r.x) && (l.y == r.y);
}

typedef ns_vector_2<int> ns_vector_2i;
typedef ns_vector_2<double> ns_vector_2d;

template<class ns_component>
std::ostream & operator<<(std::ostream & o, const ns_vector_2<ns_component> & l){
	o << "(" << l.x << "," << l.y << ")";
	return o;
}
template<class derived_t, class stream_type_t, class ns_component>
ns_text_stream<derived_t,stream_type_t> & operator<<(ns_text_stream<derived_t,stream_type_t> & o, const ns_vector_2<ns_component> & l){
	o << "(" << l.x << "," << l.y << ")";
	return o;
}

template<class ns_component>
class ns_vector_3{
public:
	template<class ns_component2, class ns_component3, class ns_component4>
	ns_vector_3(ns_component2 _x, ns_component3 _y, ns_component4 _z):x((ns_component)_x),y((ns_component)_y),z((ns_component)_z){ref[0]=&x;ref[1]=&y;ref[2]=&z;}
	ns_vector_3(){ref[0]=&x;ref[1]=&y;ref[2]=&z;}
	ns_component x,y,z;

	template<class ns_component2>
	const ns_vector_3<ns_component> operator/(const ns_component2 & div) const{return ns_vector_3<ns_component>(x/div,y/div,z/div);}	

	template<class ns_component2>
	const ns_vector_3<ns_component> operator*(const ns_component2 & div) const{return ns_vector_3<ns_component>(x*div,y*div,z*div);}

	const ns_component squared() const{return x*x + y*y + z*z;}	

	template<class vector_t>
	const ns_vector_3<ns_component> operator+(const vector_t & r) const{ return ns_vector_3<ns_component>(x+r.x,y+r.y,z+r.z);}
	template<class vector_t>
	const ns_vector_3<ns_component> operator-(const vector_t & r) const{ return ns_vector_3<ns_component>(x-r.x,y-r.y,z-r.z);}

	template<class ns_component2>
	inline static ns_vector_3<ns_component> safe_subtraction(const ns_vector_3<ns_component> & a, const ns_vector_3<ns_component2> & b){
		ns_vector_3<ns_component> ret;
		if (a.x < b.x) ret.x = 0;
		else ret.x = a.x - b.x;
		if (a.y < b.y) ret.y = 0;
		else ret.y = a.y - b.y;
		if (a.z < b.z) ret.z = 0;
		else ret.z = a.z - b.z;
		return ret;
	}
	ns_component & operator[](const unsigned int i){switch(i){case 0: return x; case 1: return y; case 2: return z; default:throw ns_ex("ns_color: Accessing Invalid Color: ") << i;}}
	const ns_component & operator[](const unsigned int i) const {switch(i){case 0: return x; case 1: return y; case 2: return z; default:throw ns_ex("ns_color: Accessing Invalid Color: ") << i;}}
private:
	ns_component * ref[3];
};


template<class ns_component>
bool operator < (const ns_vector_3<ns_component> &l, const ns_vector_3<ns_component> &r){
	return (l.x < r.x) && (l.y < r.y) && (l.z < r.z);
}
template<class ns_component>
bool operator == (const ns_vector_3<ns_component> &l, const ns_vector_3<ns_component> &r){
	return (l.x == r.x) && (l.y == r.y) && (l.z == r.z);
}

template<class ns_component>
std::ostream & operator<<(std::ostream & out, const ns_vector_3<ns_component> & l){
	out << "(" << l.x << "," << l.y << "," << l.z << ")";
	return out;
}


template<class ns_component>
const inline ns_component determinant(const ns_component & a1,const ns_component & a2,const ns_component & a3, 
							   const ns_component & b1,const ns_component & b2,const ns_component & b3,
							   const ns_component & c1,const ns_component & c2,const ns_component & c3){
	return a1*b2*c3 - a1*b3*c2 - a2*b1*c3 + a2*b3*c1 + a3*b1*c2 - a3*b2*c1;
}

template<class vector_2, class ns_component>
class ns_triangle{
public:
	vector_2 vertex[3];

	ns_triangle(){}
	ns_triangle(const vector_2 & a, const vector_2 & b, const vector_2 & c){vertex[0]=a;vertex[1]=b;vertex[2]=c;}

	const inline vector_2 circumcenter() const{
		vector_2 S;
		S.x = .5*determinant<ns_component>(	vertex[0].squared(), vertex[0].y, 1,
											vertex[1].squared(), vertex[1].y, 1,
											vertex[2].squared(), vertex[2].y, 1);
		S.y = .5*determinant<ns_component>(	vertex[0].x, vertex[0].squared(),1,
											vertex[1].x, vertex[1].squared(),1,
											vertex[2].x, vertex[2].squared(),1);
		return S/determinant<ns_component>(	vertex[0].x, vertex[0].y,1,
											vertex[1].x, vertex[1].y,1,
											vertex[2].x, vertex[2].y,1);
	}
	
	const inline vector_2 center() const{
		return vector_2((vertex[0].x+vertex[1].x+vertex[2].x)/3,(vertex[0].y+vertex[1].y+vertex[2].y)/3);
	}

	//heron's formula
	double area() const{
			double
			a = sqrt((vertex[0] - vertex[1]).squared()),
			b = sqrt((vertex[1] - vertex[2]).squared()),
			c = sqrt((vertex[2] - vertex[0]).squared());
			return sqrt( (a + b + c)*(a + b - c)*(b + c - a)*(c + a - b) )/4.0;
	}	
	double maximum_height() const {
			const double a(sqrt((vertex[0] - vertex[1]).squared()));
			const double b(sqrt((vertex[1] - vertex[2]).squared()));
			const double c(sqrt((vertex[2] - vertex[0]).squared()));
			const double area(sqrt( (a + b + c)*(a + b - c)*(b + c - a)*(c + a - b) )/4.0);
			double min = a;
			if (b < min) min = b;
			if (c < min) min = c;
			//A = .5h*b    =>    h = 2A/b
			return 2*area/min;
	}

	double angle(const unsigned int i){
		unsigned int ip = ((int)i+1)%3,
					 im = ((int)i-1)%3;

		vector_2	Al = vertex[ip] - vertex[i],
					Bl = vertex[im] - vertex[i],
					Cl = vertex[ip] - vertex[im];
		
		double A2 = Al.squared(),
			   B2 = Bl.squared(),
			   C2 = Cl.squared();

		//law of cosines
		return acos(  (A2+B2-C2)/(2*sqrt(A2)*sqrt(B2)) );

	}
};
			

template<class ns_component>
class ns_edge{
public:
	ns_edge(){}
	ns_edge(const ns_component & a, const ns_component & b){vertex[0]=a;vertex[1]=b;}
	ns_component vertex[2];
};

//returns the solution for the length along two lines a and b at which they intersect.
//In the case of paraellel lines, inf is set to true and the return value is undefined.
template<class ns_component>
inline ns_vector_2d ns_intersect_u(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, bool & parallel){
	const ns_component &x1 = a.vertex[0].x,
				 &x2 = a.vertex[1].x,
				 &x3 = b.vertex[0].x,
				 &x4 = b.vertex[1].x,
				 &y1 = a.vertex[0].y,
				 &y2 = a.vertex[1].y,
				 &y3 = b.vertex[0].y,
				 &y4 = b.vertex[1].y;
	ns_vector_2d u(0,0);
	ns_component denom = ((y4 - y3)*(x2-x1) - (x4-x3)*(y2-y1));
	if (denom == 0){
		parallel= true;
		return u;
	}
	parallel = false;
	u.x = ((x4-x3)*(y1-y3) - (y4-y3)*(x1-x3))/denom;
	u.y = ((x2-x1)*(y1-y3) - (y2-y1)*(x1-x3))/denom;
	return u;
}
//intersection between two line segments
template<class ns_component>
inline bool ns_intersect_ss(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, ns_vector_2< ns_component> & intersection){
	bool parallel;
	ns_vector_2d u= ns_intersect_u(a,b,parallel);
	if (parallel || u.x < 0 || u.x > 1 || u.y < 0 || u.y > 1)
		return false;
	intersection.x = a.vertex[0].x + u.x*(a.vertex[1].x - a.vertex[0].x);
	intersection.y = a.vertex[0].y + u.x*(a.vertex[1].y - a.vertex[0].y);
	return true;
}

//intersection between a line and a line segment
template<class ns_component>
inline bool ns_intersect_ls(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, ns_vector_2< ns_component> & intersection, ns_vector_2d & u){
	bool paraellel;
	u= ns_intersect_u(a,b,paraellel);
	if (paraellel || u.y < 0 || u.y > 1)
		return false;
	intersection.x = a.vertex[0].x + u.x*(a.vertex[1].x - a.vertex[0].x);
	intersection.y = a.vertex[0].y + u.x*(a.vertex[1].y - a.vertex[0].y);
	return true;
}
template<class ns_component>
inline bool ns_intersect_ls(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, ns_vector_2< ns_component> & intersection){
	 ns_vector_2d u;
	 return ns_intersect_ls(a,b,intersection,u);
}
//intersection between two lines
template<class ns_component>
inline bool ns_intersect_ll(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, ns_vector_2< ns_component> & intersection, ns_vector_2d & u){
	bool paraellel;
	u= ns_intersect_u(a,b,paraellel);
	if (paraellel)
		return false;
	intersection.x = a.vertex[0].x + u.x*(a.vertex[1].x - a.vertex[0].x);
	intersection.y = a.vertex[0].y + u.x*(a.vertex[1].y - a.vertex[0].y);
	return true;
}
template<class ns_component>
inline bool ns_intersect_ll(const ns_edge< ns_vector_2< ns_component> > & a, const ns_edge< ns_vector_2< ns_component> > & b, ns_vector_2< ns_component> & intersection){
	 ns_vector_2d u;
	 return ns_intersect_ll(a,b,intersection,u);
}
//unit std::vector
template<class ns_component>
inline ns_vector_2d ns_unit(const ns_edge< ns_vector_2< ns_component> > & a){
	return (a.vertex[1] - a.vertex[0])/(a.vertex[1] - a.vertex[0]).mag();
}
//normal std::vector
template<class ns_component>
inline ns_vector_2<ns_component> ns_normal(const ns_vector_2< ns_component> & a){
	return ns_vector_2d(a.y,-a.x);
}
	




typedef ns_triangle<ns_vector_2i, int> ns_triangle_i;
typedef ns_triangle<ns_vector_2d, double> ns_triangle_d;

typedef ns_edge<unsigned int> ns_edge_ui;
typedef ns_edge< ns_vector_2i > ns_edge_2i;
typedef ns_edge< ns_vector_2d > ns_edge_2d;

typedef ns_vector_3<ns_8_bit> ns_color_8;
typedef ns_vector_3<ns_16_bit> ns_color_16;
typedef ns_vector_3<ns_32_bit> ns_color_32;

//std::string ns_color_to_hex_string(const ns_color_8 & c);
template<class color_t>
color_t ns_hex_string_to_color(const std::string & c){
	if (c.empty())
		return ns_color_8(0,0,0);
	if (c.size() != 6)
		throw ns_ex("ns_hex_string_to_color::Invalid color!");
	char a[3][3];    
	a[0][0] = c[0];
	a[0][1] = c[1];
	a[0][2] = 0;
	a[1][0] = c[2];
	a[1][1] = c[3];
	a[1][2] = 0;
	a[2][0] = c[4];
	a[2][1] = c[5];
	a[2][2] = 0;

	color_t col(strtol(a[0],0,16),strtol(a[1],0,16),strtol(a[2],0,16));
	return col;
}
			
//pick a color, roughly, from the spectrum based on 
//a position between 0-1
#pragma warning(disable: 4244)  //certain template arguments will generate floating point approximation errors.
								//this cant be fixed without adding extra template specifications, which would be a pain.
								//So, we just kill the warning.


template<class color_t>
color_t ns_rainbow(const float i){
	if (i < 0.333)
		return color_t(255.0*(1.0-3.0*i),	255.0*3.0*i,	0);
	else if (i < .666)
		return color_t(0,				255.0*(1.0-3.0*(i-.333)),255*(3*(i-.333)));
	else 
		return color_t(255.0*(3.0*(i-.666)),	0,		255.0*(1.0-3.0*(i-.666)));
};
#pragma warning(default: 4244)

template<class ns_component>
void ns_rainbow(const float i, ns_component * out){
	if (i < 0.5){
		out[0] = 1-i;
		out[1] = i;
		out[2] = 0;
	}
	else{
		out[0] = 0;
		out[1] = 1-i;
		out[2] = i-.5;
	}
}

template<class color_t>
inline color_t ns_rainbow(const float i, const float offset){
	if (i + offset > 1)
		 return ns_rainbow<color_t>((i + offset) - 1);
	else return ns_rainbow<color_t>((i + offset));
}


template<class T>
inline void ns_crop_vector(std::vector<T> & v, unsigned int new_first_element, unsigned int new_length){
	
	for (unsigned int i = new_first_element; i < v.size(); i++){
			v[i-new_first_element] = v[i];
		}
	v.resize(new_length);
}
#endif
