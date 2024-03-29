//-----------------------------------------------------------------------------
// main.c
//-----------------------------------------------------------------------------
// Copyright 2014 Silicon Laboratories, Inc.
// http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
//
// Program Description:
//
// This program is a Template for firmware that uses Sleep or Suspend Mode
//
// Target:         EFM8SB1
// Tool chain:     Generic
// Command Line:   None
//
// Release 1.0 (BL)
//    - Initial Release
//    - 9 JAN 2015
//
//

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <si_toolchain.h>
#include <SI_EFM8SB1_Register_Enums.h>                  // SI_SFR declarations
#include "InitDevice.h"
#include "EFM8SB1_SMBus_Master_Multibyte.h"
//#include "smartclock.h"                // RTC Functionality
//#include "power.h"                     // Power Management Functionality

//-----------------------------------------------------------------------------
// Application-component specific constants and variables
//-----------------------------------------------------------------------------

// Stimulation parameters
// P0.7 - IREF
// Read from NFC:
uint8_t PW_HB;          // Pulse width high byte, chunks of 50 us, where 1 = 50 us, 2 = 100 us, 3 = 150 us
uint8_t PW_LB;          // Pulse width low byte
uint16_t PW;            // Pulse width 16-bit word
uint8_t T_HB;        // Pulse period timer setting high byte
uint8_t T_LB;        // Pulse period timer setting low byte
uint16_t T;          // Pulse period timer setting 16-bit word
uint8_t Iset;           // Set IREF current reference value, 0-63 (decimal) / 0 - 3F (hex)
uint8_t mode;           // Stimulation mode. 1 for singlechannel stimulation, 2 for multichannel scan (once), 3 for for multichannel scan (loop)
uint8_t channel_nr;     // Preset channel number for the single-channel stimulation
uint16_t T_on;           // Stimulation time on and off, seconds
uint16_t doubleT_on;
uint8_t T_on_HB;
uint8_t T_on_LB;
bool On;                // Voltage converter and 20V plane switch -> Switches stimulation circuit
bool isStim = 1;        // Stimulation on/off status stim on/off mode initiation
bool idle = 0; // idle bit
bool channel_set = 0;
bool telemetry_enabled = 0;
volatile uint8_t bigCounter;   // counter for Timer 2 to count timescales on the order of seconds
volatile uint16_t secondsPassed = 0; // counter for seconds and minutes

// Stimulation function prototypes
void Polarity(uint8_t);
void T0_Waitus (uint16_t);
void T2_Waitus(uint16_t);
void Pulse_On(void);
void Pulse_Off(void);
void Stim_Sequence(uint16_t, uint16_t);
void Stim_Off(void);
void mode_single_channel(void);
void mode_multichannel_scanning_nonloop(void);
void mode_multichannel_scanning_loop(void);

// I2C
// P0.0 - SMBus SDA
// P0.1 - SMBus SCL
uint8_t j;                                  // Counter variable
uint8_t SMB_DATA_OUT[NUM_BYTES_WR] = {0};   // Initialize I2C Write buffer, fill with 0
uint8_t SMB_DATA_IN[NUM_BYTES_RD] = {0};    // Initialize I2C Read buffer, fill with 0
uint8_t SAVE[16];                           // Saved Read array
uint8_t TARGET;                             // Target SMBus slave address
//define TARGET SLAVE_ADDR
volatile bool SMB_BUSY;                     // SMB status bit
volatile bool SMB_RW;                       // SMB status bit
uint16_t NUM_ERRORS;
SI_SBIT (SDA, SFR_P0, 0);                   // SMBus on P0.0
SI_SBIT (SCL, SFR_P0, 1);                   // and P0.1

// I2C function prototypes
void SMB_Write (void);
void SMB_Read (void);
void SDA_Reset(void);
void Write_Channel(uint8_t);

// LT8410, MUX36 shutdown pin
SI_SBIT (P05, SFR_P0, 5);                   // Pin 0.5 for SHDN (20V stage) enable/disable

