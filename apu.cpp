#include "apu.hpp"
#include "gba.hpp"
#include <SDL3/SDL.h>
#include <cstdint>

APU::APU() {
  registers.fill(0);
  wave_ram.fill(0);

  square1 = {};
  square2 = {};
  wave = {};
  noise = {};

  square1.duty_step = 0;
  square2.duty_step = 0;

  frame_sequencer_step = 0;
  frame_sequencer_sub_counter = 0;
  cycle_accumulator = 0;
  frame_cycle_accumulator = 0;
  tcycle_accumulator = 0;

  left_volume = 7;
  right_volume = 7;

  initializeAudio();
}

void APU::initializeAudio() {
#ifndef UNIT_TEST
  SDL_AudioSpec spec;
  spec.freq = 44100;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;

  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                     NULL, NULL);
  if (stream) {
    SDL_ResumeAudioStreamDevice(stream);
    SDL_SetAudioStreamGain(stream, 0.3f);
  } else {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                             "Could not initialize audio", NULL);
    shouldExit = true;
    return;
  }

  sample_buffer.reserve(sample_buffer_size * 2);
#endif
}

APU::~APU() {
#ifndef UNIT_TEST
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = nullptr;
  }
#endif
}

bool APU::isMasterEnabled() const { return (registers[0x16] & 0x80) != 0; }

uint16_t APU::calculatePeriodCounter(uint16_t period) const {
  return 2048 - period;
}

uint16_t APU::calculateNoisePeriod() const {
  static const uint8_t divisor_table[8] = {8, 16, 32, 48, 64, 80, 96, 112};
  return divisor_table[noise.divisor_code] << noise.shift_amount;
}

bool APU::shouldClockLengthOnEnable() const {
  return frame_sequencer_sub_counter <
         APUConstants::FRAME_SEQUENCER_HALF_PERIOD;
}

uint16_t APU::calculateSweepFrequency() {
  uint16_t change = square1.shadow_period >> square1.sweep_step;
  return square1.sweep_direction ? square1.shadow_period - change
                                 : square1.shadow_period + change;
}

void APU::handleChannel1Trigger() {
  square1.enabled = true;
  if (square1.length_counter == 0) {
    square1.length_counter = APUConstants::MAX_SQUARE_LENGTH;
    square1.length_frozen = false;
  }

  // Extra Clock on Trigger: If length is enabled and we're in the first half
  // of the length period (0-511 T-cycles into an 8192 T-cycle frame),
  // immediately clock the length counter once
  if (square1.length_enabled && shouldClockLengthOnEnable()) {
    if (!square1.length_frozen && square1.length_counter > 0) {
      if (--square1.length_counter == 0) {
        square1.length_frozen = true;
        square1.enabled = false;
      }
    }
  }

  // Reload Timer
  uint16_t period_val = calculatePeriodCounter(square1.period);
  square1.period_counter = period_val ? period_val : 1;

  // Reload Envelope
  square1.envelope_counter = square1.envelope_pace;
  square1.volume = square1.envelope_volume;

  // Reload Sweep
  square1.sweep_counter = square1.sweep_pace ? square1.sweep_pace : 8;
  square1.shadow_period = square1.period;
  square1.sweep_negate_used = false;

  // Check if sweep disables channel immediately (overflow check)
  if (square1.sweep_step > 0 &&
      calculateSweepFrequency() > APUConstants::MAX_FREQUENCY) {
    square1.enabled = false;
  }
}

void APU::handleChannel2Trigger() {
  if (square2.dac)
    square2.enabled = true;

  uint16_t period_val = calculatePeriodCounter(square2.period);
  square2.period_counter = period_val ? period_val : 1;
  square2.volume = square2.envelope_volume;
  square2.envelope_counter = square2.envelope_pace ? square2.envelope_pace : 8;

  if (square2.length_frozen) {
    square2.length_frozen = false;
    square2.length_counter = 64;
    if (square2.length_enabled && --square2.length_counter == 0) {
      square2.length_frozen = true;
      square2.enabled = false;
    }
  } else if (!square2.length_enabled) {
    square2.length_counter = 64;
  }
}

