/*
  SpriteRadio.cpp - An Energia library for transmitting data using the CC430 series of devices
  CC430Radio.h - A low-level interface library for the CC1101 radio core in the CC430 series of devices.

  Adapted from the CC430 RF Examples from TI: http://www.ti.com/lit/an/slaa465b/slaa465b.pdf
  
  by Zac Manchester

*/

#include <stdint.h>
#include <msp430.h>

#include <libio/log.h>

#include "random.h"

#include "radio.h"

#define PRN_LENGTH_BYTES 64

static CC1101Settings m_settings;
static char m_power;
static const unsigned char *m_prn0;
static const unsigned char *m_prn1;

/*
  This example code will configure the CC1101 radio core in the CC430 to
  repeatedly transmit a text message. The output signal will be MSK modulated
  at 64 kbps on a 437.24 MHz carrier and bits are encoded by alternating between
  two different 511 bit Gold codes.
*/

static const unsigned char PRN0[64] = {
  0b00000000, 0b01110110, 0b10101101, 0b01010110, 0b00010111, 0b01111010, 0b00111000, 0b10001011,
  0b10010011, 0b10110001, 0b00110001, 0b00100110, 0b00101010, 0b11110111, 0b01010011, 0b01101011,
  0b01011110, 0b11111111, 0b00000110, 0b01000111, 0b01000010, 0b01010010, 0b11101011, 0b11000100,
  0b00001101, 0b00100110, 0b01010011, 0b01001001, 0b11101110, 0b00001110, 0b11101101, 0b11110010,
  0b00000111, 0b10010010, 0b01110100, 0b00010010, 0b10111101, 0b00011000, 0b10001010, 0b00101011,
  0b10101011, 0b10001100, 0b10111110, 0b00001110, 0b00000111, 0b11011101, 0b11101000, 0b00011110,
  0b10011000, 0b01010101, 0b10111000, 0b01101000, 0b01001111, 0b11011111, 0b00111001, 0b01100011,
  0b11001011, 0b10111010, 0b01011111, 0b00100100, 0b11011010, 0b10000000, 0b01010000, 0b10111110
};

static const unsigned char PRN2[64] = {
  0b00000001, 0b01011110, 0b11010100, 0b01100001, 0b00001011, 0b11110011, 0b00110001, 0b01011100,
  0b01100110, 0b10010010, 0b01011011, 0b00101010, 0b11100000, 0b10100011, 0b00000000, 0b11100001,
  0b10111011, 0b10011111, 0b00110001, 0b11001111, 0b11110111, 0b11000000, 0b10110010, 0b01110101,
  0b10101010, 0b10100111, 0b10100101, 0b00010010, 0b00001111, 0b01011011, 0b00000010, 0b00111101,
  0b01001110, 0b01100000, 0b10001110, 0b00010111, 0b00110100, 0b10000101, 0b01100001, 0b01000101,
  0b00000110, 0b10100010, 0b00110110, 0b00101111, 0b10101001, 0b00011111, 0b11010111, 0b11111101,
  0b10011101, 0b01001000, 0b00011001, 0b00011000, 0b10101111, 0b00110110, 0b10010011, 0b00000000,
  0b00010000, 0b10000101, 0b00101000, 0b00011101, 0b01011100, 0b10101111, 0b01100100, 0b11011010
};

