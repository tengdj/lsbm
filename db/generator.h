/**                                                                                                                                                                                
 * Copyright (c) 2010 Yahoo! Inc. All rights reserved.

 *                                                                                                                                                                                 
 * Licensed under the Apache License, Version 2.0 (the "License"); you                                                                                                             
 * May not use this file except in compliance with the License. You                                                                                                                
 * may obtain a copy of the License at                                                                                                                                             
 *                                                                                                                                                                                 
 * http://www.apache.org/licenses/LICENSE-2.0                                                                                                                                      
 *                                                                                                                                                                                 
 * Unless required by applicable law or agreed to in writing, software                                                                                                             
 * distributed under the License is distributed on an "AS IS" BASIS,                                                                                                               
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or                                                                                                                 
 * implied. See the License for the specific language governing                                                                                                                    
 * permissions and limitations under the License. See accompanying                                                                                                                 
 * LICENSE file.                                                                                                                                                                   
 */

#ifndef H_GENERATOR
#define H_GENERATOR

#include <cmath>
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <iostream>

/**
 * An expression that generates a sequence of string values, following some distribution (Uniform, Zipfian, Sequential, etc.)
 */
namespace generator{

static inline long long YCSBKey_hash(long long val)
{
  	long long FNV_offset_basis_64=0xCBF29CE484222325LL;
  	long long FNV_prime_64=1099511628211LL;
  	long long hashval = FNV_offset_basis_64;
  	for (int i=0; i<8; i++)
  	{
  		long long octet=val&0x00ff;
  		val=val>>8;
  		hashval = hashval ^ octet;
  		hashval = hashval * FNV_prime_64;
  	}
  	return llabs(hashval);
}

class YCSBKeyGenerator{
private:
	long long *keypool;
	int index;
	int max;

public:
	YCSBKeyGenerator(int startnum, int filenum, int keysize):index(0) {
		keypool = new long long[keysize*filenum];
		for(long long i=0;i<keysize*filenum;i++)
		{
			keypool[i] = YCSBKey_hash(i + startnum);
		}
		sort(keypool,0,keysize*filenum);
	}
	~YCSBKeyGenerator() {
		delete keypool;
	}
	void sort(long long *num, int top, int bottom);
	int partition(long long *array, int top, int bottom);
	long long nextKey();
};

class Utils
{
public:
      /**
       * Hash an integer value.
       */
      static long hash(long val)
      {
	    return FNVhash64(val);
      }

      static const int FNV_offset_basis_32=0x811c9dc5;
      static const int FNV_prime_32=16777619;

      /**
       * 32 bit FNV hash. Produces more "random" hashes than (say) std::string.hashCode().
       *
       * @param val The value to hash.
       * @return The hash value
       */
      static int FNVhash32(int val)
      {
	   //from http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash
	   int hashval = FNV_offset_basis_32;

	   for (int i=0; i<4; i++)
	   {
	      int octet=val&0x00ff;
	      val=val>>8;

	      hashval = hashval ^ octet;
	      hashval = hashval * FNV_prime_32;
	      //hashval = hashval ^ octet;
	   }
	    return labs(hashval);
      }

      static const long FNV_offset_basis_64=0xCBF29CE484222325L;
      static const long FNV_prime_64=1099511628211L;

      /**
       * 64 bit FNV hash. Produces more "random" hashes than (say) std::string.hashCode().
       *
       * @param val The value to hash.
       * @return The hash value
       */
      static long FNVhash64(long val)
      {
	 //from http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash
	 long hashval = FNV_offset_basis_64;

	 for (int i=0; i<8; i++)
	 {
	    long octet=val&0x00ff;
	    val=val>>8;

	    hashval = hashval ^ octet;
	    hashval = hashval * FNV_prime_64;
	    //hashval = hashval ^ octet;
	 }
	 return labs(hashval);
      }
};

class Generator
{
	/**
	 * Generate the next string in the distribution.
	 */
	public:
	virtual std::string nextString()=0;

	/**
	 * Return the previous string generated by the distribution; e.g., returned from the last nextString() call. 
	 * Calling lastString() should not advance the distribution or have any side effects. If nextString() has not yet 
	 * been called, lastString() should return something reasonable.
	 */
	virtual std::string lastString()=0;
	virtual ~Generator(){};
};

class IntegerGenerator:public Generator
{
	int lastint;

