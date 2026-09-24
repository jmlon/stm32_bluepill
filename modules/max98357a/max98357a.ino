// MAX98357A I2S demo for an STM32F103C8T6 (Blue Pill).
//
// The STM32F103C8 is a medium-density part and has no I2S hardware: the I2S
// mode of SPI2/SPI3 exists only on high-density, XL-density and connectivity
// line devices (RM0008).  The CMSIS header still defines I2SCFGR/I2SPR, so
// code using them compiles, but on this chip they are reserved and do nothing.
//
// I2S is therefore assembled from two peripherals the C8 does have:
//   * SPI2, master, 16-bit frames, CPOL=0/CPHA=0, supplies BCLK (SCK) and the
//     serial data (MOSI).  Each stereo frame is two 16-bit words, so there
//     are 32 BCLK cycles per LRCLK period.
//   * TIM2 is clocked from BCLK through its ETR input (external clock mode 2)
//     and toggles LRCLK on CH2 every 16 BCLK rising edges.  The toggle happens
//     after the 15th edge of each word, so the new LRCLK level is latched with
//     the word's LSB: one BCLK before the next MSB, as I2S requires.  Because
//     the timer counts the real BCLK edges, LRCLK cannot drift out of step with
//     the data even if the SPI stream stalls briefly.
//
// Connections:
//   MAX98357A BCLK    -> PB13 (SPI2_SCK)
//   PB13              -> PA0  (TIM2_ETR) -- jumper wire on the Blue Pill
//   MAX98357A LRC     -> PA1  (TIM2_CH2)
//   MAX98357A DIN     -> PB15 (SPI2_MOSI)
//   MAX98357A GND     -> Blue Pill GND
//   MAX98357A VIN     -> 5 V (or 3.3 V; 5 V gives more output power)
//   MAX98357A GAIN/SD -> leave open for 9 dB gain, (L+R)/2 mono
//
// The MAX98357A is a digital I2S amplifier, not an SPI peripheral.  Do not
// share these SPI2 pins with other SPI devices.

#include <Arduino.h>
#include <math.h>
#include "stm32f1xx.h"