void APU::handleChannel3Trigger() {
  if (wave.dac)
    wave.enabled = true;

  uint16_t period_val = calculatePeriodCounter(wave.period);
  wave.period_counter = period_val ? period_val : 1;
  wave.wave_position = 0;

  uint8_t length_data = registers[0x0B];
  if (wave.length_frozen) {
    wave.length_frozen = false;
    wave.length_counter = APUConstants::MAX_WAVE_LENGTH;
    if (wave.length_enabled && --wave.length_counter == 0) {
      wave.length_frozen = true;
      wave.enabled = false;
    }
  } else {
    wave.length_counter = length_data
                              ? (APUConstants::MAX_WAVE_LENGTH - length_data)
                              : APUConstants::MAX_WAVE_LENGTH;
  }
}

void APU::handleChannel4Trigger() {
  if (noise.dac)
    noise.enabled = true;

  noise.period_counter = calculateNoisePeriod();
  noise.lfsr = APUConstants::LFSR_INITIAL_VALUE;
  noise.volume = noise.envelope_volume;
  noise.envelope_counter = noise.envelope_pace ? noise.envelope_pace : 8;

  if (noise.length_frozen) {
    noise.length_frozen = false;
    noise.length_counter = 64;
    if (noise.length_enabled && --noise.length_counter == 0) {
      noise.length_frozen = true;
      noise.enabled = false;
    }
  } else if (!noise.length_enabled) {
    noise.length_counter = 64;
  }
}

void APU::updateSweep() {
  if (--square1.sweep_counter == 0) {
    square1.sweep_counter = square1.sweep_pace ? square1.sweep_pace : 8;

    if (square1.sweep_step > 0 && square1.shadow_period > 0) {
      uint16_t change = square1.shadow_period >> square1.sweep_step;
      uint16_t new_period = square1.sweep_direction
                                ? square1.shadow_period - change
                                : square1.shadow_period + change;

      if (new_period > APUConstants::MAX_FREQUENCY) {
        square1.enabled = false;
      } else {
        square1.shadow_period = new_period;
        square1.period = new_period;
        square1.period_counter = calculatePeriodCounter(square1.period);

        // Perform overflow check again with the new period
        // This is a quirk of the hardware - even after a successful sweep,
        // we check if another sweep would overflow
        if (square1.sweep_step > 0 && square1.shadow_period > 0) {
          uint16_t change2 = square1.shadow_period >> square1.sweep_step;
          uint16_t new_period2 = square1.sweep_direction
                                     ? square1.shadow_period - change2
                                     : square1.shadow_period + change2;
          if (new_period2 > APUConstants::MAX_FREQUENCY)
            square1.enabled = false;
        }
        // Track if negate mode was used (prevents certain register writes)
        if (square1.sweep_direction)
          square1.sweep_negate_used = true;
      }
    }
  }
}

// Generic envelope update - works for both square and noise channels
template <typename ChannelType> void updateEnvelopeGeneric(ChannelType &ch) {
  if (ch.envelope_pace == 0)
    return;

  if (--ch.envelope_counter == 0) {
    ch.envelope_counter = ch.envelope_pace ? ch.envelope_pace : 8;
    if (ch.envelope_direction) {
      if (ch.volume < APUConstants::MAX_ENVELOPE_VOLUME)
        ch.volume++;
    } else {
      if (ch.volume > 0)
        ch.volume--;
    }
  }
}

void APU::updateEnvelope(Channels::SquareChannel &ch) {
  updateEnvelopeGeneric(ch);
}

void APU::updateNoiseEnvelope() { updateEnvelopeGeneric(noise); }

