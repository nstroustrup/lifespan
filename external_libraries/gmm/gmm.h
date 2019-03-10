#pragma once
/*
NAME:
gmm.h

PURPOSE:
A class that implements a 1-dimensional Gaussian Mixture Model fit with the EM algorithm

PLATFORM:
Tested in 2013 on a MacPro running OSX 10.8.2, but it should be platform independent as long as GSL is available.

DEPENDENCIES:
Requires GNU GSL, which can be found at <http://www.gnu.org/software/gsl/>.
When compiling, use the flags
-lgsl -lgslcblas
or
$(LIB_PATH)/libgsl.a $(LIB_PATH)/libgslcblas.a

USAGE:
The class object contains all the machinery to do a GMM estimation with the EM algorithm.
Upon instantiation, GMM will require the following:

n : number of Gaussians to use

a : array of initial guesses for the mixture coefficients

mean :  array of intial guesses for the means

var :  array of initial guesses for the variances

Optional parameters:

maxIter : maximum number of iterations of the EM algorithm, default 250

p : desired precision stopping condition, default 1e-5

v : if true, will output progress of each step of EM algorithm, default true

To run the EM algorithm, call GMM::estimate(double *data, int dataSize)

Example:

GMM gmm(n,a,mean,var);
gmm.estimate(data,dataSize);


Copyright (C) 2013  Zachary A Szpiech (szpiech@gmail.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


class GMM
{
private:

	int numGaussians; //How many gaussians do we assume?
	double *a; //Mixture proportions
	double *mean;
	double *var;

	//for holding intermediate results
	double *resp;
	double *sum_wj;
	double *sum_wj_xj;
	double *sum_wj_xj2;

	int dataSize;
	double *x; //a pointer to the data of length dataSize

	int currIteration;
	int digits;
	int place;
	int maxIterations; //EM will stop after this many iterations if it hasn't converged
	double precision; //Convergence condition

	bool verbose; //if true, prints information to stderr
	bool iterOnly; //If this and verbose are true, prints only the current iteration to stderr

	double loglikelihood; //of the data given the given model and current parameters
	double BIC; //Bayseian Information Criteria for the data given the given model and current parameters

	void update(); //Update parameters, this folds the E step, M step, loglikelihood, and BIC calculation into a single calculation
	void printState();
	void printIteration();

	double normalLog(double x, double mean, double var);

public:

	GMM(int n, double* a_init, double* mean_init, double* var_init, int maxIt, double p, bool v = true, bool iterOnly = false);
	~GMM();

	bool estimate(double* data, int size);

	double getBIC();
	double getLogLikelihood();
	double getMixCoefficient(int i);
	double getMean(int i);
	double getVar(int i);

};
