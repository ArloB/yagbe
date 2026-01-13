#ifndef APU_HPP
#define APU_HPP

#include <array>
#include <cstdint>
#include <vector>

#include <SDL3/SDL_audio.h>

// APU Hardware Constants
namespace APUConstants {
// Register bit masks
constexpr uint8_t TRIGGER_BIT = 0x80;
constexpr uint8_t LENGTH_ENABLE_BIT = 0x40;
constexpr uint8_t DAC_ENABLE_MASK = 0xF8;

// Channel length counter maximums
constexpr uint8_t MAX_SQUARE_LENGTH = 64;
constexpr uint16_t MAX_WAVE_LENGTH = 256;

// Frequency and volume limits
constexpr uint16_t MAX_FREQUENCY = 2047;
constexpr uint8_t MAX_ENVELOPE_VOLUME = 15;

// Timing constants
constexpr uint32_t FRAME_SEQUENCER_PERIOD = 8192;
constexpr uint32_t FRAME_SEQUENCER_HALF_PERIOD = 512;

// LFSR initial value for noise channel
constexpr uint16_t LFSR_INITIAL_VALUE = 0x7FFF;
} // namespace APUConstants

namespace Channels {
struct SquareChannel {
  bool enabled;
  bool dac;
  bool pan_left;
  bool pan_right;
  uint16_t period;
  uint16_t period_counter;
  uint8_t duty_cycle;
  uint8_t length_counter;
  bool length_enabled;
  bool length_frozen;
  uint8_t volume;
  uint8_t envelope_volume;
  uint8_t envelope_pace;
  uint8_t envelope_counter;
  bool envelope_direction;
  bool trigger;
  uint8_t duty_step;
};

struct SquareSweepChannel : SquareChannel {
  uint8_t sweep_pace;
  bool sweep_direction;
  uint8_t sweep_step;
  uint8_t sweep_counter;
  uint16_t shadow_period;
  bool sweep_negate_used;
};

struct WaveChannel {
  bool enabled;
  bool dac;
  bool pan_left;
  bool pan_right;
  uint16_t period;
  uint16_t period_counter;
  uint16_t length_counter;
  bool length_enabled;
  bool length_frozen;
  uint8_t volume_shift;
  uint8_t wave_position;
  bool trigger;
};

struct NoiseChannel {
  bool enabled;
  bool dac;
  bool pan_left;
  bool pan_right;
  uint16_t lfsr;
  uint16_t period_counter;
  uint8_t length_counter;
  bool length_enabled;
  bool length_frozen;
  uint8_t volume;
  uint8_t envelope_volume;
  uint8_t envelope_pace;
  uint8_t envelope_counter;
  bool envelope_direction;
  uint8_t divisor_code;
  uint8_t shift_amount;
  bool lfsr_width;
  bool trigger;
};
} // namespace Channels

class APU {
public:
  APU();
  ~APU();

  friend class APUTester;

  void step(uint8_t m_cycles);
  uint8_t readRegister(uint16_t addr);
  void writeRegister(uint16_t addr, uint8_t value);

private:
  std::array<uint8_t, 0x20> registers;
  std::array<uint8_t, 0x20> wave_ram;

  SDL_AudioStream *stream;

  Channels::SquareSweepChannel square1;
  Channels::SquareChannel square2;
  Channels::WaveChannel wave;
  Channels::NoiseChannel noise;

  uint8_t frame_sequencer_step;
  uint32_t frame_sequencer_sub_counter;

  std::vector<float> sample_buffer;
  static constexpr uint32_t sample_buffer_size = 512;
  static constexpr int target_queue_bytes = 4096;

  uint8_t left_volume;
  uint8_t right_volume;

  uint32_t cycle_accumulator;
  uint32_t frame_cycle_accumulator;
  uint32_t tcycle_accumulator;

  void initializeAudio();
  void updateFrameSequencer();
  float generateSquareSample(Channels::SquareChannel &ch);
  float generateWaveSample();
  float generateNoiseSample();
  void mixAndOutput();
  void updateChannel1();
  void updateChannel2();
  void updateChannel3();
  void updateChannel4();
  void handleChannel1Trigger();
  void handleChannel2Trigger();
  void handleChannel3Trigger();
  void handleChannel4Trigger();
  void updateSweep();
  void updateEnvelope(Channels::SquareChannel &ch);
  void updateNoiseEnvelope();
  void updateLengthCounter(Channels::SquareChannel &ch);
  void updateWaveLengthCounter();
  uint16_t calculateSweepFrequency();
  void updateNoiseLengthCounter();
  bool isMasterEnabled() const;
  uint16_t calculatePeriodCounter(uint16_t period) const;
  uint16_t calculateNoisePeriod() const;
  bool shouldClockLengthOnEnable() const;
};

#endif