static const unsigned char PRN3[64] = {
  0b11111101, 0b00111110, 0b01110111, 0b11010101, 0b00100101, 0b11101111, 0b00101100, 0b01101001,
  0b00101010, 0b11101001, 0b00111100, 0b11000100, 0b00000111, 0b10010011, 0b11000101, 0b00000111,
  0b00110111, 0b00011111, 0b01111011, 0b11010001, 0b10111010, 0b00000111, 0b10010000, 0b00110111,
  0b11011111, 0b01011010, 0b11101101, 0b11001000, 0b10001100, 0b01101001, 0b10010111, 0b00101001,
  0b10101100, 0b11011001, 0b11010110, 0b00011010, 0b11010110, 0b10101000, 0b00000101, 0b11010011,
  0b01101010, 0b11001011, 0b11010110, 0b01010010, 0b00111111, 0b11100111, 0b10000010, 0b10000110,
  0b01101110, 0b10011010, 0b01100101, 0b10100110, 0b00101110, 0b01010100, 0b11110100, 0b01111010,
  0b11001011, 0b00101110, 0b01100011, 0b10111111, 0b01010100, 0b11000100, 0b11010100, 0b01010100
};

/* The following delay functions are from
 * Energia/hardware/msp430/cores/msp430/wiring.c */

/* WDT_TICKS_PER_MILISECOND = (F_CPU / WDT_DIVIDER) / 1000
 * WDT_TICKS_PER_MILISECONDS = 1.953125 = 2 */
#define SMCLK_FREQUENCY F_CPU
#define WDT_TICKS_PER_MILISECOND (2*SMCLK_FREQUENCY/1000000)
#define WDT_DIV_BITS WDT_MDLY_0_5

static volatile uint32_t wdtCounter = 0;

/* (ab)use the WDT */
static void delay(uint32_t milliseconds)
{
	uint32_t wakeTime = wdtCounter + (milliseconds * WDT_TICKS_PER_MILISECOND);

	WDTCTL = WDTPW | WDTTMSEL | WDTCNTCL | WDT_DIV_BITS;
	SFRIE1 |= WDTIE;

	while(wdtCounter < wakeTime) {
			/* Wait for WDT interrupt in LMP0 */
			//__bis_SR_register(LPM0_bits+GIE);
	}

	WDTCTL = WDTPW | WDTHOLD;
}

static void __inline__ delayClockCycles(register unsigned int n)
{
    __asm__ __volatile__ (
                "1: \n"
                " dec        %[n] \n"
                " jne        1b \n"
        : [n] "+r"(n));
}

__attribute__((interrupt(WDT_VECTOR)))
void watchdog_isr (void)
{
    wdtCounter++;
    /* Exit from LMP3 on reti (this includes LMP0) */
	//__bic_SR_register_on_exit(LPM3_bits);
}

/* The following randomness functions are from
 * Energia/hardware/msp430/cores/msp430/WMath.cpp */

static void randomSeed(unsigned int seed)
{
    if (seed != 0) {
        srandom(seed);
    }
}

static long randomFromZero(long howbig)
{
    if (howbig == 0) {
        return 0;
    }
    return random() % howbig;
}

static long randomInRange(long howsmall, long howbig)
{
    if (howsmall >= howbig) {
        return howsmall;
    }
    long diff = howbig - howsmall;
    return randomFromZero(diff) + howsmall;
}

// Read a single byte from the radio register
static unsigned char readRegister(unsigned char address)
{
    unsigned char data_out;

    // Check for valid configuration register address, 0x3E refers to PATABLE 
    if ((address <= 0x2E) || (address == 0x3E))
    {
        // Send address + Instruction + 1 dummy byte (auto-read)
        RF1AINSTR1B = (address | RF_SNGLREGRD);
    }
    else
    {
        // Send address + Instruction + 1 dummy byte (auto-read)
        RF1AINSTR1B = (address | RF_STATREGRD);
    }

    while (!(RF1AIFCTL1 & RFDOUTIFG) );
    data_out = RF1ADOUTB;  // Read data and clear the RFDOUTIFG

    return data_out;
}

// Write a single byte to the radio register
static void writeRegister(unsigned char address, const unsigned char value)
{
	while (!(RF1AIFCTL1 & RFINSTRIFG));  // Wait for the Radio to be ready for next instruction
	RF1AINSTRB = (address | RF_SNGLREGWR);	// Send address + instruction

	RF1ADINB = value;  // Write data

	__nop();
}


