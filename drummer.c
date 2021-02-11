///////////////////////////////////////////////////////////////////////
// drummer.c : a simple Drum machine for the Raspberry Pi Pico
//
// (c) 2021 Mike Field <hamster@snap.net.nz>
//
// Really just a test of making an I2S interface. Plays a few different
// bars of drum patterns
//
///////////////////////////////////////////////////////////////////////
// This section is all the hardware specific DMA stuff
///////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <memory.h>
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pio_i2s.pio.h"

#define PIO_I2S_CLKDIV 44.25F
#define N_BUFFERS 4
#define BUFFER_SIZE 49

static int dma_chan;
static uint32_t buffer[N_BUFFERS][BUFFER_SIZE];
static volatile int buffer_playing = 0;
static int buffer_to_fill = 0;

static void dma_handler() {
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << dma_chan;
    // Give the channel a new wave table entry to read from, and re-trigger it
    dma_channel_set_read_addr(dma_chan, buffer[buffer_playing], true);
    if(buffer_playing == N_BUFFERS-1) 
       buffer_playing = 0;
    else
       buffer_playing++;
}


int setup_dma(void) {
     
    //////////////////////////////////////////////////////
    // Set up a PIO state machine to serialise our bits
    uint offset = pio_add_program(pio0, &pio_i2s_program);
    pio_i2s_program_init(pio0, 0, offset, 9, PIO_I2S_CLKDIV);

    //////////////////////////////////////////////////////
    // Configure a channel to write the buffers to PIO0 SM0's TX FIFO, 
    // paced by the data request signal from that peripheral.
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, 1); 
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);

    dma_channel_configure(
        dma_chan,
        &c,
        &pio0_hw->txf[0], // Write address (only need to set this once)
        NULL,
        BUFFER_SIZE,        
        false             // Don't start yet
    );

    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(dma_chan, true);

    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

}


///////////////////////////////////////////////////////////////////////////////////
// Everything from here down is non-h/w specific
///////////////////////////////////////////////////////////////////////////////////
// Drum samples
#include "samples/drum_clap.h"
#include "samples/drum_hihat.h"
#include "samples/drum_kick.h"
#include "samples/drum_perc.h"
#include "samples/drum_snare.h"

#define N_VOICES 6

#define BPM            155
#define SAMPLE_RATE    ((int)(125000000/PIO_I2S_CLKDIV/32/2))
#define BEATS_PER_BAR  4
#define BAR_LEN        72
#define LOOP_BARS      10

// Counters for where we are in time
static int bar = 0;
static int tickOfBar = 0;
static int sampleOfTick = 0;
static int samplesPerBeat;
static int samplesPerTick;


const struct Sounds {
    const int16_t *samples;
    const size_t  len;
} sounds[] = {
   {drum_kick,  sizeof(drum_kick)/sizeof(int16_t)},
   {drum_clap,  sizeof(drum_clap)/sizeof(int16_t)},
   {drum_snare, sizeof(drum_snare)/sizeof(int16_t)},
   {drum_hihat, sizeof(drum_hihat)/sizeof(int16_t)},
   {drum_perc,  sizeof(drum_perc)/sizeof(int16_t)}
};

// Parameters for volume and pan
static struct voice {
    int pan;
    int volume;
    int sample;
    int pos;
    int emph;
} voices[N_VOICES] = {
   { 16,  192, 0, 0, 0},
   {  8,    0, 1, 0, 0},
   { 16,   40, 2, 0, 0},
   { 18,   80, 3, 0, 0},
   { 28,   30, 4, 0, 0},
   {  4,   30, 0, 0, 0}
};


struct Pattern {
   char pattern[N_VOICES][BAR_LEN];
};


const static struct Pattern pattern0 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "1        1                     1    1        1                          ",
  "                                                                        ",
  "                  1                                   1                 ",
  "                                                                        ",
  "1        1        1        1        1        1        1        1        ",
  "                                                                        "
}};

const static struct Pattern pattern1 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "9                                   1                                   ",
  "                                                                        ",
  "4        1        1        1        3        1        1        1        ",
  "                                                                        ",
  "                                                                        ",
  "                                                                        "
}};

const static struct Pattern pattern2 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "9                                   1                                   ",
  "         1                 1                 1                 1        ",
  "1                 1                 1                 1                 ",
  "5        1        1        1        1        1                 1        ",
  "5                          1                 1                          ",
  "         1                          4                          1        "
}};


static const struct Pattern *patterns[LOOP_BARS] = {
   &pattern0,
   &pattern1,
   &pattern2
};


static const int bars[LOOP_BARS] = { 0,0,0,2,2,2,2,2,1,0};


static uint32_t generate_sample(void) {
   int32_t samples[N_VOICES];
   int32_t output[2];

   // Start of new tick?
   if(sampleOfTick == 0) {
      for(int i = 0; i < N_VOICES; i++) {
         if(patterns[bars[bar]]->pattern[i][tickOfBar] != ' ') {
            voices[i].pos  = 1;
            voices[i].emph = (patterns[bars[bar]]->pattern[i][tickOfBar]-'1') * 12;
         }
      }
   }

   for(int i = 0; i < N_VOICES; i++) {
      samples[i] = 0;
      if(voices[i].sample >= 0) {
         samples[i] = sounds[voices[i].sample].samples[voices[i].pos ];
         if(voices[i].pos != 0) {
            voices[i].pos++;
            if(voices[i].pos  >= sounds[voices[i].sample].len) {
               voices[i].pos  = 0;
            }
         }
      }
   }

   output[0] = 0;
   output[1] = 0;
   for(int i = 0; i < N_VOICES; i++) {
      int vol = voices[i].emph + voices[i].volume;
      output[0] += vol * samples[i]*(voices[i].pan);
      output[1] += vol * samples[i]*(32-voices[i].pan);
   }

   sampleOfTick++;
   if(sampleOfTick == samplesPerTick) {
      sampleOfTick = 0;
      tickOfBar++;
      if(tickOfBar == BAR_LEN) {
         tickOfBar = 0;
         bar++;
         if(bar == LOOP_BARS)
            bar = 0;
      }
   }

   // Convert samples back to 16 bit
   return (((output[0] >> 15)&0xFFFF) << 16) + ((output[1] >> 15) & 0xFFFF);
}


static void drum_fill_buffer(void) {
    if(buffer_playing == buffer_to_fill)
       return;

    for(int i = 0; i < BUFFER_SIZE; i++) {
       buffer[buffer_to_fill][i] = generate_sample();
    }
    buffer_to_fill = (buffer_to_fill+1)%N_BUFFERS;
}


int main(void) {
    ////////////////////////////////////////////////////////////
    // Calculate the timing parameters and fill all the buffers
    ////////////////////////////////////////////////////////////
    samplesPerBeat = SAMPLE_RATE*60/BPM;
    samplesPerTick = samplesPerBeat * BEATS_PER_BAR / BAR_LEN;
    buffer_playing = -1; // To stop the filling routine from stalling
    for(int i = 0; i < N_BUFFERS; i++) {
       drum_fill_buffer();
    }
    buffer_playing = N_BUFFERS-1;

    ////////////////////////////////////////////////////////////
    // Set up the DMA transfers, then call the 
    // handler to trigger the first transfer
    ////////////////////////////////////////////////////////////
    setup_dma();
    dma_handler();

    ////////////////////////////////////////////////////////////
    // Fill buffers with new samples as they are consumed
    // by the DMA transfers
    ////////////////////////////////////////////////////////////
    while (true) {
       drum_fill_buffer();
    }
}
