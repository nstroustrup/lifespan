/***************************************************************************
Module Name:
	Gaussian Mixture Model with Diagonal Covariance Matrix

History:
	2003/11/01	Fei Wang
	2013 luxiaoxun
***************************************************************************/

#pragma once
#include <fstream>

class GMM
{
public:
	GMM(int dimNum = 1, int mixNum = 1);
	GMM(const GMM &);
	GMM & operator=(const GMM &);
	~GMM();

	void Copy(const GMM* gmm);

	void SetMaxIterNum(int i)	{ m_maxIterNum = i; }
	void SetEndError(double f)	{ m_endError = f; }

	int GetDimNum() const			{ return m_dimNum; }
	int GetMixNum() const			{ return m_mixNum; }
	int GetMaxIterNum() const		{ return m_maxIterNum; }
	double GetEndError() const	{ return m_endError; }

	double& Prior(int i)	{ return m_priors[i]; }
	double* Mean(int i)		{ return m_means[i]; }
	double* Variance(int i)	{ return m_vars[i]; }
	const double& Prior(int i)const { return m_priors[i]; }
	const double* Mean(int i)const { return m_means[i]; }
	const double* Variance(int i)const { return m_vars[i]; }

	void setPrior(int i,double val)	{  m_priors[i]=val; }
	void setMean(int i,double *val)		{ for(int j=0;j<m_dimNum;j++) m_means[i][j]=val[j]; }
	void setVariance(int i,double *val)	{ for(int j=0;j<m_dimNum;j++) m_vars[i][j]=val[j]; }

	double GetProbability(const double* sample) const;
	double Get_1D_Probability(int dim,const double* sample) const;

	/*	SampleFile: <size><dim><data>...*/
    void Init(const char* sampleFileName);
	void Train(const char* sampleFileName);
	void Init(double *data, int N);
	void Train(double *data, int N);

	void DumpSampleFile(const char* fileName);

	friend std::ostream& operator<<(std::ostream& out, GMM& gmm);
	friend std::istream& operator>>(std::istream& in, GMM& gmm);

private:
	int m_dimNum;		// 
	int m_mixNum;		// Gaussian
	double* m_priors;	// Gaussian
	double** m_means;	// Gaussian
	double** m_vars;	// Gaussian

	// A minimum variance is required. Now, it is the overall variance * 0.01.
	double* m_minVars;
	int m_maxIterNum;		// The stopping criterion regarding the number of iterations
	double m_endError;		// The stopping criterion regarding the error

private:
	// Return the "j"th pdf, p(x|j).
	double GetProbability(const double* x, int j) const;
	void Allocate();
	void Dispose();
	void set_dimandmixnums(int dimNum, int mixNum);
};
