/*************************************************************************
**  AVR J1850 VPW Interface
**  by Michael Wolf
**  Modified by Remi Serriere
**
**  Released under GNU GENERAL PUBLIC LICENSE
**
**  contact: webmaster@mictronics.de
**  homepage: www.mictronics.de
**
**  Revision History
**
**  when         what  	who			why
**  31/12/04     v1.00b Michael Initial release
**															This is a beta version, some changes and
**															improvments needs must be made for future
**															versions.
**  07/01/05     v1.01  Michael + added correct no data timeout handling
**                              * watchdog will force a reset on AT Z
**  10/05/05     v1.04  Michael - fixed bug in received message error checking
**                              * changed integer types
**                              + added command AT MI to monitor frames with one
**                                byte header
**                              + added command AT Ox to enable/disable one
**                                byte header messages 
**                              - fixed bug for AT M... messages
**                              + added command AT Bx to set Baud rates
**  04/06/05     v1.05  Michael - fixed bug in one/three byte header Tx selection
**  19/08/05     v1.06  Michael + added prompt output after data
**	10/10/06     v1.07  Michael	* changed timeout handling for j1850_recv_msg()
**	20/06/21	 v1.08	Remi S	+ \r and \n reorganized to match ELM327 output as well as ATL AT command
**								* fixed ATSH 1-byte sets 1-byte header bit automatically
**								+ AVR-GCC 7.3.0 supported
**								
**
**  Used develompent tools (download @ www.avrfreaks.net):
**  Programmers Notepad v2.0.5.32
**  WinAVR (GCC) 3.4.1
**  AvrStudio4 for simulating and debugging
**	Note by Remi S: Compatible with WinAVR GCC 7.3.0, developped with Visual Studio Community
* 
**
**  [           Legend:          ]
**  [ + Added feature            ]
**  [ * Improved/changed feature ]
**  [ - Bug fixed (I hope)       ]
**
**	ToDo:
**  - tweak source code
**  - add transparent mode
**  - add 4x mode
**  - add block transmit mode
**
**************************************************************************/
#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <ctype.h>
#include "main.h"
#include "j1850.h"
/*
**---------------------------------------------------------------------------
**
** Abstract: Main routine
**
**
** Parameters: none
**
**
** Returns: NULL
**
**
**---------------------------------------------------------------------------
*/
// main routine
int16_t main( void )
{
	wdt_disable();	// make sure the watchdog is not running
	UBRRH = DEFAULT_BAUD>>8;		// set baud rate
	UBRRL = DEFAULT_BAUD;
	UCSRB =((1<<RXCIE)|(1<<RXEN)|(1<<TXEN));	// enable Rx & Tx, enable Rx interrupt
	UCSRC =((1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0));	// config USART; 8N1
	serial_msg_pntr = &serial_msg_buf[0];  // init serial msg pointer
	
	j1850_init();	// init J1850 bus

	ident();	// send identification to terminal

	serial_putc('>');  // send initial command prompt

	sei();	// enable global interrupts

	for(;;)
	{
		while( CHECKBIT(parameter_bits, MON_RX) || CHECKBIT(parameter_bits, MON_TX) || CHECKBIT(parameter_bits, MON_OBH))
		{
			uint8_t j1850_msg_buf[12];  // J1850 message buffer
			uint8_t *j1850_msg_pntr = &j1850_msg_buf[0];  //  msg pointer
			int8_t recv_nbytes;  // byte counter		
      
			recv_nbytes = j1850_recv_msg(j1850_msg_buf);	// get J1850 frame
		
			if( !(recv_nbytes & 0x80) ) // proceed only with no errors
			{
				j1850_msg_pntr = &j1850_msg_buf[0];
			
				// check for respond from correct addr or monitor all mode
				if( (CHECKBIT(parameter_bits, MON_RX) && CHECKBIT(parameter_bits, MON_TX))
				    ||
					((mon_receiver == *(j1850_msg_pntr+1)) && CHECKBIT(parameter_bits, MON_RX) )
					||
					((mon_transmitter == *(j1850_msg_pntr+2)) && CHECKBIT(parameter_bits, MON_TX) )
					||
					((mon_transmitter == *(j1850_msg_pntr)) && CHECKBIT(parameter_bits, MON_OBH) )
					)
				{
					// surpess CRC and header bytes output
					if( !CHECKBIT(parameter_bits, HEADER) )
					{ 
						if( CHECKBIT(parameter_bits, MON_OBH) ||  // check if one byte header frames are used
							CHECKBIT(parameter_bits, USE_OBH)
						   )
						{
						  recv_nbytes -= 2;  // discard 1st header byte and CRC
						  j1850_msg_pntr += 1;  // skip header byte
						}
						else
						{
						  recv_nbytes -= 4;  // discard 3 header bytes and CRC
						  j1850_msg_pntr += 3;  // skip 3 header bytes
						}
					}

					if(CHECKBIT(parameter_bits, PACKED))
					{ // check respond CRC
						if( *(j1850_msg_pntr+(recv_nbytes-1)) == j1850_crc(j1850_msg_buf, recv_nbytes-1) )
							serial_putc(recv_nbytes);  // length byte
						else
							serial_putc(recv_nbytes&0x80);  // length byte with error indicator set
					}
     
					// output response data
					for(;recv_nbytes > 0; recv_nbytes--)
					{
						if(CHECKBIT(parameter_bits, PACKED))
							serial_putc(*j1850_msg_pntr++);  // data byte
						else
						{
							serial_put_byte2ascii(*j1850_msg_pntr++);
							serial_putc(' ');
						}
					}
     			
					if(!CHECKBIT(parameter_bits, PACKED))
					{// formated output with CR and optional LF
						serial_putc('\r');
						if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					}					
					
				}  // end if valid monitoring addr
			} // end if message recv
		} // end while monitoring active
	}	// endless loop
	
	return 0;
} // end of main()