// MUX36S16 - H-bridge multiplexer, pins 1.4 - 1.7
SI_SBIT (P17, SFR_P1, 7);                   // Pin 1.7 for MUX36S16 A0
SI_SBIT (P16, SFR_P1, 6);                   // Pin 1.6 for MUX36S16 A1
SI_SBIT (P15, SFR_P1, 5);                   // Pin 1.5 for MUX36S16 A2
SI_SBIT (P14, SFR_P1, 4);                   // Pin 1.4 for MUX36S16 A3
uint8_t mux36s16_state = 0;                 // MUX36S16 state byte
// MUX36S16 state function
void MUX36S16_output(uint8_t);

// MUX36D08 - multiplexer for output channels, pins 0.2 - 0.4
SI_SBIT (P02, SFR_P0, 2);                   // Pin 0.2 for MUX36D08 A0
SI_SBIT (P03, SFR_P0, 3);                   // Pin 0.3 for MUX36D08 A1
SI_SBIT (P04, SFR_P0, 4);                   // Pin 0.4 for MUX36D08 A2
uint8_t mux36d08_state;                     // MUX36D08 state byte
// MUX36D08 state function
void MUX36D08_output(uint8_t);

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{
  // Disable the watchdog here
}
 
//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
void main (void)
{
  T0_Waitus(1);                     // Wait 50 us for stability
  // SMBus reset mode in case of I2C comms SDA fault
  enter_smbus_reset_from_RESET ();
  while(!SDA){
      SDA_Reset();
  }
  // Initialize normal operation
  enter_DefaultMode_from_smbus_reset ();

  // Read data-stimulation parameters from NT3H via I2C
  TARGET = SLAVE_ADDR;         // NT3H slave address, 0xAA for NT3H
  SMB_Read();                  // Read first 16 bytes from the memory 0x01 into READ buffer
  for(j=0; j<16; j++){
      SAVE[j] = SMB_DATA_IN[j];   // Save read buffer into array
  }

  TCON |= TCON_TR1__STOP; // stop SMBus timer 1 to save energy

  PW_HB = SAVE[0]; // Pulse Width in chunks of 50 us high byte
  PW_LB = SAVE[1]; // Pulse Width in chunks of 50 us low byte
  T_HB = SAVE[2]; // Period high byte
  T_LB = SAVE[3]; // Period frequency low byte
  T_on_HB = SAVE[4]; // Pulse train period and channel switching time, seconds
  T_on_LB = SAVE[5];
  On = SAVE[7];
  Iset = SAVE[8];     // IREF current value, remember 0x3F is maximum, 0.5 mA reference current
  mode = SAVE[9];     // Stimulation mode. 1 for singlechannel stimulation, 2 for multichannel scan (once), 3 for for multichannel scan (loop)
  channel_nr = SAVE[10];
  telemetry_enabled = SAVE[11];

  // Set the device according to read values
  P05 = On;              // Enable or disable LT8410, enable MUX36D08 and 2x MUX36S16
  PW = (PW_HB<<8)|(PW_LB); // Combine PW into single hex
  T = (T_HB<<8)|(T_LB); // Combine pulse period into single hex
  T_on = (T_on_HB<<8)|(T_on_LB);
  doubleT_on = 2*T_on;
  // RTC setup
//  RTC0CN0_Local = 0xC0;                // Initialize Local Copy of RTC0CN0
//  RTC_WriteAlarm(WAKE_INTERVAL_TICKS);// Set the Alarm Value
//  RTC0CN0_SetBits(RTC0TR+RTC0AEN+ALRM);// Enable Counter, Alarm, and Auto-Reset
//
//  LPM_Init();                         // Initialize Power Management
//  LPM_Enable_Wakeup(RTC);


  TMR2CN0 |= TMR2CN0_TR2__RUN; // start timer 2 to measure the pulse frequency

  if (mode == 1) {
      MUX36S16_output(channel_nr); // initial setup of the channel
      if (telemetry_enabled) {
        Write_Channel(channel_nr); // write the channel
      }
      mode_single_channel();
  }
  else if (mode == 2) {
      mode_multichannel_scanning_nonloop();
  }
  else if (mode == 3) {
      mode_multichannel_scanning_loop();
  }
}

//----------------------------------
// Main Application States
//----------------------------------