// Generic length counter update - works for all channel types
template <typename ChannelType>
void updateLengthCounterGeneric(ChannelType &ch) {
  if (ch.length_enabled && !ch.length_frozen && ch.length_counter > 0) {
    if (--ch.length_counter == 0) {
      ch.length_frozen = true;
      ch.enabled = false;
    }
  }
}

void APU::updateLengthCounter(Channels::SquareChannel &ch) {
  updateLengthCounterGeneric(ch);
}

void APU::updateWaveLengthCounter() { updateLengthCounterGeneric(wave); }

void APU::updateNoiseLengthCounter() { updateLengthCounterGeneric(noise); }

void APU::updateFrameSequencer() {
  // Frame sequencer runs at 512 Hz (every 8192 T-cycles)
  // Steps cycle from 0-7, each controlling different APU subsystems:
  // Step 0, 2, 4, 6: Length counters clock (256 Hz)
  // Step 2, 6: Sweep clocks (128 Hz)
  // Step 7: Volume envelopes clock (64 Hz)

  switch (frame_sequencer_step) {
  case 0:
  case 4:
    // Length counter only
    updateLengthCounter(square1);
    updateLengthCounter(square2);
    updateWaveLengthCounter();
    updateNoiseLengthCounter();
    break;
  case 2:
  case 6:
    // Length counter + Sweep
    updateLengthCounter(square1);
    updateLengthCounter(square2);
    updateWaveLengthCounter();
    updateNoiseLengthCounter();
    updateSweep();
    break;
  case 7:
    // Volume envelopes
    updateEnvelope(square1);
    updateEnvelope(square2);
    updateNoiseEnvelope();
    break;
  }
  frame_sequencer_step = (frame_sequencer_step + 1) & 7;
}

float APU::generateSquareSample(Channels::SquareChannel &ch) {
  if (!ch.enabled || !ch.dac)
    return 0.0f;

  static const uint8_t duty_patterns[4][8] = {{0, 0, 0, 0, 0, 0, 0, 1},
                                              {1, 0, 0, 0, 0, 0, 0, 1},
                                              {1, 0, 0, 0, 0, 1, 1, 1},
                                              {0, 1, 1, 1, 1, 1, 1, 0}};

  return (duty_patterns[ch.duty_cycle][ch.duty_step] ? 1.0f : -1.0f) *
         (ch.volume / 15.0f);
}

float APU::generateWaveSample() {
  if (!wave.enabled || !wave.dac)
    return 0.0f;

  uint8_t byte_index = wave.wave_position >> 1;
  uint8_t sample_4bit = (wave.wave_position & 1)
                            ? (wave_ram[byte_index] & 0x0F)
                            : ((wave_ram[byte_index] >> 4) & 0x0F);

  float sample = (sample_4bit / 7.5f) - 1.0f;

  static const float volume_mult[4] = {0.0f, 1.0f, 0.5f, 0.25f};
  return sample * volume_mult[wave.volume_shift];
}

float APU::generateNoiseSample() {
  if (!noise.enabled || !noise.dac)
    return 0.0f;
  return ((~noise.lfsr) & 1 ? 1.0f : -1.0f) * (noise.volume / 15.0f);
}

void APU::updateChannel1() {
  if (square1.enabled && --square1.period_counter == 0) {
    square1.period_counter = calculatePeriodCounter(square1.period);
    square1.duty_step = (square1.duty_step + 1) & 7;
  }
}

void APU::updateChannel2() {
  if (square2.enabled && --square2.period_counter == 0) {
    square2.period_counter = calculatePeriodCounter(square2.period);
    square2.duty_step = (square2.duty_step + 1) & 7;
  }
}

void APU::updateChannel3() {
  if (wave.enabled && --wave.period_counter == 0) {
    wave.period_counter = calculatePeriodCounter(wave.period);
    wave.wave_position = (wave.wave_position + 1) & 31;
  }
}