	/**
	 * Set the last value generated. IntegerGenerator subclasses must use this call
	 * to properly set the last string value, or the lastString() and lastInt() calls won't work.
	 */
public:
	void setLastInt(int last)
	{
		lastint=last;
	}

	/**
	 * Return the next value as an int. When overriding this method, be sure to call setLastString() properly, or the lastString() call won't work.
	 */
	virtual int nextInt()=0;

	/**
	 * Generate the next string in the distribution.
	 */
	std::string nextString()
	{
		char buf[100];
		sprintf(buf, "%d", nextInt());
		std::string s = buf;
		return s;
	}
	/**
		 * Return the previous int generated by the distribution. This call is unique to IntegerGenerator subclasses, and assumes
		 * IntegerGenerator subclasses always return ints for nextInt() (e.g. not arbitrary strings).
		 */
	int lastInt()
	{
		return lastint;
	}
	/**
	 * Return the previous string generated by the distribution; e.g., returned from the last nextString() call.
	 * Calling lastString() should not advance the distribution or have any side effects. If nextString() has not yet
	 * been called, lastString() should return something reasonable.
	 */
	std::string lastString()
	{
		char buf[100];
		sprintf(buf, "%d", lastint);
		std::string s = buf;
		return s;
	}


	/**
	 * Return the expected value (mean) of the values this generator will return.
	 */
	virtual double mean()=0;
	virtual ~IntegerGenerator(){};
};

class CounterGenerator:public IntegerGenerator
{
	int counter;
	/**
	 * Create a counter that starts at countstart
	 */
public:
	CounterGenerator(int countstart):counter(countstart)
	{
		IntegerGenerator::setLastInt(counter-1);
	}

	/**
	 * If the generator returns numeric (integer) values, return the next value as an int. Default is to return -1, which
	 * is appropriate for generators that do not return numeric values.
	 */
	int nextInt()
	{
		int ret = counter++;
		IntegerGenerator::setLastInt(ret);
		//std::cout<<"nextint:"<<ret<<std::endl;
		return ret;
	}
	int lastInt()
	{
	    return counter - 1;
	}
	double mean() {
		return 0;
		//throw new UnsupportedOperationException("Can't compute mean of non-stationary distribution!");
	}
};
class ZipfianGenerator: public IntegerGenerator
{
public:
	static const double ZIPFIAN_CONSTANT=0.99;
private:
	/**
	 * Number of items.
	 */
	long items;

	/**
	 * Min item to generate.
	 */
	long base;

	/**
	 * The zipfian constant to use.
	 */
	double zipfianconstant;

	/**
	 * Computed parameters for generating the distribution.
	 */
	double alpha,zetan,eta,theta,zeta2theta;

	/**
	 * The number of items used to compute zetan the last time.
	 */
	long countforzeta;

	/**
	 * Flag to prevent problems. If you increase the number of items the zipfian generator is allowed to choose from, this code will incrementally compute a new zeta
	 * value for the larger itemcount. However, if you decrease the number of items, the code computes zeta from scratch; this is expensive for large itemsets.
	 * Usually this is not intentional; e.g. one thread thinks the number of items is 1001 and calls "nextLong()" with that item count; then another thread who thinks the
	 * number of items is 1000 calls nextLong() with itemcount=1000 triggering the expensive recomputation. (It is expensive for 100 million items, not really for 1000 items.) Why
	 * did the second thread think there were only 1000 items? maybe it read the item count before the first thread incremented it. So this flag allows you to say if you really do
	 * want that recomputation. If true, then the code will recompute zeta if the itemcount goes down. If false, the code will assume itemcount only goes up, and never recompute.
	 */
	bool allowitemcountdecrease;

	/******************************* Constructors **************************************/

	/**
	 * Create a zipfian generator for the specified number of items.
	 * @param _items The number of items in the distribution.
	 */
public:
	ZipfianGenerator(long _items)
	{
		new (this)ZipfianGenerator(0,_items-1);
	}

	/**
	 * Create a zipfian generator for items between min and max.
	 * @param _min The smallest integer to generate in the sequence.
	 * @param _max The largest integer to generate in the sequence.
	 */
	ZipfianGenerator(long _min, long _max)
	{
		new (this)ZipfianGenerator(_min,_max,ZIPFIAN_CONSTANT);
	}

	/**
	 * Create a zipfian generator for the specified number of items using the specified zipfian constant.
	 *
	 * @param _items The number of items in the distribution.
	 * @param _zipfianconstant The zipfian constant to use.
	 */
	ZipfianGenerator(long _items, double _zipfianconstant)
	{
		new (this)ZipfianGenerator(0,_items-1,_zipfianconstant);
	}