void mode_single_channel(void) {
  while (1)
  {
    if (secondsPassed == 0 && isStim && !channel_set) {
        P05 = On;
        channel_set = 1;
    }
    if (isStim) {
        Stim_Sequence(PW, T);
    }
    if (isStim == 0 && idle == 1) {
        Stim_Off();
        idle = 0;
        P05 = 0; // switch off 20V stuff to reduce the energy
    }
  }
}

void mode_multichannel_scanning_nonloop(void) {
  while (1)
  {
    if (secondsPassed == 0 && isStim && !channel_set) {

      if (channel_nr >= 15) { // Prevent the channel from going beyond defined state and stop stimulation at the last one
          TMR2CN0 &= ~(TMR2CN0_TR2__BMASK); // stop timer 2
          secondsPassed = 0; // reset the seconds count
          bigCounter = 0; // reset overflows counter
          P05 = 0; // Switch off periphery
          while (1); // loop until MCU reset
      }
      MUX36S16_output(channel_nr); channel_set = 1; // set the channel, raise channel set bit to prevent multiple if states and step in this if branch once
      if (telemetry_enabled) {
        Write_Channel(channel_nr); // write the channel number if the setting is enabled by user
      }
      channel_nr ++; // once channel is set and transmitted via NFC, prepare the next channel
      P05 = On;       // "awaken" the periphery. This should be done once per !channel_set if branch
    } // end channel setting and periphery awakening "if" branch

    if (isStim) { // Stimulation required. Multiple step-ins - every time when isStim is "1".
        Stim_Sequence(PW, T); // Stimulation sequence with set PW and PF
    }
    else if (isStim == 0 && idle == 1) { // Once T_on (pulse train time) reached, isStim state is flipped. If isStim was 0, then idle is 1. Enter silent state
        Stim_Off();
        idle = 0; // silence bit
        P05 = 0; // switch off 20V stuff to reduce the energy
    }
  }
}

void mode_multichannel_scanning_loop(void) {
  while (1)
  {
      if (secondsPassed == 0 && isStim && !channel_set) {

        if (channel_nr >= 15) {
            channel_nr = 0;
        }
        MUX36S16_output(channel_nr); channel_set = 1;
        if (telemetry_enabled) {
          Write_Channel(channel_nr); // write the channel
        }
        channel_nr ++;
        P05 = On;
      }

      if (isStim) {
          Stim_Sequence(PW, T);
      }
      else if (isStim == 0 && idle == 1) {
          Stim_Off();
          idle = 0;
          P05 = 0; // switch off 20V stuff to reduce the energy
      }
    }
}

void Stim_Sequence(uint16_t PW, uint16_t T) {
  // disable interrupts?
  // IE_EA = 0;
  Polarity(0); // start shunted
  Polarity(1);// Forward polarity
  Pulse_On();
  //T0_Waitus(PW);
  T0_Waitus(PW);
  // (-) phase for next PW * 50 us
  Pulse_Off();
  Polarity(0);
  Polarity(2);
  Pulse_On();
  T0_Waitus(PW);
  Pulse_Off();
  Polarity(0);

  T0_Waitus(T);
  // enable interrupts?
  //IE_EA = 1;
}

void Stim_Off(void) {
  Polarity(0); // shunt
  Pulse_Off(); // current off
}


// Function definitions

/*
 * Function: Polarity
 * --------------------
 * Sets MUX36D08 switch polarity.
 */
void Polarity(uint8_t polar) {
  switch (polar) {
  case 1: // Forward polarity
    MUX36D08_output(0x01);
    break;
  case 2: // Reversed polarity
    MUX36D08_output(0x02);
    break;
  case 0: // Shunted
    MUX36D08_output(0x00);
    break;
  }
}

/*
 * Function: Pulse_On
 * --------------------
 * Activates current reference at P0.7.
 * Reads amplitude bit from SMBus as Iset.
 */
void Pulse_On(void){
  IREF0CN0 = IREF0CN0_SINK__DISABLED | IREF0CN0_MDSEL__HIGH_CURRENT
                        | (Iset << IREF0CN0_IREF0DAT__SHIFT);
}

void Pulse_Off(void){
  IREF0CN0 = IREF0CN0_SINK__DISABLED | IREF0CN0_MDSEL__HIGH_CURRENT
                            | (0x00 << IREF0CN0_IREF0DAT__SHIFT); // Current 0 mA
}

