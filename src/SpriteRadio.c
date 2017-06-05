/*
  SpriteRadio.cpp - An Energia library for transmitting data using the CC430 series of devices
  
  by Zac Manchester

*/

#include <stdint.h>
#include <stdbool.h>

#include "SpriteRadio.h"
#include "CC430Radio.h"
#include "cc430f5137.h"
#include "random.h"
#include "prn.h"

	CC1101Settings m_settings;
	char m_power;
	unsigned char *m_prn0;
	unsigned char *m_prn1;

/**
 * System clock
 */
#define SYSTEM_CLK_FREQ 8000000L
#ifndef F_CPU
#define F_CPU  SYSTEM_CLK_FREQ
#endif

#define clockCyclesPerMicrosecond() ( F_CPU / 1000000L )
#define clockCyclesToMicroseconds(a) ( (a) / clockCyclesPerMicrosecond() )
#define microsecondsToClockCycles(a) ( (a) * clockCyclesPerMicrosecond() )


// the clock source is set so that watch dog timer (WDT) ticks every clock
// cycle (F_CPU), and the watch dog timer ISR is called every 512 ticks
// for F_CPU < 8MHz and every 8192 ticks for F_CPU > 8MHz.
#if F_CPU < 8000000L
#define TICKS_PER_WDT_OVERFLOW 512
#else
#define TICKS_PER_WDT_OVERFLOW 8192
#endif

// the whole number of microseconds per WDT overflow
#define MICROSECONDS_PER_WDT_OVERFLOW (clockCyclesToMicroseconds(TICKS_PER_WDT_OVERFLOW))

// the whole number of milliseconds per WDT overflow
#define MILLIS_INC (MICROSECONDS_PER_WDT_OVERFLOW / 1000)

// the fractional number of milliseconds per WDT overflow. 
#define FRACT_INC (MICROSECONDS_PER_WDT_OVERFLOW % 1000)
#define FRACT_MAX 1000

// Increments when sleeping. Depends on ACLK source.
uint16_t SMILLIS_INC;
uint16_t SFRACT_INC;

volatile unsigned long wdt_overflow_count = 0;
volatile unsigned long wdt_millis = 0;
volatile unsigned int wdt_fract = 0;
volatile uint8_t sleeping = false;
volatile uint16_t vlo_freq = 0;


void enableWatchDogIntervalMode(void)
{
	/* WDT Password + WDT interval mode + Watchdog clock source /512 for F_CPU < 8MHz
	 * and /8192 for F_CPU > 8MHz + source from SMCLK.
	 * Note that we WDT is running in interval mode. WDT will not trigger a reset on expire in this mode. */
#if F_CPU < 8000000L
	WDTCTL = WDTPW | WDTTMSEL | WDTCNTCL | WDT_MDLY_0_5;
#else
	WDTCTL = WDTPW | WDTTMSEL | WDTCNTCL | WDT_MDLY_8;
#endif

	/* WDT interrupt enable */
#ifdef __MSP430_HAS_SFR__
	SFRIE1 |= WDTIE;
#else
	IE1 |= WDTIE;
#endif	
}

unsigned long micros()
{
	unsigned long m;

	// disable interrupts to ensure consistent readings
	// safe SREG to avoid issues if interrupts were already disabled
	bool int_state = _get_interrupt_state();
	__dint();

	m = wdt_overflow_count;

;	// safe to enable interrupts again
    if (int_state)
      __eint();

	// MSP430 does not give read access to current WDT, so we
	// have to approximate microseconds from overflows and
	// fractional milliseconds.
	// With an WDT interval of SMCLK/512, precision is +/- 256/SMCLK,
	// for example +/-256us @1MHz and +/-16us @16MHz

	return (m * MICROSECONDS_PER_WDT_OVERFLOW);
}

__attribute__((interrupt(WDT_VECTOR)))
void watchdog_isr (void)
{
  // copy these to local variables so they can be stored in registers
  // (volatile variables must be read from memory on every access)
  unsigned long m = wdt_millis;
  unsigned int f = wdt_fract;

  m += sleeping ? SMILLIS_INC:MILLIS_INC;
  f += sleeping ? SFRACT_INC:FRACT_INC;
  if (f >= FRACT_MAX) {
    f -= FRACT_MAX;
    m += 1;
  }

  wdt_fract = f;
  wdt_millis = m;
  wdt_overflow_count++;

  /* Exit from LMP3 on reti (this includes LMP0) */
  __bic_SR_register_on_exit(LPM3_bits);
}

/* (ab)use the WDT */
void delay(uint32_t milliseconds)
{
	uint32_t start = micros();
	while(milliseconds > 0) {
		if ((micros() - start) >= 1000) {
			milliseconds--;
			start += 1000;
		}
		__bis_SR_register(LPM0_bits+GIE);
	}
}

static void randomSeed(unsigned int seed)
{
  if (seed != 0) {
    srandom(seed);
  }
}

// From wiring.h
inline void delayMicroseconds(const uint16_t us)
{
  const uint16_t cyclesPerMicro = SYSTEM_CLK_FREQ/1000000L;
  __delay_cycles((us * cyclesPerMicro));
}

