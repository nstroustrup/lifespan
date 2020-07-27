#pragma once
#include "GMM.h"

struct ns_gmm_sorter {
	ns_gmm_sorter() {}
	ns_gmm_sorter(double w, double m, double v) :weight(w), mean(m), var(v) {}
	double weight, mean, var;
};
bool operator<(const ns_gmm_sorter& a, const ns_gmm_sorter& b);
inline bool ns_double_equal(const double& a, const double& b);
bool operator==(const GMM& a, const GMM& b);


template<class accessor_t, int number_of_gaussians = 3 >
class ns_emission_probabiliy_gausian_1D_model {
public:
	ns_emission_probabiliy_gausian_1D_model() : gmm(1, number_of_gaussians), specified(false) {}
	bool specified;
	template<class data_accessor_t, class data_t>
	void build_from_data(const std::vector<data_t>& observations) {
		specified = true;
		data_accessor_t data_accessor;

		double* data = new double[observations.size()];
		for (unsigned long i = 0; i < observations.size(); i++) {
			const auto v = data_accessor(observations[i].measurement);
			data[i] = v;
		}

		if (observations.size() < number_of_gaussians) {
			for (unsigned int i = 0; i < number_of_gaussians; i++) {
				gmm_weights[i] = (i == 0) ? 1 : 0;
				gmm_means[i] = 0;
				gmm_var[i] = 1;
			}


			for (unsigned int i = 0; i < observations.size(); i++)
				gmm_means[0] += data[i];
			gmm_means[0] /= observations.size();

			for (unsigned int i = 0; i < observations.size(); i++) {
				gmm.setPrior(i, gmm_weights[i]);
				gmm.setMean(i, &gmm_means[i]);
				gmm.setVariance(i, &gmm_var[i]);
			}
			return;
		}



		gmm.SetMaxIterNum(1e6);
		gmm.SetEndError(1e-5);
		gmm.Train(data, observations.size());
		double sum_of_weights = 0;

		//we sort in order of weights, so it's easy to visualize the output of the model
		std::vector< ns_gmm_sorter> sorted(number_of_gaussians);
		for (unsigned int i = 0; i < number_of_gaussians; i++)
			sorted[i] = ns_gmm_sorter(gmm.Prior(i), *gmm.Mean(i), *gmm.Variance(i));
		std::sort(sorted.begin(), sorted.end());
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			gmm_weights[i] = sorted[i].weight;
			gmm_means[i] = sorted[i].mean;
			gmm_var[i] = sorted[i].var;
			sum_of_weights += gmm_weights[i];

			gmm.setPrior(i, gmm_weights[i]);
			gmm.setMean(i, &gmm_means[i]);
			gmm.setVariance(i, &gmm_var[i]);
		}

