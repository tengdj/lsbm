/*
 * generator.cc
 *
 *  Created on: Mar 17, 2015
 *      Author: teng
 */
#include "generator.h"

#include <cmath>
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
namespace generator{
void YCSBKeyGenerator::sort(long long *num, int top, int bottom)
  {
  	int middle;
  	if (top < bottom)
  	{
  		middle = partition(num, top, bottom);
  		sort(num, top, middle);   // sort first section
  		sort(num, middle+1, bottom);    // sort second section
  	}
  	return;
  }

  int YCSBKeyGenerator::partition(long long *array, int top, int bottom)
  {
  	long long x = array[top];
  	int i = top - 1;
  	int j = bottom + 1;
  	long long temp;
  	do
  	{
  		do
  		{
  			j--;
  		}while (x >array[j]);

  		do
  		{
  			i++;
  		} while (x <array[i]);

  		if (i < j)
  		{
  			temp = array[i];
  			array[i] = array[j];
  			array[j] = temp;
  		}
  	}while (i < j);
  	return j;           // returns middle subscript
  }

  long long YCSBKeyGenerator::nextKey()
  {
  	return keypool[index++];
  }

}