void SpriteRadio_SpriteRadio() {
	
	m_power = 0xC3;
	
	m_settings = (CC1101Settings){
	    0x0E,   // FSCTRL1
		0x00,   // FSCTRL0
		0x10,   // FREQ2
		0xD1,   // FREQ1
		0x21,   // FREQ0
		0x0B,   // MDMCFG4
		0x43,   // MDMCFG3
		0x70,   // MDMCFG2
		0x02,   // MDMCFG1
		0xF8,   // MDMCFG0
		0x00,   // CHANNR
		0x07,   // DEVIATN
		0xB6,   // FREND1
		0x10,   // FREND0
		0x18,   // MCSM0
		0x1D,   // FOCCFG
		0x1C,   // BSCFG
		0xC7,   // AGCCTRL2
		0x00,   // AGCCTRL1
		0xB0,   // AGCCTRL0
		0xEA,   // FSCAL3
		0x2A,   // FSCAL2
		0x00,   // FSCAL1
		0x1F,   // FSCAL0
		0x59,   // FSTEST
		0x88,   // TEST2
		0x31,   // TEST1
		0x09,   // TEST0
		0x07,   // FIFOTHR
		0x29,   // IOCFG2
		0x06,   // IOCFG0
		0x00,   // PKTCTRL1  Packet Automation (0x04 = append status bytes)
		0x02,   // PKTCTRL0  0x02 = infinite packet length, 0x00 = Fixed Packet Size, 0x40 = whitening, 0x20 = PN9
		0x00,   // ADDR      Device address.
		0xFF    // PKTLEN    Packet Length (Bytes)
	};

	m_prn0 = PRN_0;
	m_prn1 = PRN_1;

	//Initialize random number generator
	randomSeed(((int)m_prn0[0]) + ((int)m_prn1[0]) + ((int)m_prn0[1]) + ((int)m_prn1[1]));

    enableWatchDogIntervalMode();
}

#if 0
void SpriteRadio_SpriteRadio(unsigned char prn0[], unsigned char prn1[], CC1101Settings settings) {
	
	m_power = 0xC0;
	m_settings = settings;
	
	m_prn0 = prn0;
	m_prn1 = prn1;

	//Initialize random number generator
	randomSeed(((int)m_prn0[0]) + ((int)m_prn1[0]) + ((int)m_prn0[1]) + ((int)m_prn1[1]));
}
#endif

// Set the output power of the transmitter.
void SpriteRadio_setPower(int tx_power_dbm) {
	
	// These values are from TI Design Note DN013 and are calibrated for operation at 434 MHz.
	switch (tx_power_dbm) {
		case 10:
			m_power = 0xC0;
			break;
		case 9:
			m_power = 0xC3;
			break;
		case 8:
			m_power = 0xC6;
			break;
		case 7:
			m_power = 0xC9;
			break;
		case 6:
			m_power = 0x82;
			break;
		case 5:
			m_power = 0x84;
			break;
		case 4:
			m_power = 0x87;
			break;
		case 3:
			m_power = 0x8A;
			break;
		case 2:
			m_power = 0x8C;
			break;
		case 1:
			m_power = 0x50;
			break;
		case 0:
			m_power = 0x60;
			break;
		case -1:
			m_power = 0x52;
			break;
		case -2:
			m_power = 0x63;
			break;
		case -3:
			m_power = 0x65;
			break;
		case -4:
			m_power = 0x57;
			break;
		case -5:
			m_power = 0x69;
			break;
		case -6:
			m_power = 0x6A;
			break;
		case -7:
			m_power = 0x6C;
			break;
		case -8:
			m_power = 0x6D;
			break;
		case -9:
			m_power = 0x6E;
			break;
		case -10:
			m_power = 0x34;
			break;
		case -11:
			m_power = 0x25;
			break;
		case -12:
			m_power = 0x26;
			break;
		case -13:
		case -14:
		case -15:
			m_power = 0x1D;
			break;
		case -16:
		case -17:
		case -18:
			m_power = 0x1A;
			break;
		case -19:
		case -20:
			m_power = 0x0E;
			break;
		case -21:
		case -22:
		case -23:
			m_power = 0x0A;
			break;
		case -24:
		case -25:
		case -26:
			m_power = 0x07;
			break;
		case -27:
		case -28:
		case -29:	
		case -30:
			m_power = 0x03;
			break;
		default:
			m_power = 0xC3; // 10 dBm
		}
}

