/*
NAME:
gmm.cpp

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

#include <iostream>
#include <iomanip>
#include "gsl/gsl_math.h"
#include "gsl/gsl_sf_log.h"
#include <cmath>
#include <limits>
#include <ctime>
#include "gmm.h"

using namespace std;


double GMM::getMixCoefficient(int i)
{
	if (i >= 0 && i <= numGaussians && a != NULL)
	{
		return a[i];
	}
	else
	{
		cerr << "ERROR: out of bounds.\n";
		throw 1;
	}
}

double GMM::getMean(int i)
{
	if (i >= 0 && i <= numGaussians && mean != NULL)
	{
		return mean[i];
	}
	else
	{
		cerr << "ERROR: out of bounds.\n";
		throw 1;
	}
}

double GMM::getVar(int i)
{
	if (i >= 0 && i <= numGaussians && var != NULL)
	{
		return var[i];
	}
	else
	{
		cerr << "ERROR: out of bounds.\n";
		throw 1;
	}
}

double GMM::getBIC()
{
	return BIC;
}

double GMM::getLogLikelihood()
{
	return loglikelihood;
}

/*
Calculates the log likelihood and BIC of the data given the model and current parameters

INPUT:

none, but assumes the model parameters have been initialized and the data pointer is vaild

OUTPUT:

none

FUNCTION:

Parameters BIC, and loglikelihood are modified.

Log Likelihood function:

l = \sum_{j=1}^N ln( \sum_{k=1}^K a_k p_k(x_j|\theta_k) )

BIC:

-2l + (3K-1)ln(N)

Where:

k : indexes Gaussian components
K : total number of Gaussians
j : indexes data
N : total number of data points
x_j : jth element of the data array x
a_k : mixture coefficient for Gaussian k
\theta_k : parameter vector (\mu_k,\Sigma_k) for Gaussian k
\mu_k : mean of Gaussian k
\Sigma_k : variance of Gaussian k

*/
GMM::GMM(int n, double* a_init, double* mean_init, double* var_init, int maxIt = 250, double p = 1e-5, bool v, bool only)
{
	if (mean_init == NULL || var_init == NULL || a_init == NULL)
	{
		cerr << "ERROR: NULL pointer passed as initial value.\n";
		throw 1;
	}

	if (n < 1)
	{
		cerr << "ERROR: Can not have fewer than 1 Gaussian.\n";
		throw 1;
	}

	numGaussians = n;

	a = new double[numGaussians];
	mean = new double[numGaussians];
	var = new double[numGaussians];

	resp = new double[numGaussians];
	sum_wj = new double[numGaussians];
	sum_wj_xj = new double[numGaussians];
	sum_wj_xj2 = new double[numGaussians];

	for (int i = 0; i < numGaussians; i++)
	{
		a[i] = a_init[i];
		mean[i] = mean_init[i];
		var[i] = var_init[i];
	}

	x = NULL;
	currIteration = 0;
	digits = 1;
	place = 10;
	maxIterations = maxIt;
	precision = p;
	verbose = v;
	iterOnly = only;

	loglikelihood = -numeric_limits<double>::max();
	BIC = numeric_limits<double>::max();

	return;
}

GMM::~GMM()
{
	x = NULL;
	delete[] a;
	delete[] mean;
	delete[] var;
	delete[] resp;
	delete[] sum_wj;
	delete[] sum_wj_xj;
	delete[] sum_wj_xj2;
	return;
}

double GMM::normalLog(double x, double mean, double var)
{
	const static double C = (-0.5 * gsl_sf_log(2 * M_PI));
	return C - (0.5 * gsl_sf_log(var)) - gsl_pow_2(x - mean) / (2.0 * var);
}


