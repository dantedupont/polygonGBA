/* playad.iwram.c
   8AD decoder engine

Copyright 2003 Damian Yerrick

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*/

typedef struct ADGlobals
{
  const unsigned char *data;
  int last_sample;
  int last_index;
} ADGlobals;

extern ADGlobals ad;


static const signed char ima9_step_indices[16] =
{
  -1, -1, -1, -1, 2, 4, 7, 12,
  -1, -1, -1, -1, 2, 4, 7, 12
};

const unsigned short ima_step_table[89] =
{
      7,    8,    9,   10,   11,   12,   13,   14,   16,   17,
     19,   21,   23,   25,   28,   31,   34,   37,   41,   45,
     50,   55,   60,   66,   73,   80,   88,   97,  107,  118,
    130,  143,  157,  173,  190,  209,  230,  253,  279,  307,
    337,  371,  408,  449,  494,  544,  598,  658,  724,  796,
    876,  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
   2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
   5894, 6484, 7132, 7845, 8630, 9493,10442,11487,12635,13899,
  15289,16818,18500,20350,22385,24623,27086,29794,32767
};

static inline int ima9_rescale(int step, unsigned int code)
{
  /* 0,1,2,3,4,5,6,9 */
  int diff = step >> 3;
  if(code & 1)
    diff += step >> 2;
  if(code & 2)
    diff += step >> 1;
  if(code & 4)
    diff += step;
  if((code & 7) == 7)
    diff += step >> 1;
  if(code & 8)
    diff = -diff;
  return diff;
}

void decode_ad(signed char *dst, const unsigned char *src, unsigned int len)
{
  int last_sample = ad.last_sample;
  int index = ad.last_index;
  unsigned int by = 0;

  while(len > 0)
  {
    int step, diff;
    unsigned int code;

    if(index < 0)
      index = 0;
    if(index > 88)
      index = 88;
    step = ima_step_table[index];

    if(len & 1)
      code = by >> 4;
    else
    {
      by = *src++;
      code = by & 0x0f;
    }

    diff = ima9_rescale(step, code);
    index += ima9_step_indices[code & 0x07];

    last_sample += diff;
    if(last_sample < -32768)
      last_sample = -32768;
    if(last_sample > 32767)
      last_sample = 32767;
    *dst++ = last_sample >> 8;

    len--;
  }

  ad.last_index = index;
  ad.last_sample = last_sample;
}