// Send a command to the radio
static unsigned char strobe(unsigned char command)
{
	unsigned char status_byte = 0;
	unsigned int  gdo_state;
	
	// Check for valid strobe command 
	if((command == 0xBD) || ((command >= RF_SRES) && (command <= RF_SNOP)))
	{
    	// Clear the Status read flag 
    	RF1AIFCTL1 &= ~(RFSTATIFG);    
    
    	// Wait for radio to be ready for next instruction
    	while( !(RF1AIFCTL1 & RFINSTRIFG));
    
    	// Write the strobe instruction
    	if ((command > RF_SRES) && (command < RF_SNOP))
    	{
      		gdo_state = readRegister(IOCFG2);    // buffer IOCFG2 state
      		writeRegister(IOCFG2, 0x29);         // chip-ready to GDO2
      
      		RF1AINSTRB = command; 
      		if ( (RF1AIN&0x04)== 0x04 )           // chip at sleep mode
      		{
        		if ( (command == RF_SXOFF) || (command == RF_SPWD) || (command == RF_SWOR) ) { }
        		else  	
        		{
          			while ((RF1AIN&0x04)== 0x04);     // chip-ready ?
          			delayClockCycles(6480); // Delay for ~810usec at 8MHz CPU clock, see erratum RF1A7
        		}
      		}
      		writeRegister(IOCFG2, gdo_state);    // restore IOCFG2 setting
    
      		while( !(RF1AIFCTL1 & RFSTATIFG) );
    	}
		else		                    // chip active mode (SRES)
    	{	
      		RF1AINSTRB = command; 	   
    	}
		status_byte = RF1ASTATB;
	}
	return status_byte;
}

// Reset the radio core
static void reset(void) {
	strobe(RF_SRES);  // Reset the radio core
	strobe(RF_SNOP);  // Reset the radio pointer
}

// Write data to the transmit FIFO buffer. Max length is 64 bytes.
static void writeTXBuffer(const unsigned char *data, unsigned char length) {
	
	// Write Burst works wordwise not bytewise - known errata
	unsigned char i;

	while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for the Radio to be ready for next instruction
	RF1AINSTRW = ((RF_TXFIFOWR | RF_REGWR)<<8 ) + data[0]; // Send address + instruction

	for (i = 1; i < length; i++)
	{
	  RF1ADINB = data[i];                   // Send data
	  while (!(RFDINIFG & RF1AIFCTL1));     // Wait for TX to finish
	} 
	i = RF1ADOUTB;                          // Reset RFDOUTIFG flag which contains status byte
	
}

// Write zeros to the transmit FIFO buffer. Max length is 64 bytes.
static void writeTXBufferZeros(unsigned char length) {
  
  // Write Burst works wordwise not bytewise - known errata
  unsigned char i;

  while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for the Radio to be ready for next instruction
  RF1AINSTRW = ((RF_TXFIFOWR | RF_REGWR)<<8 ) + 0; // Send address + instruction

  for (i = 1; i < length; i++)
  {
    RF1ADINB = 0;                           // Send data
    while (!(RFDINIFG & RF1AIFCTL1));       // Wait for TX to finish
  } 
  i = RF1ADOUTB;                            // Reset RFDOUTIFG flag which contains status byte
  
}

#if 0
// Read data from the receive FIFO buffer. Max length is 64 bytes.
static void readRXBuffer(unsigned char *data, unsigned char length) {

  unsigned int i;

  while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for INSTRIFG
  RF1AINSTR1B = (RF_RXFIFORD | RF_REGRD);   // Send addr of first conf. reg. to be read 
                                            // ... and the burst-register read instruction
  for (i = 0; i < (length-1); i++)
  {
    while (!(RFDOUTIFG&RF1AIFCTL1));        // Wait for the Radio Core to update the RF1ADOUTB reg
    data[i] = RF1ADOUT1B;                   // Read DOUT from Radio Core + clears RFDOUTIFG
                                            // Also initiates auo-read for next DOUT byte
  }
  data[length-1] = RF1ADOUT0B;            // Store the last DOUT from Radio Core  
}
#endif