namespace {

// SPI2 is on APB1, which the core runs at 36 MHz (SYSCLK 72 MHz / 2).
constexpr uint32_t APB1_HZ = 36000000;
constexpr uint32_t SPI_DIVIDER = 32;        // CR1.BR = 0b100
constexpr uint32_t BCLK_PER_FRAME = 32;     // 2 channels x 16 bits
// BCLK = 1.125 MHz, sample rate = 35156.25 Hz.  The MAX98357A accepts any
// sample rate from 8 kHz to 96 kHz with 32 BCLK per LRCLK period.
constexpr float SAMPLE_RATE =
    static_cast<float>(APB1_HZ) / (SPI_DIVIDER * BCLK_PER_FRAME);

constexpr uint32_t TONE_HZ = 1000;
constexpr int16_t AMPLITUDE = 12000;

// Play the tone for this long, fading out over the last few milliseconds so
// the stop does not click, then stop the clocks.
constexpr float TONE_SECONDS = 2.0f;
constexpr float FADE_SECONDS = 0.02f;
constexpr uint32_t TONE_FRAMES = static_cast<uint32_t>(TONE_SECONDS * SAMPLE_RATE);
constexpr uint32_t FADE_FRAMES = static_cast<uint32_t>(FADE_SECONDS * SAMPLE_RATE);

// A soft-float sinf() per sample is too slow to keep BCLK running
// continuously, so the waveform comes from a table built once in setup().
constexpr uint32_t SINE_TABLE_BITS = 8;
constexpr uint32_t SINE_TABLE_SIZE = 1U << SINE_TABLE_BITS;
int16_t sineTable[SINE_TABLE_SIZE];

void buildSineTable() {
  for (uint32_t i = 0; i < SINE_TABLE_SIZE; ++i) {
    const float radians = 2.0f * PI * i / SINE_TABLE_SIZE;
    sineTable[i] = static_cast<int16_t>(lroundf(sinf(radians) * AMPLITUDE));
  }
}

void configurePins() {
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;

  // PA0 (TIM2_ETR): floating input, CNF=01 MODE=00 (0x4).
  // PA1 (TIM2_CH2, LRCLK): alternate-function push-pull, 50 MHz (0xB).
  constexpr uint32_t PA_MASK = (0xFU << 0) | (0xFU << 4);
  GPIOA->CRL = (GPIOA->CRL & ~PA_MASK) | (0x4U << 0) | (0xBU << 4);

  // PB13 (SCK, BCLK) and PB15 (MOSI, DIN): alternate-function push-pull,
  // 50 MHz.  PB14 (MISO) is unused and stays a GPIO.
  constexpr uint32_t PB_MASK = (0xFU << 20) | (0xFU << 28);
  GPIOB->CRH = (GPIOB->CRH & ~PB_MASK) | (0xBU << 20) | (0xBU << 28);
}

void configureLrclkTimer() {
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

  TIM2->CR1 = 0;
  // External clock mode 2: count rising edges on ETR, no prescaler or filter.
  TIM2->SMCR = TIM_SMCR_ECE;
  TIM2->PSC = 0;
  TIM2->ARR = 15;
  TIM2->CCR2 = 15;
  // Force OC2REF low first so LRCLK starts on the left channel, then switch
  // to toggle-on-match.
  TIM2->CCMR1 = TIM_CCMR1_OC2M_2;
  TIM2->CCMR1 = TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_0;
  TIM2->CCER = TIM_CCER_CC2E;
  TIM2->EGR = TIM_EGR_UG;
}

void configureSpi2() {
  RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

  // Master, fPCLK/32, 16-bit frames, MSB first, CPOL=0/CPHA=0 (data changes
  // on the falling edge and is sampled on the rising edge, as in I2S).
  // Software NSS held high so the unused PB12 cannot raise a mode fault.
  SPI2->CR1 = 0;
  SPI2->CR2 = 0;
  SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_2 | SPI_CR1_DFF |
              SPI_CR1_SSM | SPI_CR1_SSI;
  SPI2->CR1 |= SPI_CR1_SPE;
}

void startI2S() {
  configurePins();
  configureLrclkTimer();
  configureSpi2();

  // Start counting only once SCK is idling low, so any edge produced while
  // the pins and SPI were being set up does not skew the LRCLK phase.
  TIM2->CNT = 0;
  TIM2->CR1 = TIM_CR1_CEN;
}

void writeStereoSample(int16_t sample) {
  // A MAX98357A selects one I2S channel (or the mix) using its SD_MODE pin.
  // Sending the same value in both slots makes the demo work regardless of
  // that setting and produces a mono tone from the amplifier.
  while ((SPI2->SR & SPI_SR_TXE) == 0) {
  }
  SPI2->DR = static_cast<uint16_t>(sample);

  while ((SPI2->SR & SPI_SR_TXE) == 0) {
  }
  SPI2->DR = static_cast<uint16_t>(sample);
}

void stopI2S() {
  // Let the last frame shift out completely before disabling SPI2 (RM0008
  // procedure: wait for TXE, then for BSY to clear).  With BCLK stopped the
  // MAX98357A detects the missing clock and mutes its output.
  while ((SPI2->SR & SPI_SR_TXE) == 0) {
  }
  while (SPI2->SR & SPI_SR_BSY) {
  }
  SPI2->CR1 &= ~SPI_CR1_SPE;
  TIM2->CR1 = 0;
}

}  // namespace

void setup() {
  buildSineTable();
  startI2S();
}

void loop() {
  // Keep phase continuous between loop iterations.  The top bits of the
  // 32-bit phase accumulator index the sine table.
  static uint32_t phase = 0;
  static uint32_t framesSent = 0;
  static const uint32_t PHASE_STEP =
      static_cast<uint32_t>(TONE_HZ * 4294967296.0 / SAMPLE_RATE);

  if (framesSent >= TONE_FRAMES) {
    return;
  }

  // Stream a modest-sized block, then return to the Arduino loop.  The writes
  // are blocking; each 16-bit word takes about 1000 CPU cycles to shift out,
  // which leaves ample time for the table lookup and the loop() overhead
  // without needing a DMA buffer in the Blue Pill's 20 KiB of SRAM.
  for (uint16_t i = 0; i < 256 && framesSent < TONE_FRAMES; ++i) {
    int32_t sample = sineTable[phase >> (32 - SINE_TABLE_BITS)];
    const uint32_t framesLeft = TONE_FRAMES - framesSent;
    if (framesLeft < FADE_FRAMES) {
      sample = sample * static_cast<int32_t>(framesLeft) / FADE_FRAMES;
    }
    writeStereoSample(static_cast<int16_t>(sample));
    phase += PHASE_STEP;
    ++framesSent;
  }

  if (framesSent >= TONE_FRAMES) {
    stopI2S();
  }
}
