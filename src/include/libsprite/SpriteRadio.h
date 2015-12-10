/*
  SpriteRadio.h - An Energia library for transmitting data using the CC430 series of devices
  
  by Zac Manchester

*/

#ifndef SpriteRadio_h
#define SpriteRadio_h

#define SR_DEMO_MODE

#define PRN_LENGTH_BYTES 64

#include "CC430Radio.h"

	// Constructor - optionally supply radio register settings
	void SpriteRadio_SpriteRadio();
//	SpriteRadio_SpriteRadio(unsigned char prn0[], unsigned char prn1[], CC1101Settings settings);
	
	// Set the transmitter power level. Default is 10 dBm.
	void SpriteRadio_setPower(int tx_power_dbm);

	// Transmit the given byte array as-is
    void SpriteRadio_rawTransmit(unsigned char bytes[], unsigned int length);

    // Encode the given byte with FEC and transmit
    void SpriteRadio_transmitByte(char byte);

    // Encode the given byte array with FEC and transmit
    void SpriteRadio_transmit(char bytes[], unsigned int length);

	// Initialize the radio - must be called before transmitting
    void SpriteRadio_txInit();

	// Put the radio in low power mode - call after transmitting
	void SpriteRadio_sleep();
	
	char SpriteRadio_fecEncode(char data);
void beginRawTransmit(unsigned char bytes[], unsigned int length);
void continueRawTransmit(unsigned char bytes[], unsigned int length);
void endRawTransmit();

#endif //SpriteRadio_h