/*
**---------------------------------------------------------------------------
**
** Abstract: Send program ident string to terminal
**
**
** Parameters: none
**
**
** Returns: none
**
**
**---------------------------------------------------------------------------
*/
void ident(void)
{
	serial_puts_P(ident_txt);  // show code description
}



/*
**---------------------------------------------------------------------------
**
** Abstract: Convert 2 byte ASCII hex to 1 byte decimal
**
**
** Parameters: Pointer to first ASCII char
**
**
** Returns: decimal value
**
**
**---------------------------------------------------------------------------
*/
static uint8_t ascii2byte(char *val)
{
	uint8_t temp = *val;
	
	if(temp > 0x60) temp -= 39;  // convert chars a-f
	temp -= 48;  // convert chars 0-9
	temp *= 16;
	
	temp += *(val+1);
	if(*(val+1) > 0x60) temp -= 39;  // convert chars a-f
	temp -= 48;  // convert chars 0-9	

	return temp;

}



/*
**---------------------------------------------------------------------------
**
** Abstract: Processing of received serial string
**
** Parameters: none
**
** Returns: 0 = unknown
**          1 = OK
**          2 = bus busy
**          3 = bus error
**          4 = data error
**          5 = no data
**          6 = data ( also to surpress any other output )
**
**---------------------------------------------------------------------------
*/
int8_t serial_processing(void)
{
	char *serial_msg_pntr = strlwr((char *)serial_msg_buf);  // convert string to lower
	uint8_t serial_msg_len = strlen((char *)serial_msg_buf);  // get string length
  	uint8_t *var_pntr = 0;  // point to different variables
	
	uint8_t j1850_msg_len = (serial_msg_len - 4) / 2;	
	uint8_t j1850_msg_buf[12];  // J1850 message to be send

	if( (*(serial_msg_pntr)=='a') && (*(serial_msg_pntr+1)=='t'))  // check for "at" or hex
	{  // is AT command
		// AT command found
		switch( *(serial_msg_pntr+2) )  // switch on "at" command
		{
			case 'a':  // auto receive address on
				if(*(serial_msg_pntr+3) == 'r')	SETBIT(parameter_bits, AUTO_RECV);
				if( j1850_req_header[0] & 0x04)  // check for functional or physical addr
					auto_recv_addr = j1850_req_header[2]; // use physical recv addr
				else
					auto_recv_addr = j1850_req_header[1]+1;  // use funct recv addr
				return J1850_RETURN_CODE_OK ;

			case 'b':  // set Baud rate
				if( isdigit(*(serial_msg_pntr+3)) )
				{
				  switch(*(serial_msg_pntr+3))
				  {
					case '0':
					  UBRRH = BAUD_9600>>8;		// set 9600 Baud
					  UBRRL = BAUD_9600;
					  break;

					case '1':
					  UBRRH = BAUD_14400>>8;		// set 14.4k Baud
					  UBRRL = BAUD_14400;
					  break;
					  
					case '2':
					  UBRRH = BAUD_19200>>8;		// set 19.2k Baud
					  UBRRL = BAUD_19200;
					  break;

					case '3':
					  UBRRH = BAUD_28800>>8;		// set 28.8k Baud
					  UBRRL = BAUD_28800;
					  break;

					case '4':
					  UBRRH = BAUD_38400>>8;		// set 38.4k Baud
					  UBRRL = BAUD_38400;
					  break;

					case '5':
					  UBRRH = BAUD_57600>>8;		// set 57.6k Baud
					  UBRRL = BAUD_57600;
					  break;
					
					default:
					  UBRRH = DEFAULT_BAUD>>8;		// set default baud rate
					  UBRRL = DEFAULT_BAUD;
				  }
				  return J1850_RETURN_CODE_OK ;
				}
				return J1850_RETURN_CODE_UNKNOWN; 
		
			case 'd':  // set defaults
				parameter_bits = ECHO|RESPONSE|AUTO_RECV;
				timeout_multiplier = 0x19;	// set default timeout to 4ms * 25 = 100ms
				j1850_req_header[0] = 0x68;  // Prio 3, Functional Adressing
				j1850_req_header[1] = 0x6A;  // Target legislated diagnostic
				j1850_req_header[2] = 0xF1;  // Frame source = Diagnostic Tool
				return J1850_RETURN_CODE_OK ;
		
			case 'e':  // echo on/off
				if(*(serial_msg_pntr+3) == '0')
					CLEARBIT(parameter_bits, ECHO);
				else
					SETBIT(parameter_bits, ECHO);
				return J1850_RETURN_CODE_OK ;
			
			case 'i':  // send ident string
				ident();
				return J1850_RETURN_CODE_OK ;

			case 'l': // linefeed on/off (only for data strings)
				if(*(serial_msg_pntr+3) == '0')
					CLEARBIT(parameter_bits, LINEFEED);
				else
					SETBIT(parameter_bits, LINEFEED);
				return J1850_RETURN_CODE_OK ;			

			case 'h': // show headers on/off
				if(*(serial_msg_pntr+3) == '0')
					CLEARBIT(parameter_bits, HEADER);
				else
					SETBIT(parameter_bits, HEADER);
				return J1850_RETURN_CODE_OK ;

			case 'r': // show response on/off
				if(*(serial_msg_pntr+3) == '0')
					CLEARBIT(parameter_bits, RESPONSE);
				else
					SETBIT(parameter_bits, RESPONSE);
				return J1850_RETURN_CODE_OK ;

			case 'f': // send formated
				if(*(serial_msg_pntr+3) == 'd')
					CLEARBIT(parameter_bits, PACKED);
				return J1850_RETURN_CODE_OK ;

			case 'o': // one byte header on/off
				if(*(serial_msg_pntr+3) == '0')
					CLEARBIT(parameter_bits, USE_OBH);
				else
					SETBIT(parameter_bits, USE_OBH);
				return J1850_RETURN_CODE_OK ;

			case 'p': // send packed data
				if(*(serial_msg_pntr+3) == 'd')
					SETBIT(parameter_bits, PACKED);
				return J1850_RETURN_CODE_OK ;

			case 'm':  // switch into monitoring mode
				switch(*(serial_msg_pntr+3))
				{
					case 'a':
						SETBIT(parameter_bits, MON_RX);  // monitor all
						SETBIT(parameter_bits, MON_TX);
						return J1850_RETURN_CODE_DATA; // return, no following parameter
            
					case 'i':
									CLEARBIT(parameter_bits, MON_TX);
									CLEARBIT(parameter_bits, MON_RX);
									SETBIT(parameter_bits, MON_OBH);  // monitor one byte header
									var_pntr = &mon_transmitter;
						break;  // get folowing parameter
            
					case 'r':  // monitor only receiver addr
						SETBIT(parameter_bits, MON_RX);  // monitor receiver only
						CLEARBIT(parameter_bits, MON_TX);
						var_pntr = &mon_receiver;
						break;  // get following parameter					

					case 't':  // monitor only transmitter addr
						CLEARBIT(parameter_bits, MON_RX);
						SETBIT(parameter_bits, MON_TX);  // monitor transmitter only
						var_pntr = &mon_transmitter;
						break;  // get following parameter
          
					default:
						return J1850_RETURN_CODE_UNKNOWN;
				}
				if( isxdigit(*(serial_msg_pntr+4)) && isxdigit(*(serial_msg_pntr+5))&&	( serial_msg_len == 6))  // proceed when next two chars are hex
				{
				  // make 1 byte hex from 2 chars ASCII and save
					*var_pntr = ascii2byte(serial_msg_pntr+4);
					return J1850_RETURN_CODE_DATA;
				}

			case 's': // commands SH,SR or ST
				if(	isxdigit(*(serial_msg_pntr+4)) && isxdigit(*(serial_msg_pntr+5)) )
				{  // proceed when next two chars are hex
					switch(*(serial_msg_pntr+3))
					{
						case 'd': 
							while( *serial_msg_pntr )  // check all chars are valid hex chars
							{
								//serial_log(*serial_msg_pntr);
								++serial_msg_pntr;
							}
							
							serial_msg_pntr = &serial_msg_buf[0];
														
							// convert serial message from 2 byte ASCII to 1 byte binary and store
							for(int8_t cnt = 0; cnt < j1850_msg_len; cnt++)
							{
								j1850_msg_buf[cnt] = ascii2byte(serial_msg_pntr + 4);
								serial_msg_pntr += 2;		
							}
							
							// generate CRC for J1850 message and store, use 1 or 3 byte header
							j1850_msg_buf[j1850_msg_len] = j1850_crc( j1850_msg_buf,j1850_msg_len );  
						  
							// send J1850 message and save return code, use 1 or 3 byte header
							return j1850_send_msg(j1850_msg_buf, j1850_msg_len +1);
							 
						case 'h':  // set header bytes
							if(
								isxdigit(*(serial_msg_pntr+6)) && isxdigit(*(serial_msg_pntr+7))
								&&	isxdigit(*(serial_msg_pntr+8)) && isxdigit(*(serial_msg_pntr+9))
								&&	( serial_msg_len == 10)
							)  // proceed when next four chars are hex
							{
							  // make 3 byte hex from 6 chars ASCII and save
								j1850_req_header[0]=ascii2byte(serial_msg_pntr+4);
								j1850_req_header[1]=ascii2byte(serial_msg_pntr+6);
								j1850_req_header[2]=ascii2byte(serial_msg_pntr+8);
								if( CHECKBIT(parameter_bits, AUTO_RECV) )
								{
									if( j1850_req_header[0] & 0x04)  // check for functional or physical addr
										auto_recv_addr = j1850_req_header[1]; // use physical recv addr
									else
										auto_recv_addr = j1850_req_header[1]+1;  // use funct recv addr
								}
								return J1850_RETURN_CODE_OK ;
							}else if (
								( serial_msg_len == 6)
							)  // proceed when next four chars are hex
							{
								// Using 1 byte header
								SETBIT(parameter_bits, USE_OBH);
								
								// make 1 byte hex from 2 chars ASCII and save
								j1850_req_header[0]=ascii2byte(serial_msg_pntr+4);
								
								//if( CHECKBIT(parameter_bits, AUTO_RECV) )
								//{
									//if( j1850_req_header[0] & 0x04)  // check for functional or physical addr
									//	auto_recv_addr = j1850_req_header[2]; // use physical recv addr
									//else
									//	auto_recv_addr = j1850_req_header[1]+1;  // use funct recv addr
									auto_recv_addr = ascii2byte(serial_msg_pntr+4);
								//}
								
								return J1850_RETURN_CODE_OK ;
							}
								break;
					
						case 't':  // set response timeout multipler
								var_pntr = &timeout_multiplier;
								break;
								
						case 'r':  // set receive address and manual receive mode
								var_pntr = &auto_recv_addr;
								CLEARBIT(parameter_bits, AUTO_RECV);
								break;
            
						default:
							return J1850_RETURN_CODE_UNKNOWN;
					} // end switch char 3
					if(serial_msg_len == 6)
					{
						*var_pntr =ascii2byte(serial_msg_pntr+4);
						if (timeout_multiplier < 8)  timeout_multiplier = 8;  // set multiplier for minimum of 32ms timout
						return J1850_RETURN_CODE_OK ;
					}
				} // end if char 4 and 5 isxdigit
				return J1850_RETURN_CODE_UNKNOWN;

			case 'z':  // reset all and restart device
				wdt_enable(WDTO_15MS);	// enable watdog timeout 15ms
				for(;;);	// wait for watchdog reset
		
			default:  // return error, unknown command
				return J1850_RETURN_CODE_UNKNOWN;		
		}

	}
	else
	{  // is OBD hex command
		// no AT found, must be HEX code
		if( (serial_msg_len & 1) || (serial_msg_len > 16) )  // check for "even" message lenght
			return J1850_RETURN_CODE_UNKNOWN;                                           // and maximum of 8 data bytes

		serial_msg_len /= 2;  // use half the string lenght for byte count

		while( *serial_msg_pntr )  // check all chars are valid hex chars
		{
			if(!isxdigit(*serial_msg_pntr))
				return J1850_RETURN_CODE_UNKNOWN;
			++serial_msg_pntr;
		}
		serial_msg_pntr = (char *)&serial_msg_buf[0];  // reset pointer
	
		uint8_t j1850_msg_buf[12];  // J1850 message to be send
		uint8_t *j1850_msg_pntr = &j1850_msg_buf[0];  //  msg pointer
		uint8_t cnt;  // byte counter
		
		// store header bytes 1, use at least one header byte
		*j1850_msg_pntr = j1850_req_header[0];
    
		// store header 2-3 when three byte header is in use
		if( !CHECKBIT(parameter_bits, USE_OBH) )
		{
		  *(++j1850_msg_pntr) = j1850_req_header[1];
		  *(++j1850_msg_pntr) = j1850_req_header[2];
		}
    
		// convert serial message from 2 byte ASCII to 1 byte binary and store
		for(cnt = 0; cnt < serial_msg_len; ++cnt)
		{
			*(++j1850_msg_pntr) = ascii2byte(serial_msg_pntr);
			serial_msg_pntr += 2;		
		}
		
		// generate CRC for J1850 message and store, use 1 or 3 byte header
		if(CHECKBIT(parameter_bits, USE_OBH))
			*(++j1850_msg_pntr) = j1850_crc( j1850_msg_buf,serial_msg_len+1 );  // use one header bytes
		else
			*(++j1850_msg_pntr) = j1850_crc( j1850_msg_buf,serial_msg_len+3 );  // use three header byte
      
		// send J1850 message and save return code, use 1 or 3 byte header
		uint8_t return_code;
		if(CHECKBIT(parameter_bits, USE_OBH)){
			return_code = j1850_send_msg(j1850_msg_buf, serial_msg_len+2);
		}else{
			return_code = j1850_send_msg(j1850_msg_buf, serial_msg_len+4);
		}
		
		
		
		// skip receive in case of transmit error or RESPONSE disabled
		if( (return_code == J1850_RETURN_CODE_OK) && CHECKBIT(parameter_bits, RESPONSE) )
		{
			uint16_t time_count = 0;		

			do
			{
				/*
					Run this loop until we received a valid response frame, or response timed out,
					or the bus was idle for 100ms or an bus error occured.
				*/
			
				cnt = j1850_recv_msg(j1850_msg_buf);  // receive J1850 respond

				/*
					the j1850_recv_msg() has a timeout of 100us
					so the loop will run 100ms by default before timeout
					100ms is recommended by SAE J1850 spec
				*/
				++time_count;	
				if(time_count >= 1000)
					return J1850_RETURN_CODE_NO_DATA;
				
				/*
					Check for bus error. End the loop then.
				*/
				if( cnt == (J1850_RETURN_CODE_BUS_ERROR & 0x80) )  // check if we got an error code or just number of recv bytes
				{
					if(CHECKBIT(parameter_bits, PACKED))
					{
						serial_putc(0x80);  // lenght byte with error indicator set
						return J1850_RETURN_CODE_DATA;  // surpress any other output
					}
					else
						return cnt & 0x7F;  // return "receive message" error code
				}

				j1850_msg_pntr = &j1850_msg_buf[0];

			} while( 
			auto_recv_addr != *(j1850_msg_pntr+1) 
			);	

			// check respond CRC
			if( *(j1850_msg_pntr+(cnt-1)) != j1850_crc(j1850_msg_buf, cnt-1) )
			{
				if(CHECKBIT(parameter_bits, PACKED))
				{
					serial_putc(0x80);  // length byte with error indicator set
					return J1850_RETURN_CODE_DATA;  // surpress any other output
				}
				else{
					//serial_puts_P(debug_done_send);
					return J1850_RETURN_CODE_DATA_ERROR;
				}
			}
			
			
			
			// check for respond from correct addr in auto or man recv mode
			if( auto_recv_addr != *(j1850_msg_pntr+1) )
			{
				if(CHECKBIT(parameter_bits, PACKED))
				{
					serial_putc(0x00);  // length byte
					return J1850_RETURN_CODE_DATA;  // surpress any other output
				}
				else
					return J1850_RETURN_CODE_NO_DATA;
			}
			
			if( !CHECKBIT(parameter_bits, HEADER) )
			{ 
				if(CHECKBIT(parameter_bits, USE_OBH) )  // check if one byte header frames are used
				{
					cnt -= 2;  // discard 1st header byte and CRC
					j1850_msg_pntr += 1;  // skip header byte
				}
				else
				{
					cnt -= 4;  // discard 3 header bytes and CRC
					j1850_msg_pntr += 3;  // skip 3 header bytes
				}
			}
			
			
			
			if(CHECKBIT(parameter_bits, PACKED))
				serial_putc(cnt);  // length byte
			
			// output response data
			for(;cnt > 0; --cnt)
			{
				if(CHECKBIT(parameter_bits, PACKED))
					serial_putc(*j1850_msg_pntr++);  // length byte
				else
				{
					serial_put_byte2ascii(*j1850_msg_pntr++);
					serial_putc(' ');
				}
			}
			
			if(!CHECKBIT(parameter_bits, PACKED))
			{// formated output with CR and optional LF
				serial_putc('\r');
				if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
			}
			return J1850_RETURN_CODE_DATA;  // surpress any other output

		}  // end if J1850 OK && RESPONSE
		else  // transmit error or show RESPONSE OFF, return error code
			return return_code;
		
	} // end if !AT
	
	// we should never reach this return
	return J1850_RETURN_CODE_UNKNOWN;
} // end serial_processing


