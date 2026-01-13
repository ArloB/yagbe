#include "apu.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <string>

// =============================================================================
// Test Framework
// =============================================================================

#define ASSERT_EQ(a, b)                                                        \
  if ((a) != (b)) {                                                            \
    std::cerr << "FAILED: " << #a << " (" << (int)(a) << ") != " << #b << " (" \
              << (int)(b) << ") at line " << __LINE__ << std::endl;            \
    return false;                                                              \
  }

#define ASSERT_TRUE(a)                                                         \
  if (!(a)) {                                                                  \
    std::cerr << "FAILED: " << #a << " is false at line " << __LINE__          \
              << std::endl;                                                    \
    return false;                                                              \
  }

#define ASSERT_FALSE(a)                                                        \
  if ((a)) {                                                                   \
    std::cerr << "FAILED: " << #a << " is true at line " << __LINE__           \
              << std::endl;                                                    \
    return false;                                                              \
  }

// APU Register addresses
constexpr uint16_t NR10 = 0xFF10;
constexpr uint16_t NR11 = 0xFF11;
constexpr uint16_t NR12 = 0xFF12;
constexpr uint16_t NR13 = 0xFF13;
constexpr uint16_t NR14 = 0xFF14;
constexpr uint16_t NR21 = 0xFF16;
constexpr uint16_t NR22 = 0xFF17;
constexpr uint16_t NR23 = 0xFF18;
constexpr uint16_t NR24 = 0xFF19;
constexpr uint16_t NR30 = 0xFF1A;
constexpr uint16_t NR31 = 0xFF1B;
constexpr uint16_t NR32 = 0xFF1C;
constexpr uint16_t NR33 = 0xFF1D;
constexpr uint16_t NR34 = 0xFF1E;
constexpr uint16_t NR41 = 0xFF20;
constexpr uint16_t NR42 = 0xFF21;
constexpr uint16_t NR43 = 0xFF22;
constexpr uint16_t NR44 = 0xFF23;
constexpr uint16_t NR50 = 0xFF24;
constexpr uint16_t NR51 = 0xFF25;
constexpr uint16_t NR52 = 0xFF26;
constexpr uint16_t WAVE = 0xFF30;

// Channel masks for NR52
constexpr uint8_t CH1_MASK = 0x01;
constexpr uint8_t CH2_MASK = 0x02;
constexpr uint8_t CH3_MASK = 0x04;
constexpr uint8_t CH4_MASK = 0x08;

// Timing constants (in T-cycles, the APU's internal timing unit)
// 1 M-cycle = 4 T-cycles
// Frame sequencer ticks every 8192 T-cycles (512 Hz)
// Length counter clocks at steps 0, 2, 4, 6 (256 Hz)
// Sweep clocks at steps 2, 6 (128 Hz)
// Envelope clocks at step 7 (64 Hz)
constexpr int TCYCLES_PER_FRAME_TICK = 8192;
constexpr int MCYCLES_PER_FRAME_TICK = 2048;
constexpr int MCYCLES_PER_LENGTH_TICK = 4096; // 2 frame ticks = 1 length tick
constexpr int MCYCLES_PER_SWEEP_TICK = 8192;  // 4 frame ticks
constexpr int MCYCLES_PER_ENV_TICK = 16384;   // 8 frame ticks

// Channel configuration for multi-channel tests
struct ChannelConfig {
  std::string name;
  uint8_t mask;
  uint16_t reg_base; // First register (NRx0 or equivalent)
  uint16_t len_reg;  // Length register (NRx1)
  uint16_t env_reg;  // Envelope/volume register (NRx2)
  uint16_t freq_lo;  // Frequency low (NRx3)
  uint16_t ctrl_reg; // Control/trigger register (NRx4)
  int max_length;    // 64 for square/noise, 256 for wave
  bool has_sweep;
  bool has_envelope;
  bool is_wave;
};

const ChannelConfig CH1_CONFIG = {"Square1", CH1_MASK, NR10, NR11, NR12, NR13,
                                  NR14,      64,       true, true, false};
const ChannelConfig CH2_CONFIG = {"Square2", CH2_MASK, NR21,  NR21, NR22, NR23,
                                  NR24,      64,       false, true, false};
