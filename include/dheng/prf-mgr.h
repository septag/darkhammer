/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/


#ifndef PRF_MGR_H_
#define PRF_MGR_H_

#include "dhcore/types.h"

#if defined(_PROFILE_)
#define PRF_OPENSAMPLE(name) prf_opensample((name), __FILE__, __LINE__)
#define PRF_CLOSESAMPLE() prf_closesample()
#else
#define PRF_OPENSAMPLE(name)
#define PRF_CLOSESAMPLE()
#endif

void prf_zero();
result_t prf_initmgr();
void prf_releasemgr();

/**
 * open performance point by a given name
 * on the code where 'point' is opened, the time is saved
 */
void prf_opensample(const char* name, const char* file, uint line);

/**
 * close the point opened previously by prf_opensample.
 * saves time difference and put it in the hierarchy of 'points'
 */
void prf_closesample();

/**
 * clears saved profile points
 */
void prf_presentsamples(fl64 ft);

#endif /* PRF_MGR_H_ */