/*
**---------------------------------------------------------------------------
**
** Abstract: Send one byte via USART
**
** Parameters: data byte
**
** Returns: NULL
**
**---------------------------------------------------------------------------
*/
int16_t serial_putc(int8_t data)
{
	// wait for USART to become available
		while ( (UCSRA & _BV(UDRE)) != _BV(UDRE));
		UDR = data; 									// send character
		return 0;
}; //end usart_putc

void serial_log(int8_t c){
	serial_putc(c);
	serial_putc('\r');
	if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
}

/*
**---------------------------------------------------------------------------
**
** Abstract: Make 2 byte ASCII from one byte binary and send to terminal
**
** Parameters: input byte
**
** Returns: none
**
**---------------------------------------------------------------------------
*/
void serial_put_byte2ascii(uint8_t val)
{
	uint8_t ascii1=val;
	serial_putc( ((ascii1>>4) < 10) ? (ascii1>>4) + 48 : (ascii1>>4) + 55 );  // upper nibble
	serial_putc( ((val&0x0f) < 10) ? (val&0x0f) + 48 : (val&0x0f) + 55 );  // lower nibble
}


/*
**---------------------------------------------------------------------------
**
** Abstract: Print string in program memory to terminal
**
** Parameters: Pointer to string
**
** Returns: none
**
**---------------------------------------------------------------------------
*/
void serial_puts_P(const char *s)
{
	while( pgm_read_byte(&*s)) serial_putc( pgm_read_byte(&*s++));// send string char by char
}