		if (abs(sum_of_weights - 1) > 0.01)
			throw ns_ex("GMM problem");
	}
	//Note that we do not calculate the /probability/ of observing the value.
	//we calculate the value of the /probability density function/ at a certain t
	//which is bounded between 0 and infinity!
	double point_emission_pdf(const ns_analyzed_image_time_path_element_measurements& e) const {
		if (!specified)
			throw ns_ex("Accessing unspecified accessor!");
		accessor_t accessor;
		const double val = accessor(e);
		const double b = gmm.GetProbability(&val);
		return b;
	}
	static void write_header(std::ostream& o) {
		o << "Specified";
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			o << ",Weight " << i << ", Mean " << i << ", Var " << i;
		}

	}
	void write(std::ostream& o) const {
		o.precision(30);
		o << (specified ? "1" : "0");

		for (unsigned int i = 0; i < number_of_gaussians; i++)
			o << "," << log(gmm_weights[i]) << "," << gmm_means[i] << "," << gmm_var[i];
	}
	void read(std::istream& in) {
		ns_get_string get_string;
		std::string tmp;
		get_string(in, tmp);
		specified = (tmp == "1");
		ns_get_double get_double;
		if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			get_double(in, gmm_weights[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, gmm_means[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, gmm_var[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		}
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			if (!std::isfinite(gmm_weights[i]))
				gmm_weights[i] = 0;
			else gmm_weights[i] = exp(gmm_weights[i]);

			gmm.setPrior(i, gmm_weights[i]);
			gmm.setMean(i, &gmm_means[i]);
			gmm.setVariance(i, &gmm_var[i]);

		}
	}

	bool equal(const ns_emission_probabiliy_gausian_1D_model<accessor_t>& t) const {
		if (specified != t.specified) {
			std::cerr << "specification mismatch\n";
			return false;
		}
		if (!specified)
			return true;
		return this->gmm == t.gmm;
	}
private:

	GMM gmm;
	double gmm_weights[3],
		gmm_means[3],
		gmm_var[3];
};




template<class measurement_accessor_t>
struct ns_covarying_gaussian_dimension {
	measurement_accessor_t* measurement_accessor;
	std::string name;
	ns_covarying_gaussian_dimension<measurement_accessor_t>(measurement_accessor_t* m, const std::string& n) :measurement_accessor(m), name(n) {}
	ns_covarying_gaussian_dimension<measurement_accessor_t>(const ns_covarying_gaussian_dimension& c) {
		name = c.name;
		measurement_accessor = c.measurement_accessor->clone();
	}
	ns_covarying_gaussian_dimension<measurement_accessor_t>(ns_covarying_gaussian_dimension&& c) { measurement_accessor = c.measurement_accessor; c.measurement_accessor = 0; name = c.name; }
	~ns_covarying_gaussian_dimension<measurement_accessor_t>() { ns_safe_delete(measurement_accessor); }
};
template<class ns_measurement_accessor>
bool operator==(const ns_covarying_gaussian_dimension< ns_measurement_accessor >& a, const ns_covarying_gaussian_dimension< ns_measurement_accessor >& b);

template<class measurement_accessor_t, int number_of_dimensions = 2, int number_of_gaussians = 4>
class ns_emission_probabiliy_gaussian_diagonal_covariance_model {
public:
	ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t, number_of_dimensions, number_of_gaussians>() : gmm(number_of_dimensions, number_of_gaussians) {}

	GMM gmm;
	std::vector< ns_covarying_gaussian_dimension<measurement_accessor_t> > dimensions;
	mutable double observation_buffer[number_of_dimensions];
	static double* training_data_buffer;
	static unsigned long training_data_buffer_size;
	static ns_lock training_data_buffer_lock;
	void guestimate_small_set(const std::vector<const std::vector<typename measurement_accessor_t::data_t>* >& observations) {
		
		//calculate means
		double means[number_of_dimensions];
		for (unsigned int d = 0; d < number_of_dimensions; d++)
			means[d] = 0;
		bool valid_point;
		unsigned long N = 0;
		for (unsigned int i = 0; i < observations.size(); i++)
			for (unsigned int j = 0; j < observations[i]->size(); j++)
				for (unsigned int d = 0; d < number_of_dimensions; d++) {
					double val = (*dimensions[d].measurement_accessor)((*observations[i])[j],valid_point);
					if (valid_point) {
						means[d] += val;
						N++;
					}
				}
		if (N != 0)
			for (unsigned int d = 0; d < number_of_dimensions; d++)
				means[d] /= N;

		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			gmm.setPrior(i, i == 0);	//only use 1 gaussian
			gmm.setMean(i, means);
			gmm.setVariance(i, means);
		}
	}
	void build_from_data(const std::vector<const std::vector<typename measurement_accessor_t::data_t>* >& observations) {
		unsigned long N(0);
		for (unsigned int i = 0; i < observations.size(); i++)
			N += observations[i]->size();

		if (N < number_of_gaussians) {
			guestimate_small_set(observations);
			return;
		}
		ns_acquire_lock_for_scope lock(training_data_buffer_lock, __FILE__, __LINE__);
		if (training_data_buffer_size < number_of_dimensions * N) {
			delete[] training_data_buffer;
			training_data_buffer = 0;
			training_data_buffer_size = 0;
		}
		if (training_data_buffer == 0) {
			training_data_buffer_size = number_of_dimensions * N;
			training_data_buffer = new double[training_data_buffer_size];
		}
		unsigned ij = 0;
		bool valid_data_point;
		N = 0;
		for (unsigned long i = 0; i < observations.size(); i++) {
			for (unsigned int j = 0; j < observations[i]->size(); j++) {
				for (unsigned int d = 0; d < number_of_dimensions; d++) {
					const double val = (*dimensions[d].measurement_accessor)((*observations[i])[j], valid_data_point);
					if (!std::isfinite(val))
						throw ns_ex("Invalid training data: ") << val << ": observations[" << i << "][" << j << "]\n";
					if (valid_data_point) {
						training_data_buffer[number_of_dimensions * (ij)+d] = val;
						N++;
					}

				}
				ij++;
			}
		}
		if (N < number_of_gaussians) {
			guestimate_small_set(observations);
			return;
		}
		//gmm.SetMaxIterNum(1e6);
		//gmm.SetEndError(1e-5);
		gmm.Train(training_data_buffer, N);
		lock.release();
		double sum_of_weights = 0;
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			sum_of_weights += gmm.Prior(i);

			if (isnan(gmm.Prior(i)))
				throw ns_ex("GMM problem");
		}

		if (abs(sum_of_weights - 1) > 0.01)
			throw ns_ex("GMM problem");
	}
	//the pdf values are proportional to the probability of observing a range of values within a small dt of an observation.
	//so as long as we are always comparing observations at the same t, we can multiply these together.
	//So we can use them for viterbi algorithm 
	template<class l_data_t>
	double point_emission_log_probability(const l_data_t & e) const {
		bool valid_point;
		for (unsigned int d = 0; d < number_of_dimensions; d++)
			observation_buffer[d] = (*dimensions[d].measurement_accessor)(e, valid_point);
		return log(gmm.GetProbability(observation_buffer));
	}

	//these are the actual likelihoods of an observation being drawn from the GMM.
	//They are more expensive to calculate so we only use them when necessary
	template<class l_data_t>
	double point_emission_likelihood(const l_data_t& e) const {
		bool valid_data;
		for (unsigned int d = 0; d < number_of_dimensions; d++)
			observation_buffer[d] = (*dimensions[d].measurement_accessor)(e,valid_data);
		return gmm.GetLikelihood(observation_buffer);
	}

	template<class l_data_t>
	void log_sub_probabilities(const l_data_t & m, std::vector<double>& measurements, std::vector<double>& probabilities) const {
		measurements.resize(number_of_dimensions);
		probabilities.resize(number_of_dimensions);
		bool valid_point;
		for (unsigned int d = 0; d < number_of_dimensions; d++) 
			observation_buffer[d] = measurements[d] = (*dimensions[d].measurement_accessor)(m,valid_point);


		for (unsigned int d = 0; d < number_of_dimensions; d++)
			probabilities[d] = log(gmm.Get_1D_Probability(d, observation_buffer));


	}
	void sub_probability_names(std::vector<std::string>& names) const {
		names.resize(number_of_dimensions);
		for (unsigned int d = 0; d < number_of_dimensions; d++)
			names[d] = dimensions[d].name;
	}
	unsigned long number_of_sub_probabilities() const {
		return number_of_dimensions;
	}
	static void write_header(std::ostream& o) {
		o << "Version,Permissions,Movement State,Number of Dimensions ,Number of Gaussians, Dimension Name";
		for (unsigned int i = 0; i < number_of_gaussians; i++) {
			o << ",Weight " << i << ", Mean " << i << ", Var " << i;
		}
	}
	void write(const ns_hmm_movement_state state, const std::string& version, int extra_data, std::ostream& o) const {
		o.precision(30);
		for (unsigned int d = 0; d < number_of_dimensions; d++) {
			o << version << "," << extra_data << "," << ns_hmm_movement_state_to_string(state) << "," << number_of_dimensions << "," << number_of_gaussians << "," << dimensions[d].name;

			for (unsigned int g = 0; g < number_of_gaussians; g++)
				o << "," << log(gmm.Prior(g)) << "," << gmm.Mean(g)[d] << "," << gmm.Variance(g)[d];
			if (d + 1 != number_of_dimensions)
				o << "\n";
		}
	}
	void read_dimension(const unsigned int dim, std::vector<double>& weights, std::vector<double>& means, std::vector<double>& vars, std::istream& in) {
		//we place the mean in dimension dim for gaussian g at
		//number_of_dimensions*g + dim

		ns_get_double get_double;
		double tmp;
		for (unsigned int g = 0; g < number_of_gaussians; g++) {
			get_double(in, weights[g]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, means[number_of_dimensions * g + dim]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, vars[number_of_dimensions * g + dim]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		}
	}
	bool read(std::istream& i, ns_hmm_movement_state& state, std::string& software_version, int& extra_data) {

		ns_get_string get_string;
		ns_get_int get_int;
		std::string tmp;
		std::string dimension_name;
		int file_number_of_dimensions, file_number_of_gaussians;
		int r = 0;
		std::map<std::string, int> dimension_name_mapping;
		for (unsigned int i = 0; i < number_of_dimensions; i++) {
			dimension_name_mapping[dimensions[i].name] = i;
		}
		std::vector<double> weights(number_of_gaussians);
		std::vector<double> means(number_of_gaussians * number_of_dimensions);  // the means for gaussian i start at i*number_of_gaussians
		std::vector<double> vars(number_of_gaussians * number_of_dimensions);

		software_version = "";
		extra_data = 0;
		state = ns_hmm_unknown_state;
		if (i.fail())
			return false;
		while (!i.fail()) {
			get_string(i, software_version);
			if (i.fail()) {
				if (r == 0) {
					return false;
				}
				else
					throw ns_ex("ns_emission_probabiliy_model::read()::Bad model file");
			}
			if (software_version == "")
				std::cerr << "WA";
			get_string(i, tmp);
			extra_data = atoi(tmp.c_str());
			get_string(i, tmp);
			ns_hmm_movement_state state_temp = ns_hmm_movement_state_from_string(tmp);	//all information for each state should be written to files in contiguous lines
			if (r != 0 && state_temp != state)
				throw ns_ex("ns_emission_probabiliy_model::read()::Mixed up order of emission probability model!");
			state = state_temp;

			get_int(i, file_number_of_dimensions);
			get_int(i, file_number_of_gaussians);
			if (file_number_of_gaussians != number_of_gaussians)
				throw ns_ex("Model gaussian number mismatch");
			if (file_number_of_dimensions != number_of_dimensions)
				throw ns_ex("Model dimension number mismatch");

			get_string(i, dimension_name);
			if (i.fail())
				throw ns_ex("ns_emission_probabiliy_model::read()::Bad model file");

			auto p = dimension_name_mapping.find(dimension_name);
			if (p == dimension_name_mapping.end())
				throw ns_ex("Unknown measurement type: ") << dimension_name;
			if (p->second >= number_of_dimensions)
				throw ns_ex("Invalid dimension size");
			read_dimension(p->second, weights, means, vars, i);
			r++;
			if (r == number_of_dimensions)
				break;
		}

		for (unsigned int g = 0; g < number_of_gaussians; g++) {
			if (!std::isfinite(weights[g]))
				gmm.setPrior(g, 0);
			else
				gmm.setPrior(g, exp(weights[g]));
			gmm.setMean(g, &means[number_of_dimensions * g]);
			gmm.setVariance(g, &vars[number_of_dimensions * g]);
		}
		return true;
	}
};


template<class accessor_t,int number_of_dimensions = 2, int number_of_gaussians = 4>
bool operator==(const ns_emission_probabiliy_gaussian_diagonal_covariance_model<accessor_t, number_of_dimensions, number_of_gaussians>& a, const ns_emission_probabiliy_gaussian_diagonal_covariance_model<accessor_t, number_of_dimensions, number_of_gaussians>& b) {
	if (!(a.gmm == b.gmm)) {
		std::cerr << "GMMS aren't equal\n";
		return false;
	}
	if (a.dimensions.size() != b.dimensions.size()) {
		std::cerr << "Dimension numbers don't match\n";
		return false;
	}
	for (unsigned int i = 0; i < a.dimensions.size(); i++)
		if (!(a.dimensions[i] == b.dimensions[i])) {
			std::cerr << "Dimension " << i << " isn't equal\n";
			return false;
		}
	return true;
}