char SpriteRadio_fecEncode(char data)
{
  	//Calculate parity bits using a (16,8,5) block code
  	//given by the following generator matrix:
	/*unsigned char G[8][16] = {
  		{1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
  		{0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
  		{1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0},
  		{0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0},
  		{0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0},
  		{1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0},
  		{0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
  		{1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1}};*/

  	char p = 0;
  	p |= (((data&BIT7)>>7)^((data&BIT5)>>5)^((data&BIT2)>>2)^(data&BIT0))<<7;
  	p |= (((data&BIT6)>>6)^((data&BIT5)>>5)^((data&BIT4)>>4)^((data&BIT2)>>2)^((data&BIT1)>>1)^(data&BIT0))<<6;
  	p |= (((data&BIT4)>>4)^((data&BIT3)>>3)^((data&BIT2)>>2)^((data&BIT1)>>1))<<5;
  	p |= (((data&BIT7)>>7)^((data&BIT3)>>3)^((data&BIT2)>>2)^((data&BIT1)>>1)^(data&BIT0))<<4;
  	p |= (((data&BIT7)>>7)^((data&BIT6)>>6)^((data&BIT5)>>5)^((data&BIT1)>>1))<<3;
  	p |= (((data&BIT7)>>7)^((data&BIT6)>>6)^((data&BIT5)>>5)^((data&BIT4)>>4)^(data&BIT0))<<2;
  	p |= (((data&BIT7)>>7)^((data&BIT6)>>6)^((data&BIT4)>>4)^((data&BIT3)>>3)^((data&BIT2)>>2)^(data&BIT0))<<1;
  	p |= (((data&BIT5)>>5)^((data&BIT4)>>4)^((data&BIT3)>>3)^(data&BIT0));

  	return p;
}

void SpriteRadio_transmit(char bytes[], unsigned int length)
{
#ifdef SR_DEBUG_MODE

	for(unsigned int k = 0; k < length; ++k)
	{
		SpriteRadio_transmitByte(bytes[k]);
		delay(1000);
	}

#else

	delay(random(0, 2000));

	for(unsigned int k = 0; k < length; ++k)
	{
		SpriteRadio_transmitByte(bytes[k]);

		delay(random(8000, 12000));
	}

#endif
}

void SpriteRadio_transmitByte(char byte)
{
	char parity = SpriteRadio_fecEncode(byte);

	//Transmit preamble (1110010)
	beginRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);

	//Transmit parity byte
	parity & BIT7 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT6 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT5 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT4 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT3 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT2 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT1 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	parity & BIT0 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	
	//Transmit data byte
	byte & BIT7 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT6 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT5 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT4 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT3 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT2 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT1 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	byte & BIT0 ? continueRawTransmit(m_prn1,PRN_LENGTH_BYTES) : continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);

	//Transmit postamble (1011000)
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn1,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);
	continueRawTransmit(m_prn0,PRN_LENGTH_BYTES);

	endRawTransmit();
}

void SpriteRadio_rawTransmit(unsigned char bytes[], unsigned int length) {
	
	beginRawTransmit(bytes, length);
	endRawTransmit();
}

void beginRawTransmit(unsigned char bytes[], unsigned int length) {
	char status;

	//Wait for radio to be in idle state
	status = strobe(RF_SIDLE);
	while (status & 0xF0)
	{
		status = strobe(RF_SNOP);
	}
	
	//Clear TX FIFO
	status = strobe(RF_SFTX);

	if(length <= 64)
	{
		writeTXBuffer(bytes, length); //Write bytes to transmit buffer
		status = strobe(RF_STX);  //Turn on transmitter
	}
	else
	{
		unsigned char bytes_free, bytes_to_write;
	  	unsigned int bytes_to_go, counter;
		
		writeTXBuffer(bytes, 64); //Write first 64 bytes to transmit buffer
		bytes_to_go = length - 64;
		counter = 64;

		status = strobe(RF_STX);  //Turn on transmitter

		//Wait for oscillator to stabilize
		while (status & 0xC0)
		{
			status = strobe(RF_SNOP);
		}

		while(bytes_to_go)
		{
			delayMicroseconds(1000); //Wait for some bytes to be transmitted

			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

			writeTXBuffer(bytes+counter, bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}
}

void continueRawTransmit(unsigned char bytes[], unsigned int length) {

	unsigned char bytes_free, bytes_to_write;
	unsigned int bytes_to_go, counter;
		
	bytes_to_go = length;
	counter = 0;

	if(bytes)
	{
		while(bytes_to_go)
		{
			delayMicroseconds(1000); //Wait for some bytes to be transmitted

			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

			writeTXBuffer(bytes+counter, bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}
	else
	{
		while(bytes_to_go)
		{
			delayMicroseconds(1000); //Wait for some bytes to be transmitted

			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

			writeTXBufferZeros(bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}

	return;
}

void endRawTransmit() {

	char status = strobe(RF_SNOP);

	//Wait for transmission to finish
	while(status != 0x7F)
	{
		status = strobe(RF_SNOP);
	}
	strobe(RF_SIDLE); //Put radio back in idle mode
	return;
}

void SpriteRadio_txInit() {
	
	char status;

	reset();
	writeConfiguration(&m_settings);  // Write settings to configuration registers
	writePATable(m_power);

	//Put radio into idle state
	status = strobe(RF_SIDLE);
	while (status & 0xF0)
	{
	  status = strobe(RF_SNOP);
	}
}

void SpriteRadio_sleep() {
	
	strobe(RF_SIDLE); //Put radio back in idle mode
}