void APU::updateChannel4() {
  if (!noise.enabled)
    return;

  if (--noise.period_counter == 0) {
    noise.period_counter = calculateNoisePeriod();
    uint8_t xor_result = (noise.lfsr & 1) ^ ((noise.lfsr >> 1) & 1);
    noise.lfsr = (noise.lfsr >> 1) | (xor_result << 14);
    if (noise.lfsr_width) {
      noise.lfsr = (noise.lfsr & ~(1 << 6)) | (xor_result << 6);
    }
  }
}

void APU::mixAndOutput() {
#ifndef UNIT_TEST
  if (!stream)
    return;

  float ch1 = generateSquareSample(square1);
  float ch2 = generateSquareSample(square2);
  float ch3 = generateWaveSample();
  float ch4 = generateNoiseSample();

  float left = 0.0f, right = 0.0f;
  int left_cnt = 0, right_cnt = 0;

  if (square1.pan_left) {
    left += ch1;
    left_cnt++;
  }
  if (square1.pan_right) {
    right += ch1;
    right_cnt++;
  }
  if (square2.pan_left) {
    left += ch2;
    left_cnt++;
  }
  if (square2.pan_right) {
    right += ch2;
    right_cnt++;
  }
  if (wave.pan_left) {
    left += ch3;
    left_cnt++;
  }
  if (wave.pan_right) {
    right += ch3;
    right_cnt++;
  }
  if (noise.pan_left) {
    left += ch4;
    left_cnt++;
  }
  if (noise.pan_right) {
    right += ch4;
    right_cnt++;
  }

  if (left_cnt > 0)
    left /= left_cnt;
  if (right_cnt > 0)
    right /= right_cnt;

  left *= (left_volume + 1) * 0.03125f;
  right *= (right_volume + 1) * 0.03125f;

  sample_buffer.push_back(left);
  sample_buffer.push_back(right);

  if (sample_buffer.size() >= sample_buffer_size * 2) {
    int queued = SDL_GetAudioStreamQueued(stream);
    while (queued > target_queue_bytes) {
      SDL_Delay(1);
      queued = SDL_GetAudioStreamQueued(stream);
    }

    SDL_PutAudioStreamData(stream, sample_buffer.data(),
                           sample_buffer.size() * sizeof(float));
    sample_buffer.clear();
  }
#endif
}

void APU::step(uint8_t m_cycles) {
  // Convert M-Cycles to T-Cycles
  uint32_t cycles = m_cycles * 4;

  frame_cycle_accumulator += cycles;

  while (frame_cycle_accumulator >= APUConstants::FRAME_SEQUENCER_PERIOD) {
    if (isMasterEnabled()) {
      updateFrameSequencer();
    } else {
      frame_sequencer_step = (frame_sequencer_step + 1) & 7;
    }
    frame_cycle_accumulator -= APUConstants::FRAME_SEQUENCER_PERIOD;
  }
  frame_sequencer_sub_counter = frame_cycle_accumulator;

  if (!isMasterEnabled())
    return;

  cycle_accumulator += cycles;
  tcycle_accumulator += cycles;

  while (tcycle_accumulator >= 4) {
    tcycle_accumulator -= 4;
    updateChannel1();
    updateChannel2();
    updateChannel3();
    updateChannel4();
  }

  constexpr float cycles_per_sample = 4194304.0f / 44100.0f;
  while (cycle_accumulator >= cycles_per_sample) {
    cycle_accumulator -= cycles_per_sample;
    mixAndOutput();
  }
}

