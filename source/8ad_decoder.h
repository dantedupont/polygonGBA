#ifndef EIGHTAD_DECODER_H
#define EIGHTAD_DECODER_H

// 8AD decoder state structure (from Pin Eight's implementation)
typedef struct ADGlobals
{
  const unsigned char *data;
  int last_sample;
  int last_index;
} ADGlobals;

// Function prototypes for 8AD decoder
void decode_ad(ADGlobals *decoder, signed char *dst, const unsigned char *src, unsigned int len);
void init_8ad_decoder(ADGlobals *ad, const unsigned char *data);

// 8AD step table (from Pin Eight's implementation)
extern const unsigned short ima_step_table[89];

#endif