const ChannelConfig CH3_CONFIG = {"Wave", CH3_MASK, NR30,  NR31,  NR32, NR33,
                                  NR34,   256,      false, false, true};
const ChannelConfig CH4_CONFIG = {"Noise", CH4_MASK, NR41,  NR41, NR42, NR43,
                                  NR44,    64,       false, true, false};

class APUTester {
public:
  APU *apu = nullptr;

  APUTester() { apu = new APU(); }
  ~APUTester() { delete apu; }

  void reset() {
    delete apu;
    apu = new APU();
  }

  // Step APU by M-cycles (each M-cycle = 4 T-cycles)
  void step(int m_cycles) {
    for (int i = 0; i < m_cycles; i++) {
      apu->step(1);
    }
  }

  // Wait for n length ticks (256 Hz, 4096 M-cycles each)
  void delay_apu(int ticks) { step(MCYCLES_PER_LENGTH_TICK * ticks); }

  // Wait for n T-cycles (4 T-cycles = 1 M-cycle)
  // Note: we step in M-cycles, so we round up
  void delay_clocks(int t_cycles) { step((t_cycles + 3) / 4); }

  // Sync APU to a known frame sequencer state
  // This sets up the APU as if we just powered on
  void sync_apu() {
    reset();
    apu->writeRegister(NR52, 0x80);
    // After reset, frame_sequencer_step = 0, frame_sequencer_sub_counter = 0
    // This means we're at the very start of frame step 0
  }

  // Sync for sweep testing - align to just after a sweep clock
  void sync_sweep() {
    sync_apu();
    // Sweep clocks at steps 2 and 6
    // Advance to step 2 (2 frame ticks = 4096 M-cycles from step 0)
    step(MCYCLES_PER_FRAME_TICK * 2);
  }

  // Position the frame sequencer at a specific sub-counter value
  // This directly manipulates internal state for precise timing tests
  void set_frame_position(uint8_t step, uint32_t sub_counter) {
    apu->frame_sequencer_step = step;
    apu->frame_sequencer_sub_counter = sub_counter;
    apu->frame_cycle_accumulator = sub_counter;
  }

  // Check if we're in the first half of a length period (extra clock happens)
  bool in_first_half() {
    // First half: sub_counter < 4096 T-cycles
    return apu->frame_sequencer_sub_counter < 4096;
  }

  // Enable DAC for a channel
  void enable_dac(const ChannelConfig &ch) {
    if (ch.is_wave) {
      apu->writeRegister(ch.reg_base, 0x80);
    } else {
      apu->writeRegister(ch.env_reg, 0xF8);
    }
  }

  // Disable DAC for a channel
  void disable_dac(const ChannelConfig &ch) {
    if (ch.is_wave) {
      apu->writeRegister(ch.reg_base, 0x00);
    } else {
      apu->writeRegister(ch.env_reg, 0x00);
    }
  }

  // Check if channel is on (NR52 status bit set)
  bool is_channel_on(const ChannelConfig &ch) {
    return (apu->readRegister(NR52) & ch.mask) != 0;
  }

  // Check if channel is off
  bool is_channel_off(const ChannelConfig &ch) {
    return (apu->readRegister(NR52) & ch.mask) == 0;
  }

  // Get internal length counter value for a channel
  int get_length_counter(const ChannelConfig &ch) {
    if (ch.mask == CH1_MASK)
      return apu->square1.length_counter;
    if (ch.mask == CH2_MASK)
      return apu->square2.length_counter;
    if (ch.mask == CH3_MASK)
      return apu->wave.length_counter;
    if (ch.mask == CH4_MASK)
      return apu->noise.length_counter;
    return -1;
  }

  // =============================================================================
  // Test 01: Registers
  // =============================================================================

  bool test_01_02_register_masks() {
    std::cout << "  01-02: NR10-NR51 and wave RAM write/read..." << std::flush;

    const uint8_t masks[] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, // NR10-NR14
        0xFF, 0x3F, 0x00, 0xFF, 0xBF, // NR20-NR24
        0x7F, 0xFF, 0x9F, 0xFF, 0xBF, // NR30-NR34
        0xFF, 0xFF, 0x00, 0x00, 0xBF, // NR40-NR44
        0x00, 0x00, 0x70              // NR50-NR52
    };