// Write the RF configuration settings to the radio
static void writeConfiguration(CC1101Settings *settings) {
	writeRegister(FSCTRL1,  settings->fsctrl1);
    writeRegister(FSCTRL0,  settings->fsctrl0);
    writeRegister(FREQ2,    settings->freq2);
    writeRegister(FREQ1,    settings->freq1);
    writeRegister(FREQ0,    settings->freq0);
    writeRegister(MDMCFG4,  settings->mdmcfg4);
    writeRegister(MDMCFG3,  settings->mdmcfg3);
    writeRegister(MDMCFG2,  settings->mdmcfg2);
    writeRegister(MDMCFG1,  settings->mdmcfg1);
    writeRegister(MDMCFG0,  settings->mdmcfg0);
    writeRegister(CHANNR,   settings->channr);
    writeRegister(DEVIATN,  settings->deviatn);
    writeRegister(FREND1,   settings->frend1);
    writeRegister(FREND0,   settings->frend0);
    writeRegister(MCSM0 ,   settings->mcsm0);
    writeRegister(FOCCFG,   settings->foccfg);
    writeRegister(BSCFG,    settings->bscfg);
    writeRegister(AGCCTRL2, settings->agcctrl2);
    writeRegister(AGCCTRL1, settings->agcctrl1);
    writeRegister(AGCCTRL0, settings->agcctrl0);
    writeRegister(FSCAL3,   settings->fscal3);
    writeRegister(FSCAL2,   settings->fscal2);
    writeRegister(FSCAL1,   settings->fscal1);
    writeRegister(FSCAL0,   settings->fscal0);
    writeRegister(FSTEST,   settings->fstest);
    writeRegister(TEST2,    settings->test2);
    writeRegister(TEST1,    settings->test1);
    writeRegister(TEST0,    settings->test0);
    writeRegister(FIFOTHR,  settings->fifothr);
    writeRegister(IOCFG2,   settings->iocfg2);
    writeRegister(IOCFG0,   settings->iocfg0);
    writeRegister(PKTCTRL1, settings->pktctrl1);
    writeRegister(PKTCTRL0, settings->pktctrl0);
    writeRegister(ADDR,     settings->addr);
    writeRegister(PKTLEN,   settings->pktlen);
}

// Set radio output power registers
static void writePATable(unsigned char value) {
	
	unsigned char valueRead = 0;
	while(valueRead != value)
  	{
    	/* Write the power output to the PA_TABLE and verify the write operation.  */
    	unsigned char i = 0; 

    	/* wait for radio to be ready for next instruction */
    	while( !(RF1AIFCTL1 & RFINSTRIFG));
    	RF1AINSTRW = (0x7E00 | value); // PA Table write (burst)
    
    	/* wait for radio to be ready for next instruction */
    	while( !(RF1AIFCTL1 & RFINSTRIFG));
      	RF1AINSTR1B = RF_PATABRD;
    
    	// Traverse PATABLE pointers to read 
    	for (i = 0; i < 7; i++)
    	{
      		while( !(RF1AIFCTL1 & RFDOUTIFG));
      		valueRead  = RF1ADOUT1B;     
    	}
    	while( !(RF1AIFCTL1 & RFDOUTIFG));
    	valueRead  = RF1ADOUTB;
	}
}

void radio_init() {
	
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

	m_prn0 = PRN2;
	m_prn1 = PRN3;

	//Initialize random number generator
	randomSeed(((int)m_prn0[0]) + ((int)m_prn1[0]) + ((int)m_prn0[1]) + ((int)m_prn1[1]));
}