/*
**---------------------------------------------------------------------------
**
** Abstract: USART Receive Interrupt
**
** Parameters: none
**
** Returns: none
**
**---------------------------------------------------------------------------
*/
//SIGNAL(SIG_UART_RECV)
/* USART, Rx Complete */
			
ISR(_VECTOR(11))
{
	uint8_t *hlp_pntr = &serial_msg_buf[sizeof(serial_msg_buf)-1];  // get end of serial buffer

	// check for buffer end, prevent buffer overflow
	if ( serial_msg_pntr > hlp_pntr )
	{
		serial_msg_pntr = &serial_msg_buf[sizeof(serial_msg_buf)-1];
	}
	
	uint8_t in_char = UDR;  // get received char

	// end monitor modes on any received char
	if( CHECKBIT(parameter_bits,MON_RX) ||
      CHECKBIT(parameter_bits,MON_TX) ||
      CHECKBIT(parameter_bits,MON_OBH)
    )
	{
		CLEARBIT(parameter_bits,MON_RX);
		CLEARBIT(parameter_bits,MON_TX);
		CLEARBIT(parameter_bits,MON_OBH);
		serial_puts_P(stopped);
		if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
		print_prompt();  // command prompt to terminal
	}
	else  // no active monitor mode
	{
		if( CHECKBIT(parameter_bits,ECHO) )  // return char when echo is on
			serial_putc(in_char);		

		// check for terminating char
		if(in_char == 0x0D)
		{
			//if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');    
			*(serial_msg_pntr) = 0x00;	// terminate received message
			switch ( serial_processing() )  // process serial message
			{
				case J1850_RETURN_CODE_OK:  // success
					serial_puts_P(PSTR("OK\r"));
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal
					break;
				case J1850_RETURN_CODE_BUS_BUSY:  // bus was busy
					serial_puts_P(bus_busy_txt);
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal
					break;
				case J1850_RETURN_CODE_BUS_ERROR:  // bus error detected
					serial_puts_P(bus_error_txt);
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal
					break;
				case J1850_RETURN_CODE_DATA_ERROR:  // data error detected
					serial_puts_P(data_error_txt);
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal						
					break;
				case J1850_RETURN_CODE_NO_DATA:  // no data response (response timeout)
					serial_puts_P(no_data_txt);
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal	
					break;
				case J1850_RETURN_CODE_DATA:     // data response
					if( 
						CHECKBIT(parameter_bits,MON_RX) ||
						CHECKBIT(parameter_bits,MON_TX) ||
						CHECKBIT(parameter_bits,MON_OBH)
					){
						break;
					}
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					print_prompt();  // command prompt to terminal
					break;			
				default: // unknown error
					serial_puts_P(PSTR("?\r"));
					if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
					//print_prompt();  // command prompt to terminal	
			}
			serial_msg_pntr = &serial_msg_buf[0];  // start new message
		}
	
		// received char was no termination
		if(isalnum((int16_t)in_char))
		{  // check for valid alphanumeric char and save in buffer
			*serial_msg_pntr = in_char;
			++serial_msg_pntr;	
		}
  }
};// end of UART receive interrupt


/*
**---------------------------------------------------------------------------
**
** Abstract: Print command prompt to terminal
**
**
** Parameters: none
**
**
** Returns: none
**
**
**---------------------------------------------------------------------------
*/
void print_prompt(void)
{
	if(CHECKBIT(parameter_bits, LINEFEED)) serial_putc('\n');
	serial_puts_P(PSTR("\r>"));	// send new command prompt
}