	/**
	 * Create a zipfian generator for items between min and max (inclusive) for the specified zipfian constant.
	 * @param min The smallest integer to generate in the sequence.
	 * @param max The largest integer to generate in the sequence.
	 * @param _zipfianconstant The zipfian constant to use.
	 */
	ZipfianGenerator(long min, long max, double _zipfianconstant)
	{
		new (this)ZipfianGenerator(min,max,_zipfianconstant,zetastatic(max-min+1,_zipfianconstant));
	}
	/**
	 * Create a zipfian generator for items between min and max (inclusive) for the specified zipfian constant, using the precomputed value of zeta.
	 *
	 * @param min The smallest integer to generate in the sequence.
	 * @param max The largest integer to generate in the sequence.
	 * @param _zipfianconstant The zipfian constant to use.
	 * @param _zetan The precomputed zeta constant.
	 */
	ZipfianGenerator(long min, long max, double _zipfianconstant, double _zetan)
	{
        //std::cout<<"terry is good"<<std::endl;
		allowitemcountdecrease = false;
		items=max-min+1;
		base=min;
		zipfianconstant=_zipfianconstant;

		theta=zipfianconstant;

		zeta2theta=zeta(2,theta);


		alpha=1.0/(1.0-theta);
		//zetan=zeta(items,theta);
		zetan=_zetan;
		countforzeta=items;
		eta=(1-pow(2.0/items,1-theta))/(1-zeta2theta/zetan);
		srand(time(NULL));
		nextInt();
		//System.out.println("XXXX 4 XXXX");
	}

	/**************************************************************************/

	/**
	 * Compute the zeta constant needed for the distribution. Do this from scratch for a distribution with n items, using the
	 * zipfian constant theta. Remember the value of n, so if we change the itemcount, we can recompute zeta.
	 *
	 * @param n The number of items to compute zeta over.
	 * @param theta The zipfian constant.
	 */
	double zeta(long n, double theta)
	{
		countforzeta=n;
		return zetastatic(n,theta);
	}

	/**
	 * Compute the zeta constant needed for the distribution. Do this from scratch for a distribution with n items, using the
	 * zipfian constant theta. This is a static version of the function which will not remember n.
	 * @param n The number of items to compute zeta over.
	 * @param theta The zipfian constant.
	 */
	static double zetastatic(long n, double theta)
	{
		return zetastatic(0,n,theta,0);
	}

	/**
	 * Compute the zeta constant needed for the distribution. Do this incrementally for a distribution that
	 * has n items now but used to have st items. Use the zipfian constant theta. Remember the new value of
	 * n so that if we change the itemcount, we'll know to recompute zeta.
	 *
	 * @param st The number of items used to compute the last initialsum
	 * @param n The number of items to compute zeta over.
	 * @param theta The zipfian constant.
     * @param initialsum The value of zeta we are computing incrementally from.
	 */
	double zeta(long st, long n, double theta, double initialsum)
	{
		countforzeta=n;
		return zetastatic(st,n,theta,initialsum);
	}

	/**
	 * Compute the zeta constant needed for the distribution. Do this incrementally for a distribution that
	 * has n items now but used to have st items. Use the zipfian constant theta. Remember the new value of
	 * n so that if we change the itemcount, we'll know to recompute zeta.
	 * @param st The number of items used to compute the last initialsum
	 * @param n The number of items to compute zeta over.
	 * @param theta The zipfian constant.
     * @param initialsum The value of zeta we are computing incrementally from.
	 */
	static double zetastatic(long st, long n, double theta, double initialsum)
	{
		double sum=initialsum;
        //std::cout<<n<<std::endl;
       // std::cout<<st<<" ";
        //std::cout<<n<<" ";
        //std::cout<<theta<<std::endl;
		for (long i=st; i<n; i++)
		{
			sum+=1/(pow(i+1,theta));
		}

		//std::cout<<"sum="<<sum<<std::endl;

		return sum;
	}