/* Function: T0_Waitus
 * --------------------
 * Wait function based on timer 0 overflow.
 * Overflows every 50 us. Does not generate interrupt.
 * Commented lines are original overflow bits and some of my calculations.
 */
void T0_Waitus (uint16_t us)
{
   TCON &= ~0x30;                      // Stop Timer0; Clear TCON_TF0
   TMOD &= ~0x0f;                      // 16-bit free run mode
   TMOD |=  0x01;

   CKCON0 |= 0x04;                      // Timer0 counts SYSCLKs

   while (us) {
      TCON_TR0 = 0;                         // Stop Timer0
      //TH0 = ((-SYSCLK/1000) >> 8);     // Overflow in 1ms
      // Overflow in 0xFC18 (64536) cycles, which for the 20 MHz is 50 us.
      //TH0 = (0xFF << TH0_TH0__SHIFT);


      TH0 = (0xFE << TH0_TH0__SHIFT); // 0xFD
      TL0 = (0x0B << TL0_TL0__SHIFT); // 0xF2

      //TL0 = ((-SYSCLK/1000) & 0xFF);
      //TL0 = (0xE1 << TL0_TL0__SHIFT);
      TCON_TF0 = 0;                         // Clear overflow indicator
      TCON_TR0 = 1;                         // Start Timer0
      while (!TCON_TF0);                    // Wait for overflow
      us--;                            // Update us counter
   }

   TCON_TR0 = 0;                            // Stop Timer0
}

void T2_Waitus (uint16_t us) {
  // Save Timer Configuration
//  uint8_t TMR2CN0_TR2_save;
//  TMR2CN0_TR2_save = TMR2CN0 & TMR2CN0_TR2__BMASK;
//
  uint8_t TMR2CN0_TR2_save;
  TMR2CN0_TR2_save = TMR2CN0 & TMR2CN0_TR2__BMASK;

  while (us) {
      // Stop Timer
      TMR2CN0 &= ~(TMR2CN0_TR2__BMASK);
      TMR2RLH = (0xFF << TMR2RLH_TMR2RLH__SHIFT); // Reload high byte 0xFF
      TMR2RLL = (0x83 << TMR2RLL_TMR2RLL__SHIFT); // Reload low byte 0xF8
      TMR2CN0 |= TMR2CN0_TR2__RUN; // start timer 2
      //while ((!TMR2CN0_TF2L) || (!TMR2CN0_TF2H));                    // Wait for overflow (low byte)
      while (!TMR2CN0_TF2H);
      us--;                            // Update us counter
  }
  TMR2CN0 &= ~(TMR2CN0_TR2__BMASK); // stop timer 2
}

// Function: SDA_Reset
// * --------------------
// * Reset I2C if the SDA line busy.
//
void SDA_Reset(void)
{
    uint8_t j;                    // Dummy variable counters
    // Provide clock pulses to allow the slave to advance out
    // of its current state. This will allow it to release SDA.
    XBR1 = 0x40;                     // Enable Crossbar
    SCL = 0;                         // Drive the clock low
    for(j = 0; j < 255; j++);        // Hold the clock low
    SCL = 1;                         // Release the clock
    while(!SCL);                     // Wait for open-drain
                     // clock output to rise
    for(j = 0; j < 10; j++);         // Hold the clock high
    XBR1 = 0x00;                     // Disable Crossbar
}

void SMB_Write (void)
{
  while (SMB_BUSY);                   // Wait for SMBus to be free.
   SMB_BUSY = 1;                       // Claim SMBus (set to busy)
   SMB_RW = 0;                         // Mark this transfer as a WRITE
   SMB0CN0_STA = 1;                            // Start transfer

   while (SMB_BUSY);
}

void SMB_Read (void)
{
  while (SMB_BUSY);               // Wait for bus to be free.
   SMB_BUSY = 1;                       // Claim SMBus (set to busy)
   SMB_RW = 1;                         // Mark this transfer as a READ
   SMB0CN0_STA = 1;                            // Start transfer

   while (SMB_BUSY);               // Wait for transfer to complete
}

