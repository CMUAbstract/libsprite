/* From: Energia/hardware/msp430/cores/msp430/WMath.cpp */

/* Using interal random and srandom in file random.c 
* until msp430-libc adds supports for random and srandom */
long random(void);
void srandom(unsigned long __seed);