	/****************************************************************************************/
	/**
		 * Generate the next item as a long.
		 *
		 * @param itemcount The number of items in the distribution.
		 * @return The next item in the sequence.
		 */
long nextLong(long itemcount)
{
			//from "Quickly Generating Billion-Record Synthetic Databases", Jim Gray et al, SIGMOD 1994
           // std::cout<<"got here"<<std::endl;
			if (itemcount!=countforzeta)
			{

				//have to recompute zetan and eta, since they depend on itemcount
				if (itemcount>countforzeta)
				{
						//System.err.println("WARNING: Incrementally recomputing Zipfian distribtion. (itemcount="+itemcount+" countforzeta="+countforzeta+")");

						//we have added more items. can compute zetan incrementally, which is cheaper
						zetan=zeta(countforzeta,itemcount,theta,zetan);
						eta=(1-pow(2.0/items,1-theta))/(1-zeta2theta/zetan);
				}
				else if ( (itemcount<countforzeta) && (allowitemcountdecrease) )
				{
						//have to start over with zetan
						//note : for large itemsets, this is very slow. so don't do it!

						//TODO: can also have a negative incremental computation, e.g. if you decrease the number of items, then just subtract
						//the zeta sequence terms for the items that went away. This would be faster than recomputing from scratch when the number of items
						//decreases

						//std::cout<<"WARNING: Recomputing Zipfian distribtion. This is slow and should be avoided. (itemcount="+itemcount+" countforzeta="+countforzeta+")";

						zetan=zeta(itemcount,theta);
						eta=(1-pow(2.0/items,1-theta))/(1-zeta2theta/zetan);
					}
				}

			double u=(double)(rand()/(double)RAND_MAX);
			//std::cout<<"random u="<<u<<std::endl;
			double uz=u*zetan;

			if (uz<1.0)
			{
				return 0;
			}

			if (uz<1.0+pow(0.5,theta))
			{
				return 1;
			}

			long ret=base+(long)((itemcount) * pow(eta*u - eta + 1, alpha));
			setLastInt((int)ret);
			return ret;
		}
	/**
	 * Generate the next item. this distribution will be skewed toward lower integers; e.g. 0 will
	 * be the most popular, 1 the next most popular, etc.
	 * @param itemcount The number of items in the distribution.
	 * @return The next item in the sequence.
	 */
	int nextInt(int itemcount)
	{
		return (int)nextLong(itemcount);
	}



	/**
	 * Return the next value, skewed by the Zipfian distribution. The 0th item will be the most popular, followed by the 1st, followed
	 * by the 2nd, etc. (Or, if min != 0, the min-th item is the most popular, the min+1th item the next most popular, etc.) If you want the
	 * popular items scattered throughout the item space, use ScrambledZipfianGenerator instead.
	 */
	int nextInt()
	{
		return (int)nextLong(items);
	}

	/**
	 * Return the next value, skewed by the Zipfian distribution. The 0th item will be the most popular, followed by the 1st, followed
	 * by the 2nd, etc. (Or, if min != 0, the min-th item is the most popular, the min+1th item the next most popular, etc.) If you want the
	 * popular items scattered throughout the item space, use ScrambledZipfianGenerator instead.
	 */
	long nextLong()
	{
		return nextLong(items);
	}

	/**
	 * @todo Implement ZipfianGenerator.mean()
	 */
	double mean() {
		return 0;
		//throw new UnsupportedOperationException("@todo implement ZipfianGenerator.mean()");
	}
};
class SkewedLatestGenerator:public IntegerGenerator
{
	CounterGenerator _basis;
	ZipfianGenerator *_zipfian;
public:
	SkewedLatestGenerator(CounterGenerator basis):
    _basis(basis)
	{
		_zipfian = new ZipfianGenerator(_basis.lastInt());
		nextInt();
	}

	/**
	 * Generate the next string in the distribution, skewed Zipfian favoring the items most recently returned by the basis generator.
	 */
	int nextInt()
	{
		int max=_basis.lastInt();
		int nextint=max-_zipfian->nextInt(max);
		setLastInt(nextint);
		return nextint;
	}

	double mean() {
		return 0;
		//throw new UnsupportedOperationException("Can't compute mean of non-stationary distribution!");
	}

};


class ScrambledZipfianGenerator: public IntegerGenerator
{

	ZipfianGenerator *gen;
	long _min,_max,_itemcount;
public:
	static const double ZETAN=26.46902820178302;
    static const double USED_ZIPFIAN_CONSTANT=0.99;
	static const long ITEM_COUNT=10000000000L;


	/******************************* Constructors **************************************/

	/**
	 * Create a zipfian generator for the specified number of items.
	 * @param _items The number of items in the distribution.
	 */
	ScrambledZipfianGenerator(long _items)
	{
		new (this)ScrambledZipfianGenerator(0,_items-1);
	}

