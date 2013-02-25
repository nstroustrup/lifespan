#ifndef NS_LINEAR_REGRESSION_MODEL_H
#define NS_LINEAR_REGRESSION_MODEL_H



struct ns_linear_regression_model_parameters{
	double slope,
		   y_intercept;
	double mean_squared_error;
};
class ns_linear_regression_model{
public:
	template<class T>
	static ns_linear_regression_model_parameters fit(const std::vector<T> & y, const std::vector<T> & t){
		return fit(y,t,0,y.size());
	}
	
	template<class T>
	static ns_linear_regression_model_parameters fit(const std::vector<T> & y, const std::vector<T> & t, const unsigned long start_i, const unsigned long size){
		if (size < 2)
			throw ns_ex("ns_fit_two_part_linear_model()::Attempting to run a linear regression on ") << size << " points";
		double min_y(y[start_i]),min_t(t[start_i]),max_y(y[start_i]),max_t(t[start_i]);

		for (unsigned int i = start_i+1; i < start_i+size; i++){
			if (min_y > y[i])
				min_y = y[i];
			if (min_t > t[i])
				min_t = t[i];
			if (max_y < y[i])
				max_y = y[i];
			if (max_t < t[i])
				max_t = t[i];
		}
		double y_offset = min_y/2.0+max_y/2.0,
			   t_offset = min_t/2.0+max_t/2.0;
		double mean_y(0),mean_t(0);
		for (unsigned int i = start_i+1; i < start_i+size; i++){
			mean_y+=y[i]-y_offset;
			mean_t+=t[i]-t_offset;
		}
		mean_y = mean_y/(double)size+y_offset;
		mean_t = mean_t/(double)size+t_offset;
	
		ns_linear_regression_model_parameters p;
		p.slope = 0;
		double denom(0);
		for (unsigned int i = start_i+1; i < start_i+size; i++){
			p.slope+=(t[i]-mean_t)*(y[i]-mean_y);
			denom += (t[i]-mean_t)*(t[i]-mean_t);
		}
		if (denom == 0){
			if (p.slope != 0)
				p.slope = std::numeric_limits<double>::infinity();
		}
		else p.slope/=denom;
		p.y_intercept = mean_y-p.slope*mean_t;

		p.mean_squared_error = 0;
		for (unsigned int i = start_i+1; i < start_i+size; i++){
			p.mean_squared_error += (y[i]-(p.y_intercept+p.slope*t[i]))*
									(y[i]-(p.y_intercept+p.slope*t[i]));
		}
		p.mean_squared_error/=size;
	//	p.y_intercept+=y_offset;

		return p;
	}
};


#endif