// Set the output power of the transmitter.
void radio_setPower(int tx_power_dbm) {
	
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

static char fecEncode(char data)
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

static void beginRawTransmit(const unsigned char bytes[], unsigned int length) {
	char status;

	LOG("radio: waiting for idle\r\n");
	//Wait for radio to be in idle state
	status = strobe(RF_SIDLE);
	while (status & 0xF0)
	{
		status = strobe(RF_SNOP);
	}
	
	//Clear TX FIFO
	LOG("radio: clear tx fifo\r\n");
	status = strobe(RF_SFTX);

	if(length <= 64)
	{
    	LOG("radio: write tx buf\r\n");
		writeTXBuffer(bytes, length); //Write bytes to transmit buffer
    	LOG("radio: turning tx on\r\n");
		status = strobe(RF_STX);  //Turn on transmitter
    	LOG("radio: tx status %x\r\n", status);
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
			delay(1); //Wait for some bytes to be transmitted

			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

			writeTXBuffer(bytes+counter, bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}
}

static void continueRawTransmit(const unsigned char bytes[], unsigned int length) {

	unsigned char bytes_free, bytes_to_write;
	unsigned int bytes_to_go, counter;
		
	bytes_to_go = length;
	counter = 0;

	LOG("radio: cont tx: len %u\r\n", length);

	if(bytes)
	{
		while(bytes_to_go)
		{
			delay(1); //Wait for some bytes to be transmitted

        	LOG("radio: cont (1): get free\r\n");
			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

        	LOG("radio: cont (1): free %u b left %u b\r\n",
                    bytes_free, bytes_to_write);

			writeTXBuffer(bytes+counter, bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}
	else
	{
		while(bytes_to_go)
		{
			delay(1); //Wait for some bytes to be transmitted

        	LOG("radio: cont (2): get free\r\n");
			bytes_free = strobe(RF_SNOP) & 0x0F;
			bytes_to_write = bytes_free < bytes_to_go ? bytes_free : bytes_to_go;

        	LOG("radio: cont (2): free %u b left %u b\r\n",
                    bytes_free, bytes_to_write);

			writeTXBufferZeros(bytes_to_write);
			bytes_to_go -= bytes_to_write;
			counter += bytes_to_write;
		}
	}

	return;
}

static void endRawTransmit() {

	char status = strobe(RF_SNOP);

	//Wait for transmission to finish
	LOG("radio: wait for tx to finish\r\n");
	while(status != 0x7F)
	{
		status = strobe(RF_SNOP);
	}
	LOG("radio: idleing\r\n");
	strobe(RF_SIDLE); //Put radio back in idle mode
	return;
}

void radio_rawTransmit(const unsigned char bytes[], unsigned int length) {
	
	beginRawTransmit(bytes, length);
	endRawTransmit();
}

void radio_transmitByte(const char byte)
{
	char parity = fecEncode(byte);

	LOG("radio: tx byte: %u\r\n", byte);

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



void radio_transmit(const char bytes[], unsigned int length)
{
#ifdef SR_DEMO_MODE

	LOG("radio: transmit %u b\r\n", length);

	for(int k = 0; k < length; ++k)
	{
		radio_transmitByte(bytes[k]);
		delay(1000);
	}

#else

	delay(randomInRange(0, 2000));

	for(int k = 0; k < length; ++k)
	{
		radio_transmitByte(bytes[k]);

		delay(randomInRange(8000, 12000));
	}

#endif
}

void radio_txInit() {
	
	char status;

	LOG("radio: reset\r\n");
	reset();

	LOG("radio: write config\r\n");
	writeConfiguration(&m_settings);  // Write settings to configuration registers
	writePATable(m_power);

	//Put radio into idle state
	LOG("radio: idle\r\n");
	status = strobe(RF_SIDLE);
	while (status & 0xF0)
	{
	  status = strobe(RF_SNOP);
	}

	LOG("radio: ready\r\n");
}

void radio_sleep() {
	
	LOG("radio: sleep\r\n");
	strobe(RF_SIDLE); //Put radio back in idle mode
}