	/**
	 * Create a zipfian generator for items between min and max.
	 * @param _min The smallest integer to generate in the sequence.
	 * @param _max The largest integer to generate in the sequence.
	 */
	ScrambledZipfianGenerator(long _min, long _max)
	{
		ScrambledZipfianGenerator(_min,_max,ZipfianGenerator::ZIPFIAN_CONSTANT);
	}

	/**
	 * Create a zipfian generator for the specified number of items using the specified zipfian constant.
	 *
	 * @param _items The number of items in the distribution.
	 * @param _zipfianconstant The zipfian constant to use.
	 */
	/*
// not supported, as the value of zeta depends on the zipfian constant, and we have only precomputed zeta for one zipfian constant
	public ScrambledZipfianGenerator(long _items, double _zipfianconstant)
	{
		this(0,_items-1,_zipfianconstant);
	}
*/

	/**
	 * Create a zipfian generator for items between min and max (inclusive) for the specified zipfian constant. If you
	 * use a zipfian constant other than 0.99, this will take a long time to complete because we need to recompute zeta.
	 * @param min The smallest integer to generate in the sequence.
	 * @param max The largest integer to generate in the sequence.
	 * @param _zipfianconstant The zipfian constant to use.
	 */
       ScrambledZipfianGenerator(long min, long max, double _zipfianconstant)
	{
		_min=min;
		_max=max;
		_itemcount=_max-_min+1;
		if (_zipfianconstant == USED_ZIPFIAN_CONSTANT)
		{
		    gen=new ZipfianGenerator(0,ITEM_COUNT,_zipfianconstant,ZETAN);
		} else {
		    gen=new ZipfianGenerator(0,ITEM_COUNT,_zipfianconstant);
		}
	}

	/**************************************************************************************************/
       /**
       	 * Return the next long in the sequence.
       	 */
        long nextLong()
       	{
       		long ret=gen->nextLong();
       		ret=_min+Utils::FNVhash64(ret)%_itemcount;
       		setLastInt((int)ret);
       		return ret;
       	}
	/**
	 * Return the next int in the sequence.
	 */
	int nextInt() {
		return (int)nextLong();
	}

	/**
	 * since the values are scrambled (hopefully uniformly), the mean is simply the middle of the range.
	 */
	double mean() {
		return ((double)(((long)_min) +(long)_max))/2.0;
	}
};

class UniformIntegerGenerator: public IntegerGenerator
{
	int _lb,_ub,_interval;
public:
	/**
	 * Creates a generator that will return integers uniformly randomly from the interval [lb,ub] inclusive (that is, lb and ub are possible values)
	 *
	 * @param lb the lower bound (inclusive) of generated values
	 * @param ub the upper bound (inclusive) of generated values
	 */
	UniformIntegerGenerator(int lb, int ub)
	{
		_lb=lb;
		_ub=ub;
		_interval=_ub-_lb+1;
	}

	int nextInt()
	{
		//srand((unsigned)time(0));
		int ret=rand()%_interval+_lb;
		IntegerGenerator::setLastInt(ret);
		return ret;
	}

	double mean() {
		return ((double)((long)(_lb + (long)_ub))) / 2.0;
	}

};

class UniformGenerator: public Generator
{

	std::vector<std::string> _values;
	std::string _laststring;
	UniformIntegerGenerator *_gen;
public:

	/**
	 * Creates a generator that will return strings from the specified set uniformly randomly
	 */
	UniformGenerator(std::vector<std::string> values)
	{
	    for(int i=0;i<values.size();i++)
	    {
	    	_values.push_back(values.at(i));
	    }
		//_values=(std::vector<std::string>)values.clone();
		_laststring.clear();
		_gen=new UniformIntegerGenerator(0,values.size()-1);
	}

	/**
	 * Generate the next string in the distribution.
	 */
	std::string nextString()
	{
		_laststring=_values.at(_gen->nextInt());
		return _laststring;
	}

	/**
	 * Return the previous string generated by the distribution; e.g., returned from the last nextString() call.
	 * Calling lastString() should not advance the distribution or have any side effects. If nextString() has not yet
	 * been called, lastString() should return something reasonable.
	 */
	std::string lastString()
	{
		if (_laststring.empty())
		{
			nextString();
		}
		return _laststring;
	}
};




}

#endif