/*
 * Function: MUX36S16_output
 * -------------------------------
 * MUX36D08 output selection function
 * Selects output channel of the stimulation.
 *
 * mux36s16_state:switch state read from NT3H.
 * Switch state converted to corresponding bits that are set as control pins for MUX36
 * as per MUX36 truth table.
 * MUX36S16 truth table:
 * EN A3 A2 A1 A0 ON-CHANNEL
 * 0  X  X  X  X  All channels are off
 * 1 0 0 0 0 Channel 1
 * 1 0 0 0 1 Channel 2
 * 1 0 0 1 0 Channel 3
 * 1 0 0 1 1 Channel 4
 * 1 0 1 0 0 Channel 5
 * 1 0 1 0 1 Channel 6
 * 1 0 1 1 0 Channel 7
 * 1 0 1 1 1 Channel 8
 * 1 1 0 0 0 Channel 9
 * 1 1 0 0 1 Channel 10
 * 1 1 0 1 0 Channel 11
 * 1 1 0 1 1 Channel 12
 * 1 1 1 0 0 Channel 13
 * 1 1 1 0 1 Channel 14
 * 1 1 1 1 0 Channel 15
 * 1 1 1 1 1 Channel 16
 */
void MUX36S16_output(uint8_t mux36s16_state){
  //IE_EA = 0;
  /*
   * Because channel 14 for the output MUX36S16 is skipped,
   * channel numbers are shifted by one for the channel 14 (decimal code 13, hex D) and 15
   * (decimal code 14, hex E). Calibrate that such that user can input standard channels
   * according to the channel cuff enumeration.
   */

  if (mux36s16_state >= 13){
      mux36s16_state = mux36s16_state + 1;
  }
  P17 = (mux36s16_state & (1 << (1-1))) ? 1 : 0; // Get 1st bit of MUX36S16 state byte
  P16 = (mux36s16_state & (1 << (2-1))) ? 1 : 0; // Get 2nd bit of the state byte
  P15 = (mux36s16_state & (1 << (3-1))) ? 1 : 0; // Get 3rd bit of the state byte
  P14 = (mux36s16_state & (1 << (4-1))) ? 1 : 0; // Get 4th bit of the state byte
  //IE_EA = 1;
}

/*
 * Function: MUX36D08_output
 * -------------------------------
 * MUX36D08 output selection function
 * Selects output channel of the stimulation.
 *
 * mux36d08_state:switch state read from NT3H.
 * Switch state converted to corresponding bits that are set as control pins for MUX36
 * as per MUX36 truth table.
 * MUX36D08 truth table:
 * EN A2  A1  A0  ON-CHANNEL
 * 0  X   X   X   All channels are off
 * 1  0   0   0   Channels 1A and 1B
 * 1  0   0   1   Channels 2A and 2B
 * 1  0   1   0   Channels 3A and 3B
 * 1  0   1   1   Channels 4A and 4B
 * 1  1   0   0   Channels 5A and 5B
 * 1  1   0   1   Channels 6A and 6B
 * 1  1   1   0   Channels 7A and 7B
 * 1  1   1   1   Channels 8A and 8B
 */
void MUX36D08_output(uint8_t mux36d08_state){
  //IE_EA = 0;
  P02 = (mux36d08_state & (1 << (1-1))) ? 1 : 0; // Get 1st bit of MUX36D08 state byte
  P03 = (mux36d08_state & (1 << (2-1))) ? 1 : 0; // Get 2nd bit of the state byte
  P04 = (mux36d08_state & (1 << (3-1))) ? 1 : 0; // Get 3rd bit of the state byte
  //IE_EA = 1;
}

/*
 * Function: Write_Channel
 * -----------------------
 * For the multichannel stimulation writes the current channel number to NT3H
 * (Address 8 on the NFC Tools GUI)
 */
void Write_Channel(uint8_t channel_to_write)
{
  TCON |= TCON_TR1__RUN; // start timer 1 for SMBus clock
  SMB_DATA_OUT[0] = channel_to_write;
  TARGET = SLAVE_ADDR;
  SMB_Write();                     // Initiate SMBus write
  TCON |= TCON_TR1__STOP; // stop timer 1 to save energy
}
