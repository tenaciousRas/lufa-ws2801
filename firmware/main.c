/*
 WS2801 Test Jig
 Copyright (C) Free Beachler, 2012 (longevitysoft at gmail.com).
 Original concept and source code copyright Sparkfun (www.sparkfun.com)

 Permission to use, copy, modify, distribute, and sell this
 software and its documentation for any purpose is hereby granted
 without fee, provided that the above copyright notice appear in
 all copies and that both that the copyright notice and this
 permission notice and warranty disclaimer appear in supporting
 documentation, and that the name of the author not be used in
 advertising or publicity pertaining to distribution of the
 software without specific, written prior permission.
 
 The author disclaim all warranties with regard to this
 software, including all implied warranties of merchantability
 and fitness.  In no event shall the author be liable for any
 special, indirect or consequential damages or any damages
 whatsoever resulting from loss of use, data or profits, whether
 in an action of contract, negligence or other tortious action,
 arising out of or in connection with the use or performance of
 this software.
 */

/** \file
 * Test-jig firmware for a WS2801 connected to an ATMega32u4 with LUFA support.
 * This can be used for basic verification of a working 32u4+WS2801 setup.
 * 
 * It only uses LUFA for some serial monitoring but this is easily extended.
 *
 * For the AdaFruit breakout the WS2801 connections are as follows:
 *   5v:  5v rail
 *   GND:  GND rail
 *   CKI:  PB0
 *   SDI:  PB2
 * 
 * For the Arduino Leonardo the WS2801 connections are as follows:
 *   5v:  5v rail
 *   GND:  GND rail
 *   CKI:  D10
 *   SDI:  D12
 */

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include "VirtualSerial.h"
#include "LUFA/Drivers/Peripheral/SPI.h"

int CKI = 0;
int SDI = 2;

#define STRIP_LENGTH 1 //1 LEDs on this strip
long strip_colors[STRIP_LENGTH];

extern CDC_Line_Coding_t sport;

static void writeEPByte(const char* data)
{
	if ((data != NULL) && sport.BaudRateBPS)
	{
		/* Select the Serial Tx Endpoint */
		Endpoint_SelectEndpoint(CDC_TX_EPNUM);
        
		/* Write the String to the Endpoint */
		Endpoint_Write_Stream_LE(data, strlen(data), NULL);
        
		/* Remember if the packet to send completely fills the endpoint */
		bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);
        
		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
        
		/* If the last packet filled the endpoint, send an empty packet to release the buffer on
		 * the receiver (otherwise all data will be cached until a non-full packet is received) */
		if (IsFull)
		{
			/* Wait until the endpoint is ready for another packet */
			Endpoint_WaitUntilReady();
            
			/* Send an empty packet to ensure that the host does not buffer data sent to it */
			Endpoint_ClearIN();
		}
	}
    
	/* Select the Serial Rx Endpoint */
	Endpoint_SelectEndpoint(CDC_RX_EPNUM);
    
	/* Throw away any received data from the host */
	if (Endpoint_IsOUTReceived()) {
        Endpoint_ClearOUT();
    }
}

//Throws random colors down the strip array
void addRandom(void) {
    int x;
    
    //First, shuffle all the current colors down one spot on the strip
    for(x = (STRIP_LENGTH - 1) ; x > 0 ; x--)
        strip_colors[x] = strip_colors[x - 1];
    
    //Now form a new RGB color
    long new_color = 0;
    for(x = 0 ; x < 3 ; x++){
        new_color <<= 8;
        new_color |= (random() * 2); //Give me a number from 0 to 0xFF
        // new_color |= 0xF0F0F0;  //poorman's random number to brightness levels.
    }
    
    strip_colors[0] = new_color; //Add the new random color to the strip
}

//Takes the current strip color array and pushes it out
void post_frame (void) {
    // each LED requires 24 bits of data
    // MSB: R7, R6, R5..., G7, G6..., B7, B6... B0 
    // once the 24 bits have been delivered, the IC immediately relays these bits to its neighbor
    // pulling clock low for 500us or more causes the IC to post the data.
    PORTB |= (1<<CKI);
    for(int LED_number = 0 ; LED_number < STRIP_LENGTH ; LED_number++) {
        long this_led_color = strip_colors[LED_number]; //24 bits of color data
        for(uint8_t color_bit = 23 ; color_bit != 255 ; color_bit--) {
            // feed color bit 23 first (red data MSB)
            PORTB &= ~(1<<CKI);
            long mask = 1L << color_bit;
            if (this_led_color & mask) {
                PORTB |= (1<<SDI);
            } else {
                PORTB &= ~(1<<SDI);
            }
            PORTB |= (1<<CKI);  // latch data
        }
        //Pull clock low to put strip into reset/post mode
        PORTB &= ~(1<<CKI);
        _delay_us(500);
    }
}

int main(void)
{
    writeEPByte("WS2801 Test JIG");
    // bit-bangin spi
    DDRB |= (1<<CKI | 1<<SDI);
    srandom(1);
    //Clear out the array
    for(int x = 0 ; x < STRIP_LENGTH ; x++) {
        strip_colors[x] = 0;
    }
    post_frame(); //Push the current color frame to the strip
    for(;;) {
        //Pre-fill the color array with known values
        strip_colors[0] = 0xFF0000; //Bright Red
        strip_colors[1] = 0x00FF00; //Bright Green
        strip_colors[2] = 0x0000FF; //Bright Blue
        strip_colors[3] = 0x010000; //Faint red
        strip_colors[4] = 0x800000; //1/2 red (0x80 = 128 out of 256)
        post_frame(); //Push the current color frame to the strip
        _delay_ms(2000);
        static char c_buff[7] = {0};
        while(1) { //Do nothing
            addRandom();
            writeEPByte("added hex color #");
            writeEPByte(itoa(24, c_buff, 16));
            writeEPByte("\n");
            post_frame(); //Push the current color frame to the strip
            _delay_ms(750);
        }
    }
    return 0;   /* unreachable */
}
