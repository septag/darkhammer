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

#include <time.h>
#include <stdlib.h>
#include "numeric.h"
#include "err.h"

void rand_seed()
{
    srand((unsigned int)time(NULL));
}

bool_t rand_flipcoin(uint prob)
{
    return ((uint)rand_getn(0, 100) <= prob);
}

int rand_getn(int min, int max)
{
    int r = rand();
    return ((r % (max-min+1)) + min);
}

float rand_getf(float min, float max)
{
    fl64 r = ((fl64)rand())/RAND_MAX;   /* [0, 1] */
    return (float)((r * (max-min)) + min);
}

