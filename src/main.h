/*************************************************************************
**  AVR J1850 VPW Interface
**
**  by Michael Wolf
**  contact: webmaster@mictronics.de
**  homepage: www.mictronics.de
**
**  Modified by Remi Serriere
**  GitHub: https://github.com/remiserriere/AVR-J1850-VPW
**
**  Released under GNU GENERAL PUBLIC LICENSE
**
**  Revision History
**
**  when         what  	  who			why
**	31/12/04	v1.00 	Michael	Initial release
**  05/05/05    v1.01	Michael  	* changed integer types
**  19/08/05    v1.02 	Michael  	* changed status messages
**                      	        + added ELM322 ID tag
**	60/06/21	v1.08	Remi S 		* changed default parameters
**									+ added stopped text for ATMx AT commands 
**  10/07/21    v1.09   Remi S      + added parameter bit mask for message length checking or not
**                                  * changed SERIAL_MSG_BUF_SIZE to 128 bytes
**
**************************************************************************/
#ifndef __MAIN_H__
#define __MAIN_H__

// Set default RS232 baud rate
#define BAUD_RATE    115200

// J1850 message (max 12 byte - 3 byte header - 1 CRC byte) x 2
// because of 2 ASCII chars/byte + 1 terminator
// or 10 bytes for AT command
#define SERIAL_MSG_BUF_SIZE	128

const char ident_txt[]    PROGMEM = "AVR-J1850 VPW v1.09\r" __DATE__" / "__TIME__"\r\r";
//const char ident_txt[]    PROGMEM = "ELM322 v2.0\r\n\r\n";

const char bus_busy_txt[]   PROGMEM = "BUSBUSY\r";
const char bus_error_txt[]  PROGMEM = "BUSERROR\r";
const char data_error_txt[] PROGMEM = "<DATAERROR\r";
const char no_data_txt[]    PROGMEM = "NO DATA\r";
const char stopped[]    PROGMEM = "STOPPED\r";

// define bit macros
#define SETBIT(x,y) (x |= (y)) 		// Set bit y in byte x
#define CLEARBIT(x,y) (x &= (~y)) // Clear bit y in byte x
#define CHECKBIT(x,y) (x & (y)) 	// Check bit y in byte x

// define parameter bit mask constants 
#define ECHO 		0x0001 // bit 0 : Echo on/off
#define HEADER		0x0002 // bit 1 : Headers on/off
#define LINEFEED	0x0004 // bit 2 : Linefeeds on/off
#define RESPONSE	0x0008 // bit 3 : Responses on/off
#define PACKED		0x0010 // bit 4 : use packed data
#define AUTO_RECV	0x0020 // bit 5 : auto receive on/off
#define MON_TX		0x0040 // bit 6 : monitor transmitter
#define MON_RX		0x0080 // bit 7 : monitor receiver
#define MON_OBH		0x0100 // bit 8 : monitor one byte header
#define USE_OBH		0x0200 // bit 9 : use one byte header in Tx message
#define MSG_LEN		0x0400 // bit 10 : check for message length before sending on the bus

// use of bit-mask for parameters init to default values
volatile uint16_t parameter_bits = HEADER|RESPONSE|AUTO_RECV;
//volatile uint16_t parameter_bits = HEADER|LINEFEED|RESPONSE|AUTO_RECV;

uint8_t j1850_req_header[3] = {0x68, 0x6A, 0xF1};  // default request header
uint8_t auto_recv_addr = 0x6B;  // physical or functional address in receive mode
uint8_t mon_receiver;  // monitor receiver only addr
uint8_t mon_transmitter;  // monitor transmitter only addr

uint8_t serial_msg_buf[SERIAL_MSG_BUF_SIZE];	 // serial Rx buffer
uint8_t *serial_msg_pntr;

int16_t serial_putc(int8_t data);	// send one databyte to USART
void serial_put_byte2ascii(uint8_t val);
void serial_puts_P(const char *s);
int8_t serial_processing(void);
void ident(void);
void print_prompt(void);

#define DEFAULT_BAUD   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)BAUD_RATE*16)-1))	// calculate baud rate value for UBBR

#define BAUD_9600   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)9600*16)-1))
#define BAUD_14400   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)14400*16)-1))
#define BAUD_19200   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)19200*16)-1))
#define BAUD_28800   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)28800*16)-1))
#define BAUD_38400   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)38400*16)-1))
#define BAUD_57600   ((unsigned int)((unsigned long)MCU_XTAL/((unsigned long)57600*16)-1))

#endif // __MAIN_H__