uint8_t APU::readRegister(uint16_t addr) {
  if (addr >= 0xFF30 && addr <= 0xFF3F) {
    if (wave.enabled) {
      return 0xFF;
    }
    return wave_ram[addr - 0xFF30];
  }

  // NR52 Unused bits are always 1
  if (addr == 0xFF26) {
    uint8_t status = (registers[0x16] & 0x80) | 0x70;
    if (square1.enabled)
      status |= 0x01;
    if (square2.enabled)
      status |= 0x02;
    if (wave.enabled)
      status |= 0x04;
    if (noise.enabled)
      status |= 0x08;
    return status;
  }

  if (addr == 0xFF15 || addr == 0xFF1F || (addr >= 0xFF27 && addr <= 0xFF2F)) {
    return 0xFF;
  }

  if (addr >= 0xFF30 && addr <= 0xFF3F) {
    return wave.enabled ? wave_ram[wave.wave_position >> 1]
                        : wave_ram[addr - 0xFF30];
  }

  if (addr >= 0xFF10 && addr <= 0xFF25) {
    uint8_t value = registers[addr - 0xFF10];
    switch (addr) {
    case 0xFF10:
      return value | 0x80;
    case 0xFF11:
      return value | 0x3F;
    case 0xFF12:
      return value;
    case 0xFF13:
      return value | 0xFF;
    case 0xFF14:
      return value | 0xBF;
    case 0xFF16:
      return value | 0x3F;
    case 0xFF17:
      return value;
    case 0xFF18:
      return value | 0xFF;
    case 0xFF19:
      return value | 0xBF;
    case 0xFF1A:
      return value | 0x7F;
    case 0xFF1B:
      return value | 0xFF;
    case 0xFF1C:
      return value | 0x9F;
    case 0xFF1D:
      return value | 0xFF;
    case 0xFF1E:
      return value | 0xBF;
    case 0xFF20:
      return value | 0xFF;
    case 0xFF21:
      return value;
    case 0xFF22:
      return value;
    case 0xFF23:
      return value | 0xBF;
    case 0xFF24:
      return value;
    case 0xFF25:
      return value;
    default:
      return value;
    }
  }

  return 0xFF;
}