/*
Calculates one step of the EM algorithm

INPUT:

none, but assumes the model parameters have been initialized and the data pointer is vaild

OUTPUT:

none

FUNCTION:

Performs both the expectation and maximization steps simultaneously, then calculates the
log likelihood and BIC for the current model and parameters.  The calculation has been
broken down in such a way as to minimize the number of loops needed (or at least get close
to the minimum).  I could be very wrong.

Parameters a, mean, var, a_t, mean_t, var_t, BIC, and loglikelohood are modified.

E-step:

w^t_{jk} = a^t_k p_k(x_j|\theta^t_k) / ( \sum_{i=1}^K a^t_i p_i(x_j|\theta^t_i) )

M-step:

a^{t+1}_k = 1/N \sum_{j=1}^N w^t_{jk}

\mu^{t+1}_k = ( \sum_{j=1}^N w^t_{jk} x_j ) / ( \sum_{j=1}^N w^t_{jk} )

\Sigma^{t+1}_k = ( \sum_{j=1}^N w^t_{jk} (x_j - \mu^{t+1}_k)^2 ) / ( \sum_{j=1}^N w^t_{jk} )

Where:

t : current iteration
k : indexes Gaussian components
K : total number of Gaussians
j : indexes data
N : total number of data points
x_j : jth element of the data array x
w_{jk} : prob of membership of data point j in gaussian k
a_k : mixture coefficient for Gaussian k
p_k(x_j|\theta_k) : PDF of kth Gaussian with parameters \theta_k
\theta_k : parameter vector (\mu_k,\Sigma_k)
\mu_k : mean of Gaussian k
\Sigma_k : variance of Gaussian k


*/
void GMM::update()
{
	double den;
	double l_max, tmp, sum;

	for (int k = 0; k < numGaussians; k++)
	{
		sum_wj[k] = 0;
		sum_wj_xj[k] = 0;
		sum_wj_xj2[k] = 0;
	}

	double L = 0;

	for (int j = 0; j < dataSize; j++)
	{
		l_max = -numeric_limits<double>::max();
		for (int i = 0; i < numGaussians; i++)
		{
			resp[i] = gsl_sf_log(a[i]) + normalLog(x[j], mean[i], var[i]); //calculating log(p_i(x_j|theta_i)
			if (resp[i] > l_max) l_max = resp[i];
		}

		//logsum to avoid at least 1 underflow
		sum = 0;
		for (int i = 0; i < numGaussians; i++) sum += exp(resp[i] - l_max);
		tmp = l_max + gsl_sf_log(sum);

		L += tmp;//loglikelihood

		den = 0;
		for (int i = 0; i < numGaussians; i++)
		{
			resp[i] = exp(resp[i] - tmp);
			den += resp[i];
		}

		for (int k = 0; k < numGaussians; k++)
		{
			sum_wj[k] += resp[k] / den;
			sum_wj_xj[k] += x[j] * resp[k] / den;
			sum_wj_xj2[k] += x[j] * x[j] * resp[k] / den;
		}
	}
	//Assign next iteration parameters to current parameters
	for (int k = 0; k < numGaussians; k++)
	{
		a[k] = sum_wj[k] / double(dataSize);
		mean[k] = sum_wj_xj[k] / sum_wj[k];
		var[k] = sum_wj_xj2[k] / sum_wj[k] - mean[k] * mean[k];
	}

	loglikelihood = L;
	BIC = -2.0 * loglikelihood + double(3.0 * numGaussians - 1) * gsl_sf_log(dataSize);
	return;
}


/*
Prints the current values of all parameters, log likelihood, and BIC
*/
void GMM::printState()
{
	cerr << setprecision(5) << scientific;

	cerr << "(";
	for (int k = 0; k < numGaussians - 1; k++) cerr << a[k] << ",";
	cerr << a[numGaussians - 1] << ")\t";

	cerr << "(";
	for (int k = 0; k < numGaussians - 1; k++) cerr << mean[k] << ",";
	cerr << mean[numGaussians - 1] << ")\t";

	cerr << "(";
	for (int k = 0; k < numGaussians - 1; k++) cerr << var[k] << ",";
	cerr << var[numGaussians - 1] << ")\t";

	cerr << loglikelihood << "\t" << BIC << endl;
}

void GMM::printIteration() {
	for (int i = 0; i < digits; i++) cerr << "\b";
	cerr << currIteration;
	if (currIteration / place > 0) {
		place *= 10;
		digits++;
	}

}

/*
Starts the GMM EM estimation proceedure

INPUT:

double *data : an array of data point observations to use for GMM
int size : the length of the array

OUTPUT:

bool : true if the EM algorithm converged to the specified precision
false otherwise

FUNCTION:

Executes EM steps until convergence or until reached maxIterations.
Parameters a, mean, var, a_t, mean_t, var_t, BIC, and loglikelohood are modified.

*/
bool GMM::estimate(double *data, int size)
{
	bool converged = false;

	if (data == NULL || size < 1)
	{
		cerr << "Invalid dataset.\n";
		throw 1;
	}

	if (verbose) cerr << "Begin GMM estimation with k = " << numGaussians << " Gaussians...\n";

	dataSize = size;
	x = data;

	double lastloglikelihood = loglikelihood;

	if (verbose)
	{
		if (iterOnly) {
			cerr << "iteration: 0";
			printIteration();
		}
		else {
			cerr << "iteration\tmixture\tmean\tvar\tlogL\tBIC\n";
			cerr << "0\t";
			printState();
		}
	}
	int i;
	for (i = 1; i <= maxIterations; i++)
	{
		currIteration = i;
		//EM steps are done here, includes recalculation of the likelihood and BIC
		update();

		if (verbose)
		{
			if (iterOnly) printIteration();
			else {
				cerr << currIteration << "\t";
				printState();
			}
		}

		if (abs(loglikelihood - lastloglikelihood) <= precision)
		{
			converged = true;
			break;
		}

		lastloglikelihood = loglikelihood;
	}

	if ((iterOnly && verbose) && (converged || i == maxIterations)) cerr << endl;

	return converged;
}