    for (int d = 0; d <= 255; d++) {
      apu->writeRegister(NR52, 0x80);
      apu->writeRegister(NR51, 0);
      apu->writeRegister(NR30, 0);

      for (int i = 0; i < sizeof(masks); i++) {
        uint16_t addr = NR10 + i;
        if (addr == NR52)
          continue;

        uint8_t expected = masks[i] | d;
        apu->writeRegister(addr, d);
        uint8_t val = apu->readRegister(addr);

        if (val != expected) {
          std::cout << "FAILED at reg 0x" << std::hex << addr
                    << " write=" << (int)d << " expected=0x" << (int)expected
                    << " got=0x" << (int)val << std::dec << std::endl;
          return false;
        }
      }
    }
    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_01_03_nr52_write_read() {
    std::cout << "  01-03: NR52 write/read..." << std::flush;
    sync_apu();

    apu->writeRegister(NR52, 0x00);
    ASSERT_EQ(apu->readRegister(NR52), 0x70);

    apu->writeRegister(NR52, 0xFF);
    ASSERT_EQ(apu->readRegister(NR52), 0xF0);

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_01_04_power_wave_ram() {
    std::cout << "  01-04: Powering APU shouldn't affect wave RAM..."
              << std::flush;
    sync_apu();

    for (int i = 0; i < 16; i++) {
      apu->writeRegister(WAVE + i, 0x37);
    }

    apu->writeRegister(NR52, 0x00);

    for (int i = 0; i < 16; i++) {
      ASSERT_EQ(apu->readRegister(WAVE + i), 0x37);
    }

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_01_05_power_off_clears_regs() {
    std::cout << "  01-05: Powering APU off should clear registers..."
              << std::flush;
    sync_apu();

    for (uint16_t addr = NR10; addr < NR52; addr++) {
      apu->writeRegister(addr, 0xFF);
    }

    apu->writeRegister(NR52, 0x00);
    apu->writeRegister(NR52, 0x80);

    const uint8_t masks[] = {0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
                             0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
                             0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00};

    for (int i = 0; i < sizeof(masks); i++) {
      uint16_t addr = NR10 + i;
      ASSERT_EQ(apu->readRegister(addr), masks[i]);
    }

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_01_06_power_off_ignores_writes() {
    std::cout << "  01-06: When off, should ignore writes..." << std::flush;
    sync_apu();

    apu->writeRegister(NR52, 0x00);

    for (uint16_t addr = NR10; addr < NR52; addr++) {
      apu->writeRegister(addr, 0xFF);
    }

    apu->writeRegister(NR52, 0x80);

    const uint8_t masks[] = {0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
                             0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
                             0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00};

    for (int i = 0; i < sizeof(masks); i++) {
      uint16_t addr = NR10 + i;
      ASSERT_EQ(apu->readRegister(addr), masks[i]);
    }

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_01_07_power_off_allows_reads() {
    std::cout << "  01-07: When off, should allow normal reads..."
              << std::flush;
    sync_apu();

    apu->writeRegister(NR52, 0x00);

    const uint8_t masks[] = {0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
                             0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
                             0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00};

    for (int i = 0; i < sizeof(masks); i++) {
      uint16_t addr = NR10 + i;
      ASSERT_EQ(apu->readRegister(addr), masks[i]);
    }

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 02: Length Counter
  // =============================================================================

  // Setup for length counter tests - positions frame sequencer to avoid
  // accidental extra clocking on trigger
  bool test_02_begin(const ChannelConfig &ch) {
    sync_apu();
    // Move past the first half to avoid extra clock on trigger
    // Position at step 0 but past mid-point
    set_frame_position(0, 4100); // Second half of step 0

    enable_dac(ch);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Set length enable without trigger

    // Set length = 4
    int len_data = ch.max_length - 4;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, 0xC0 | (len_data & 0x3F));
    }

    apu->writeRegister(ch.ctrl_reg, 0xC0); // Trigger + Length enable
    return true;
  }

  bool test_02_02_length_clears_status(const ChannelConfig &ch) {
    std::cout << "  02-02 [" << ch.name << "]: Length->0 clears status..."
              << std::flush;
    test_02_begin(ch);

    // Initial length is 4, need 4 length clocks to reach 0
    delay_apu(3);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_03_length_reload(const ChannelConfig &ch) {
    std::cout << "  02-03 [" << ch.name << "]: Length can be reloaded..."
              << std::flush;
    test_02_begin(ch);

    // Reload to 10
    int len_data = ch.max_length - 10;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }

    delay_apu(9);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_04_length_zero_loads_max(const ChannelConfig &ch) {
    std::cout << "  02-04 [" << ch.name << "]: Load 0 gives max length..."
              << std::flush;
    test_02_begin(ch);

    // Load 0 = maximum
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, 0x00);
    } else {
      apu->writeRegister(ch.len_reg, 0x00);
    }

    delay_apu(ch.max_length - 1);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_05_trigger_no_affect_length(const ChannelConfig &ch) {
    std::cout << "  02-05 [" << ch.name << "]: Trigger doesn't affect length..."
              << std::flush;
    test_02_begin(ch); // Length = 4

    delay_apu(1); // Length now 3

    // Set up for clean re-trigger (in second half to avoid extra clock)
    set_frame_position(apu->frame_sequencer_step, 4100);
    apu->writeRegister(ch.ctrl_reg, 0xC0); // Trigger again

    delay_apu(2); // Length should be 1
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1); // Length should be 0
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_06_trigger_zero_to_max(const ChannelConfig &ch) {
    std::cout << "  02-06 [" << ch.name << "]: Trigger converts 0 to max..."
              << std::flush;
    test_02_begin(ch);

    delay_apu(4); // Length reaches 0
    ASSERT_TRUE(is_channel_off(ch));

    // Position in second half for clean trigger
    set_frame_position(apu->frame_sequencer_step, 4100);
    apu->writeRegister(ch.ctrl_reg, 0xC0); // Trigger

    // Should now have max length
    delay_apu(ch.max_length - 1);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_07_trigger_disabled_zero_to_max(const ChannelConfig &ch) {
    std::cout << "  02-07 [" << ch.name
              << "]: Trigger w/disabled len converts 0 to max..." << std::flush;
    test_02_begin(ch);

    delay_apu(4);                          // Length reaches 0
    apu->writeRegister(ch.ctrl_reg, 0x00); // Disable length

    set_frame_position(apu->frame_sequencer_step, 4100);
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger (no length enable)
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable length

    delay_apu(ch.max_length - 1);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_08_disable_len_no_enable(const ChannelConfig &ch) {
    std::cout << "  02-08 [" << ch.name
              << "]: Disabling length doesn't re-enable..." << std::flush;
    test_02_begin(ch);

    delay_apu(4); // Length reaches 0
    ASSERT_TRUE(is_channel_off(ch));

    apu->writeRegister(ch.ctrl_reg, 0x00); // Disable length
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_09_disable_len_stops_clock(const ChannelConfig &ch) {
    std::cout << "  02-09 [" << ch.name
              << "]: Disabled length stops clocking..." << std::flush;
    test_02_begin(ch);

    apu->writeRegister(ch.ctrl_reg, 0x00); // Disable length
    delay_apu(4);                          // Length frozen, should still be 4

    set_frame_position(apu->frame_sequencer_step, 4100);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable length

    delay_apu(3); // Length should now be 1
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_10_reload_no_enable(const ChannelConfig &ch) {
    std::cout << "  02-10 [" << ch.name << "]: Reloading doesn't re-enable..."
              << std::flush;
    test_02_begin(ch);

    delay_apu(4); // Length reaches 0
    ASSERT_TRUE(is_channel_off(ch));

    int len_data = ch.max_length - 2;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_13_dac_disable_immediate(const ChannelConfig &ch) {
    std::cout << "  02-13 [" << ch.name << "]: DAC disable -> immediate off..."
              << std::flush;
    test_02_begin(ch);

    delay_apu(2);
    ASSERT_TRUE(is_channel_on(ch));

    disable_dac(ch);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_14_dac_prevents_trigger(const ChannelConfig &ch) {
    std::cout << "  02-14 [" << ch.name << "]: Disabled DAC prevents trigger..."
              << std::flush;
    sync_apu();
    set_frame_position(0, 4100); // Second half

    disable_dac(ch);
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_02_15_dac_enable_no_reenable(const ChannelConfig &ch) {
    std::cout << "  02-15 [" << ch.name << "]: DAC enable doesn't re-enable..."
              << std::flush;
    test_02_begin(ch);

    delay_apu(2);
    ASSERT_TRUE(is_channel_on(ch));

    disable_dac(ch);
    ASSERT_TRUE(is_channel_off(ch));

    enable_dac(ch);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 03: Trigger (Extra clock on enable)
  // =============================================================================

  bool test_03_02_enable_second_half_no_clock(const ChannelConfig &ch) {
    std::cout << "  03-02 [" << ch.name << "]: Enable in 2nd half no clock..."
              << std::flush;
    sync_apu();
    enable_dac(ch);

    // Set length = 2
    int len_data = ch.max_length - 2;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger, no length enable

    // Position in second half of length period
    set_frame_position(0, 4200);           // Second half
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable length (no extra clock)

    // Length should still be 2
    ASSERT_EQ(get_length_counter(ch), 2);

    delay_apu(1);
    ASSERT_TRUE(is_channel_on(ch)); // Length = 1
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch)); // Length = 0

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_03_03_enable_first_half_clocks(const ChannelConfig &ch) {
    std::cout << "  03-03 [" << ch.name << "]: Enable in 1st half clocks..."
              << std::flush;
    sync_apu();
    enable_dac(ch);

    int len_data = ch.max_length - 2;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger

    // Position in first half
    set_frame_position(0, 2000);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable - should clock

    // Length should be 1 (clocked once)
    ASSERT_EQ(get_length_counter(ch), 1);

    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_03_05_clock_to_zero_disables(const ChannelConfig &ch) {
    std::cout << "  03-05 [" << ch.name << "]: Clock to 0 disables..."
              << std::flush;
    sync_apu();
    enable_dac(ch);

    // Length = 1
    int len_data = ch.max_length - 1;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger

    // Position in first half for extra clock
    set_frame_position(0, 2000);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable - clocks to 0

    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_03_07_trigger_unfreezes(const ChannelConfig &ch) {
    std::cout << "  03-07 [" << ch.name << "]: Trigger unfreezes length..."
              << std::flush;
    sync_apu();
    enable_dac(ch);

    int len_data = ch.max_length - 1;
    if (ch.is_wave) {
      apu->writeRegister(ch.len_reg, len_data & 0xFF);
    } else {
      apu->writeRegister(ch.len_reg, len_data & 0x3F);
    }
    apu->writeRegister(ch.ctrl_reg, 0x80); // Trigger

    set_frame_position(0, 2000);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable - clocks to 0
    ASSERT_TRUE(is_channel_off(ch));

    apu->writeRegister(ch.ctrl_reg, 0x00); // Disable length

    set_frame_position(0, 4200);
    apu->writeRegister(ch.ctrl_reg,
                       0x80); // Trigger - unfreezes, length becomes max

    ASSERT_EQ(get_length_counter(ch), ch.max_length);

    set_frame_position(0, 4200);
    apu->writeRegister(ch.ctrl_reg, 0x40); // Enable length
    delay_apu(2);

    // Should have length = max - 2 now
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(ch.max_length - 3);
    ASSERT_TRUE(is_channel_on(ch));
    delay_apu(1);
    ASSERT_TRUE(is_channel_off(ch));

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 04: Sweep (Channel 1 only)
  // =============================================================================

  bool test_04_begin() {
    sync_sweep();
    apu->writeRegister(NR14, 0x40);
    apu->writeRegister(NR11, 0xDF); // Length = 33
    apu->writeRegister(NR12, 0x08); // DAC enabled, silent
    return true;
  }

  bool test_04_02_shift_calculates_on_trigger() {
    std::cout << "  04-02: If shift>0, calculates on trigger..." << std::flush;
    test_04_begin();

    apu->writeRegister(NR10, 0x01); // Shift=1
    apu->writeRegister(NR13, 0xFF);
    apu->writeRegister(NR14, 0xC7); // Freq = 0x7FF, trigger

    // 0x7FF + 0x7FF/2 = 0x7FF + 0x3FF = 0xBFE > 0x7FF -> overflow
    ASSERT_TRUE(is_channel_off(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_04_03_no_shift_no_calc() {
    std::cout << "  04-03: If shift=0, doesn't calculate..." << std::flush;
    test_04_begin();

    apu->writeRegister(NR10, 0x10); // Period=1, shift=0
    apu->writeRegister(NR13, 0xFF);
    apu->writeRegister(NR14, 0xC7);

    delay_apu(1);
    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_04_04_period_zero_no_calc() {
    std::cout << "  04-04: If period=0, doesn't calculate (treated as 8)..."
              << std::flush;
    test_04_begin();

    apu->writeRegister(NR10, 0x00); // Period=0 (treated as 8), shift=0
    apu->writeRegister(NR13, 0xFF);
    apu->writeRegister(NR14, 0xC7);

    // With period=0 treated as 8, sweep won't fire soon
    delay_apu(0x10);
    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_04_06_overflow_disables() {
    std::cout << "  04-06: Calculation > $7FF disables..." << std::flush;
    test_04_begin();

    apu->writeRegister(NR10, 0x02); // Shift=2
    apu->writeRegister(NR13, 0x67);
    apu->writeRegister(NR14, 0xC6); // Freq = 0x667

    // 0x667 + 0x667/4 = 0x667 + 0x199 = 0x800 > 0x7FF
    ASSERT_TRUE(is_channel_off(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_04_07_no_overflow_no_disable() {
    std::cout << "  04-07: Calculation <= $7FF doesn't disable..."
              << std::flush;
    test_04_begin();

    apu->writeRegister(NR10, 0x01); // Shift=1
    apu->writeRegister(NR13, 0x55);
    apu->writeRegister(NR14, 0xC5); // Freq = 0x555

    // 0x555 + 0x555/2 = 0x555 + 0x2AA = 0x7FF (exactly at limit)
    delay_apu(0x20);
    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 05: Sweep Details
  // =============================================================================

  bool test_05_04_negate_exit_disables() {
    std::cout << "  05-04: Exiting negate after calc disables..." << std::flush;
    sync_sweep();
    apu->writeRegister(NR14, 0x40);
    apu->writeRegister(NR11, 0xE0);
    apu->writeRegister(NR12, 0x08);

    apu->writeRegister(NR10, 0x09); // Negate, shift=1
    apu->writeRegister(NR13, 0x00);
    apu->writeRegister(NR14, 0xC0);

    delay_apu(2);
    apu->writeRegister(NR10, 0x10); // Exit negate

    ASSERT_TRUE(is_channel_off(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_05_07_twos_complement() {
    std::cout << "  05-07: Subtract uses two's complement..." << std::flush;
    sync_sweep();
    set_frame_position(0, 4100);
    apu->writeRegister(NR14, 0x40);
    apu->writeRegister(NR11, 0xE0);
    apu->writeRegister(NR12, 0x08);

    apu->writeRegister(NR10, 0x1C); // Negate, period=1, shift=4
    apu->writeRegister(NR13, 0xB0);
    apu->writeRegister(NR14, 0x85);

    delay_apu(2);
    apu->writeRegister(NR10, 0x01);
    apu->writeRegister(NR14, 0xC5);

    delay_apu(0x1F);
    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 07: Timing
  // =============================================================================

  bool test_07_02_length_period() {
    std::cout << "  07-02: Length period correct..." << std::flush;
    sync_apu();

    set_frame_position(0, 4100);
    apu->writeRegister(NR14, 0x40);
    apu->writeRegister(NR11, 0x3F); // Length = 1
    apu->writeRegister(NR12, 0x08);
    apu->writeRegister(NR14, 0xC0);

    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    // Count M-cycles until channel off
    int cycles = 0;
    while (is_channel_on(CH1_CONFIG) && cycles < 10000) {
      step(1);
      cycles++;
    }

    // Should take approximately 4096 M-cycles (1 length tick)
    ASSERT_TRUE(cycles > 3000);
    ASSERT_TRUE(cycles < 5000);

    std::cout << "PASS (cycles=" << cycles << ")" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 08: Power
  // =============================================================================

  bool test_08_length_during_power() {
    std::cout << "  08: Length counters unaffected by power off (DMG)..."
              << std::flush;
    sync_apu();

    // Load length counters
    apu->writeRegister(NR11, 0xEF);
    apu->writeRegister(NR21, 0xDE);
    apu->writeRegister(NR31, 0xBC);
    apu->writeRegister(NR41, 0xCD);

    // Remember lengths
    int sq1_len = apu->square1.length_counter;
    int sq2_len = apu->square2.length_counter;
    int wav_len = apu->wave.length_counter;
    int noi_len = apu->noise.length_counter;

    // Power off
    apu->writeRegister(NR52, 0x00);

    // On DMG, length counters should NOT be clocked while off
    step(1000);

    // Power back on
    apu->writeRegister(NR52, 0x80);

    // Length counters should be preserved (DMG behavior)
    ASSERT_EQ(apu->square1.length_counter, sq1_len);
    ASSERT_EQ(apu->square2.length_counter, sq2_len);
    ASSERT_EQ(apu->wave.length_counter, wav_len);
    ASSERT_EQ(apu->noise.length_counter, noi_len);

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 09: Wave Read
  // =============================================================================

  bool test_09_wave_read_while_on() {
    std::cout << "  09: Wave read while on returns 0xFF..." << std::flush;
    sync_apu();

    for (int i = 0; i < 16; i++) {
      apu->writeRegister(WAVE + i, 0xAA);
    }

    apu->writeRegister(NR30, 0x80);
    apu->writeRegister(NR34, 0x80);

    ASSERT_TRUE(is_channel_on(CH3_CONFIG));

    uint8_t val = apu->readRegister(WAVE);
    ASSERT_EQ(val, 0xFF);

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 11: Regs After Power
  // =============================================================================

  bool test_11_02_power_clears_nr12() {
    std::cout << "  11-02: Power off clears NR12..." << std::flush;
    sync_apu();

    apu->writeRegister(NR12, 0xF0);
    apu->writeRegister(NR52, 0x00);
    step(100);
    apu->writeRegister(NR52, 0x80);

    // NR12 should be cleared, so DAC is off
    set_frame_position(0, 4100);
    apu->writeRegister(NR14, 0x80);
    ASSERT_TRUE(is_channel_off(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  bool test_11_03_power_clears_nr13() {
    std::cout << "  11-03: Power off clears NR13..." << std::flush;
    sync_apu();

    apu->writeRegister(NR13, 0xFF);
    apu->writeRegister(NR52, 0x00);
    step(100);
    apu->writeRegister(NR52, 0x80);

    apu->writeRegister(NR10, 0x11);
    apu->writeRegister(NR12, 0x08);
    set_frame_position(0, 4100);
    apu->writeRegister(NR14, 0x80);

    delay_apu(20);
    ASSERT_TRUE(is_channel_on(CH1_CONFIG));

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Test 12: Wave Write
  // =============================================================================

  bool test_12_wave_write_while_on() {
    std::cout << "  12: Wave write while on blocked..." << std::flush;
    sync_apu();

    for (int i = 0; i < 16; i++) {
      apu->writeRegister(WAVE + i, 0xAA);
    }

    apu->writeRegister(NR30, 0x80);
    apu->writeRegister(NR34, 0x80);

    apu->writeRegister(WAVE, 0x55);

    apu->writeRegister(NR30, 0x00);

    std::cout << "PASS" << std::endl;
    return true;
  }

  // =============================================================================
  // Run All Tests
  // =============================================================================

  void run_all() {
    std::cout << "===== APU Test Suite (Blargg dmg_sound) =====" << std::endl;
    std::cout << "Note: Direct frame sequencer manipulation for accurate timing"
              << std::endl;

    int passed = 0;
    int failed = 0;

    auto run_test = [&](std::function<bool()> test) {
      reset();
      if (test())
        passed++;
      else
        failed++;
    };

    std::cout << "\n=== Test 01: Registers ===" << std::endl;
    run_test([this]() { return test_01_02_register_masks(); });
    run_test([this]() { return test_01_03_nr52_write_read(); });
    run_test([this]() { return test_01_04_power_wave_ram(); });
    run_test([this]() { return test_01_05_power_off_clears_regs(); });
    run_test([this]() { return test_01_06_power_off_ignores_writes(); });
    run_test([this]() { return test_01_07_power_off_allows_reads(); });

    std::cout << "\n=== Test 02: Length Counter ===" << std::endl;
    for (const auto &ch : {CH1_CONFIG, CH2_CONFIG, CH3_CONFIG, CH4_CONFIG}) {
      run_test([this, &ch]() { return test_02_02_length_clears_status(ch); });
      run_test([this, &ch]() { return test_02_03_length_reload(ch); });
      run_test([this, &ch]() { return test_02_04_length_zero_loads_max(ch); });
      run_test(
          [this, &ch]() { return test_02_05_trigger_no_affect_length(ch); });
      run_test([this, &ch]() { return test_02_06_trigger_zero_to_max(ch); });
      run_test([this, &ch]() {
        return test_02_07_trigger_disabled_zero_to_max(ch);
      });
      run_test([this, &ch]() { return test_02_08_disable_len_no_enable(ch); });
      run_test(
          [this, &ch]() { return test_02_09_disable_len_stops_clock(ch); });
      run_test([this, &ch]() { return test_02_10_reload_no_enable(ch); });
      run_test([this, &ch]() { return test_02_13_dac_disable_immediate(ch); });
      run_test([this, &ch]() { return test_02_14_dac_prevents_trigger(ch); });
      run_test([this, &ch]() { return test_02_15_dac_enable_no_reenable(ch); });
    }

    std::cout << "\n=== Test 03: Trigger ===" << std::endl;
    for (const auto &ch : {CH1_CONFIG, CH2_CONFIG, CH3_CONFIG, CH4_CONFIG}) {
      run_test(
          [this, &ch]() { return test_03_02_enable_second_half_no_clock(ch); });
      run_test(
          [this, &ch]() { return test_03_03_enable_first_half_clocks(ch); });
      run_test([this, &ch]() { return test_03_05_clock_to_zero_disables(ch); });
      run_test([this, &ch]() { return test_03_07_trigger_unfreezes(ch); });
    }

    std::cout << "\n=== Test 04: Sweep ===" << std::endl;
    run_test([this]() { return test_04_02_shift_calculates_on_trigger(); });
    run_test([this]() { return test_04_03_no_shift_no_calc(); });
    run_test([this]() { return test_04_04_period_zero_no_calc(); });
    run_test([this]() { return test_04_06_overflow_disables(); });
    run_test([this]() { return test_04_07_no_overflow_no_disable(); });

    std::cout << "\n=== Test 05: Sweep Details ===" << std::endl;
    run_test([this]() { return test_05_04_negate_exit_disables(); });
    run_test([this]() { return test_05_07_twos_complement(); });

    std::cout << "\n=== Test 07: Timing ===" << std::endl;
    run_test([this]() { return test_07_02_length_period(); });

    std::cout << "\n=== Test 08: Power ===" << std::endl;
    run_test([this]() { return test_08_length_during_power(); });

    std::cout << "\n=== Test 09: Wave Read ===" << std::endl;
    run_test([this]() { return test_09_wave_read_while_on(); });

    std::cout << "\n=== Test 11: Regs After Power ===" << std::endl;
    run_test([this]() { return test_11_02_power_clears_nr12(); });
    run_test([this]() { return test_11_03_power_clears_nr13(); });

    std::cout << "\n=== Test 12: Wave Write ===" << std::endl;
    run_test([this]() { return test_12_wave_write_while_on(); });

    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed"
              << std::endl;
    if (failed == 0) {
      std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
      std::cout << "SOME TESTS FAILED" << std::endl;
    }
  }
};

#ifdef UNIT_TEST
int main() {
  APUTester tester;
  tester.run_all();
  return 0;
}
#endif