void APU::writeRegister(uint16_t addr, uint8_t value) {
  // NR52 (0xFF26) - Sound on/off
  if (addr == 0xFF26) {
    bool apu_enabled = (value & 0x80) != 0;
    bool was_enabled = (registers[0x16] & 0x80) != 0;
    registers[0x16] = value & 0x80;

    if (!apu_enabled) {
      for (int i = 0; i < 0x16; i++) {
        if (i != 0x10)
          registers[i] = 0;
      }
      square1.enabled = false;
      square1.dac = false;
      square1.period &= 0x700;
      square2.enabled = false;
      square2.dac = false;
      square2.period &= 0x700;
      wave.enabled = false;
      wave.dac = false;
      wave.period &= 0x700;
      noise.enabled = false;
      noise.dac = false;
    } else if (!was_enabled && apu_enabled) {
      frame_sequencer_step = 0;
      frame_cycle_accumulator =
          (frame_cycle_accumulator + APUConstants::FRAME_SEQUENCER_PERIOD) %
          APUConstants::FRAME_SEQUENCER_PERIOD;
      square1.sweep_counter = square1.sweep_pace ? square1.sweep_pace : 8;
    }

    return;
  }

  // Length counter registers (NR11, NR21, NR31, NR41) must be writable
  // even when APU is powered off (DMG hardware quirk)
  // Handle them before the power-off check to ensure length_counter fields
  // update

  if (addr == 0xFF11) { // NR11 - Square1 length
    registers[0x01] = value;
    square1.duty_cycle = (value & 0xC0) >> 6;
    uint8_t length_data = value & 0x3F;
    square1.length_counter = APUConstants::MAX_SQUARE_LENGTH - length_data;
    if (square1.length_frozen && square1.length_counter > 0) {
      square1.length_frozen = false;
    }
    return;
  }

  if (addr == 0xFF16) { // NR21 - Square2 length
    registers[0x06] = value;
    square2.duty_cycle = (value & 0xC0) >> 6;
    uint8_t length_data = value & 0x3F;
    square2.length_counter = APUConstants::MAX_SQUARE_LENGTH - length_data;
    if (square2.length_frozen && square2.length_counter > 0) {
      square2.length_frozen = false;
    }
    return;
  }

  if (addr == 0xFF1B) { // NR31 - Wave length
    registers[0x0B] = value;
    wave.length_counter = value ? (APUConstants::MAX_WAVE_LENGTH - value)
                                : APUConstants::MAX_WAVE_LENGTH;
    if (wave.length_frozen && wave.length_counter > 0) {
      wave.length_frozen = false;
    }
    return;
  }

  if (addr == 0xFF20) { // NR41 - Noise length
    registers[0x10] = value & 0x3F;
    uint8_t length_data = value & 0x3F;
    noise.length_counter = APUConstants::MAX_SQUARE_LENGTH - length_data;
    if (noise.length_frozen && noise.length_counter > 0) {
      noise.length_frozen = false;
    }
    return;
  }

  // Check for APU Power Off
  bool apu_enabled = (registers[0x16] & 0x80) != 0;
  if (!apu_enabled && addr != 0xFF26) {
    if (addr >= 0xFF30 && addr <= 0xFF3F) {
      wave_ram[addr - 0xFF30] = value;
      return;
    }

    // All other writes blocked when powered off
    return;
  }

  if (addr >= 0xFF30 && addr <= 0xFF3F) {
    if (wave.enabled) {
      return;
    }
    wave_ram[addr - 0xFF30] = value;
    return;
  }

  if (addr == 0xFF15 || addr == 0xFF1F || (addr >= 0xFF27 && addr <= 0xFF2F)) {
    return;
  }

  // Check for APU Power Off

  if (addr >= 0xFF10 && addr <= 0xFF25) {
    uint8_t reg_idx = addr - 0xFF10;

    switch (addr) {
    case 0xFF10:
      registers[reg_idx] = value & 0x7F;
      break;
    case 0xFF11:
    case 0xFF16:
      registers[reg_idx] = value;
      break;
    case 0xFF20:
      registers[reg_idx] = value & 0x3F;
      break;
    case 0xFF14:
    case 0xFF19:
    case 0xFF1E:
    case 0xFF23:
      registers[reg_idx] = value & 0xC7;
      break;
    case 0xFF1A:
      registers[reg_idx] = value & 0x80;
      break;
    case 0xFF1C:
      registers[reg_idx] = value & 0x60;
      break;
    default:
      registers[reg_idx] = value;
      break;
    }
  }

  switch (addr) {
  case 0xFF10: {
    bool old_direction = square1.sweep_direction;
    square1.sweep_pace = (value & 0x70) >> 4;
    square1.sweep_direction = (value & 0x08) != 0;
    square1.sweep_step = value & 0x07;
    if (old_direction && !square1.sweep_direction &&
        square1.sweep_negate_used) {
      square1.enabled = false;
    }
  } break;

  case 0xFF12:
    if (!(value & 0xF8)) {
      square1.dac = false;
      square1.enabled = false;
    } else {
      square1.dac = true;
    }
    square1.envelope_volume = (value & 0xF0) >> 4;
    square1.envelope_pace = value & 0x07;
    square1.envelope_direction = (value & 0x08) != 0;
    break;

  case 0xFF13:
    square1.period = (square1.period & 0x700) | value;
    break;

  case 0xFF14:
    square1.period = (square1.period & 0xFF) | ((value & 0x07) << 8);
    {
      bool old_length_enabled = square1.length_enabled;
      square1.length_enabled = (value & 0x40) != 0;
      if (!(value & 0x80) && !old_length_enabled && square1.length_enabled &&
          shouldClockLengthOnEnable()) {
        if (!square1.length_frozen && square1.length_counter > 0) {
          if (--square1.length_counter == 0) {
            square1.length_frozen = true;
            square1.enabled = false;
          }
        }
      }
    }
    if (value & 0x80)
      handleChannel1Trigger();
    break;

  case 0xFF17:
    if (!(value & 0xF8)) {
      square2.dac = false;
      square2.enabled = false;
    } else {
      square2.dac = true;
    }
    square2.envelope_volume = (value & 0xF0) >> 4;
    square2.envelope_pace = value & 0x07;
    square2.envelope_direction = (value & 0x08) != 0;
    break;

  case 0xFF18:
    square2.period = (square2.period & 0x700) | value;
    break;

  case 0xFF19:
    square2.period = (square2.period & 0xFF) | ((value & 0x07) << 8);
    {
      bool old_length_enabled = square2.length_enabled;
      square2.length_enabled = (value & 0x40) != 0;
      if (!old_length_enabled && square2.length_enabled &&
          shouldClockLengthOnEnable()) {
        if (!square2.length_frozen && square2.length_counter > 0) {
          if (--square2.length_counter == 0) {
            square2.length_frozen = true;
            square2.enabled = false;
          }
        }
      }
    }
    if (value & 0x80)
      handleChannel2Trigger();
    break;

  case 0xFF1A:
    wave.dac = (value & 0x80) != 0;
    if (!wave.dac)
      wave.enabled = false;
    break;

  case 0xFF1C:
    wave.volume_shift = (value & 0x60) >> 5;
    break;

  case 0xFF1D:
    wave.period = (wave.period & 0x700) | value;
    break;

  case 0xFF1E:
    wave.period = (wave.period & 0xFF) | ((value & 0x07) << 8);
    {
      bool old_length_enabled = wave.length_enabled;
      wave.length_enabled = (value & 0x40) != 0;
      if (!old_length_enabled && wave.length_enabled &&
          shouldClockLengthOnEnable()) {
        if (!wave.length_frozen && wave.length_counter > 0) {
          if (--wave.length_counter == 0) {
            wave.length_frozen = true;
            wave.enabled = false;
          }
        }
      }
    }
    if (value & 0x80)
      handleChannel3Trigger();
    break;

  case 0xFF20: {
    uint8_t length_data = value & 0x3F;
    noise.length_counter = 64 - length_data;
    if (noise.length_frozen && noise.length_counter > 0) {
      noise.length_frozen = false;
    }
  } break;

  case 0xFF21:
    if (!(value & 0xF8)) {
      noise.dac = false;
      noise.enabled = false;
    } else {
      noise.dac = true;
    }
    noise.envelope_volume = (value & 0xF0) >> 4;
    noise.envelope_pace = value & 0x07;
    noise.envelope_direction = (value & 0x08) != 0;
    break;

  case 0xFF22:
    noise.shift_amount = (value & 0xF0) >> 4;
    noise.lfsr_width = (value & 0x08) != 0;
    noise.divisor_code = value & 0x07;
    break;

  case 0xFF23: {
    bool old_length_enabled = noise.length_enabled;
    noise.length_enabled = (value & 0x40) != 0;
    if (!old_length_enabled && noise.length_enabled &&
        shouldClockLengthOnEnable()) {
      if (!noise.length_frozen && noise.length_counter > 0) {
        if (--noise.length_counter == 0) {
          noise.length_frozen = true;
          noise.enabled = false;
        }
      }
    }
  }
    if (value & 0x80)
      handleChannel4Trigger();
    break;

  case 0xFF24:
    left_volume = value & 0x0F;
    right_volume = (value & 0xF0) >> 4;
    break;

  case 0xFF25:
    square1.pan_left = (value & 0x01) != 0;
    square1.pan_right = (value & 0x02) != 0;
    square2.pan_left = (value & 0x04) != 0;
    square2.pan_right = (value & 0x08) != 0;
    wave.pan_left = (value & 0x10) != 0;
    wave.pan_right = (value & 0x20) != 0;
    noise.pan_left = (value & 0x40) != 0;
    noise.pan_right = (value & 0x80) != 0;
    break;
  }
}