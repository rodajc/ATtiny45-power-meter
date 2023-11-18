/* Name: main.c
 * Project: EasyLogger
 * Author: Christian Starkjohann
 * Creation Date: 2006-04-23
 * Tabsize: 4
 * Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id$
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "oddebug.h"

/* interface with usb_control_msg for CLI */
#define CLICMD_ECHO   0
#define CLICMD_GETOSC 1
#define CLICMD_RUNADC 6
#define CLICMD_GETADC 7
#define CLICMD_GETACC 8
#define CLICMD_GETCNT 9

#define STATE_WAIT 0
#define STATE_SEND_KEY 1
#define STATE_RELEASE_KEY 2

#define UTIL_BIN4(x)        (uchar)((0##x & 01000)/64 + (0##x & 0100)/16 + (0##x & 010)/4 + (0##x & 1))
#define UTIL_BIN8(hi, lo)   (uchar)(UTIL_BIN4(hi) * 16 + UTIL_BIN4(lo))

#ifndef NULL
#define NULL    ((void *)0)
#endif

/* ------------------------------------------------------------------------- */

static uchar    reportBuffer[2];    /* buffer for HID reports */
static uchar    idleRate;           /* in 4 ms units */
static uchar    intervalRunning, adcPending, defOSCCAL, state = STATE_WAIT;
unsigned int    adcCnt;
static unsigned long     adcAccu;


/* ------------------------------------------------------------------------- */

const PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { /* USB report descriptor */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};
/* We use a simplifed keyboard report descriptor which does not support the
 * boot protocol. We don't allow setting status LEDs and we only allow one
 * simultaneous key press (except modifiers). We can therefore use short
 * 2 byte input reports.
 * The report descriptor has been created with usb.org's "HID Descriptor Tool"
 * which can be downloaded from http://www.usb.org/developers/hidpage/.
 * Redundant entries (such as LOGICAL_MINIMUM and USAGE_PAGE) have been omitted
 * for the second INPUT item.
 */

/* Keyboard usage values, see usb.org's HID-usage-tables document, chapter
 * 10 Keyboard/Keypad Page for more codes.
 */
#define MOD_CONTROL_LEFT    (1<<0)
#define MOD_SHIFT_LEFT      (1<<1)
#define MOD_ALT_LEFT        (1<<2)
#define MOD_GUI_LEFT        (1<<3)
#define MOD_CONTROL_RIGHT   (1<<4)
#define MOD_SHIFT_RIGHT     (1<<5)
#define MOD_ALT_RIGHT       (1<<6)
#define MOD_GUI_RIGHT       (1<<7)

#define KEY_OKAY      0x4B

/* ------------------------------------------------------------------------- */

static void buildReport(void)
{
    reportBuffer[0] = 0;    /* no modifiers */
    if(state == STATE_SEND_KEY)
        reportBuffer[1] = KEY_OKAY;
    else
        reportBuffer[1] = 0;
}

/* ------------------------------------------------------------------------- */

static void adcInit(void)
{
    ADMUX = 0b10000110;  /* Vref=1.1V internal reference, measure ADC2-ADC3, gain=1x */
    ADCSRA = 0b00000111; /* disable auto trigger, disable interrupt, rate = 1/128 */
    ADCSRB = 0b10000000; /* enable BIN: Bipolar Input Mode */
}

/* ------------------------------------------------------------------------- */

static void startTimer(void)
{
    if(intervalRunning == 0){
	TCCR1 = 0x0f;    /* select clock: 16.5M/16384 -> overflow occurs after 254 ms */
	TCNT1 = 55;      /* to achieve overflow in 200 ms i.e. approx. 10 waves @ 50 Hz */
	if(TIFR & (1 << TOV1))
	    TIFR = (1 << TOV1);  /* clear overflow */
	intervalRunning = 1;
	ADCSRA |= (1 << ADEN);   /* enable ADC */
	adcPending = 0;
	adcCnt = 0;
	adcAccu = 0;
    }
}

/* ------------------------------------------------------------------------- */

static void timerPoll(void)
{
    if(TIFR & (1 << TOV1)){
        TIFR = (1 << TOV1);      /* clear overflow */
	intervalRunning = 0;
	ADCSRA &= ~(1 << ADEN);  /* disable ADC */
	TCCR1 = 0x00;            /* stop timer/counter1 */
    }
}

/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */

uchar	usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
static uchar            replyBuf[3];
static unsigned int     adcResult;


    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = reportBuffer;
            reportBuffer[0] = 0;
            reportBuffer[1] = 0;
            return sizeof(reportBuffer);
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }else{
        switch(rq->bRequest) {
        case CLICMD_ECHO:  /* result = 2 bytes */
            usbMsgPtr = replyBuf;
            replyBuf[0] = rq->wValue.bytes[0];
            replyBuf[1] = rq->wValue.bytes[1];
            return 2;
        case CLICMD_GETOSC:  /* result = 2 bytes */
            usbMsgPtr = replyBuf;
            replyBuf[0] = defOSCCAL;
            replyBuf[1] = OSCCAL;
            return 2;
        case CLICMD_RUNADC:  /* no response expected */
	    startTimer();
            return 0;
        case CLICMD_GETADC:  /* result = 2 bytes */
            usbMsgPtr = replyBuf;
	    if(intervalRunning | (adcCnt == 0)){
                replyBuf[0] = 0;
                replyBuf[1] = 0;
            }else{
		adcResult = (float)adcAccu / adcCnt + 0.5;  /* average voltage = peak voltage * 2/π */
                replyBuf[0] = adcResult & 255; /* low byte */
                replyBuf[1] = adcResult >> 8;  /* high byte */
            }
            return 2;
        case CLICMD_GETACC:  /* result = 3 bytes */
            usbMsgPtr = replyBuf;
	    if(intervalRunning){
                replyBuf[0] = 0;
                replyBuf[1] = 0;
                replyBuf[2] = 0;
            }else{
                replyBuf[0] = adcAccu & 255;    /* low byte */
		adcResult = adcAccu >> 8;
                replyBuf[1] = adcResult & 255;    /* second byte */
                replyBuf[2] = adcResult >> 8;     /* high byte */
            }
            return 3;
        case CLICMD_GETCNT:  /* result = 2 bytes */
            usbMsgPtr = replyBuf;
	    if(intervalRunning){
                replyBuf[0] = 0;
                replyBuf[1] = 0;
            }else{
                replyBuf[0] = adcCnt & 255; /* low byte */
                replyBuf[1] = adcCnt >> 8;  /* high byte */
            }
            return 2;
        }
    }
    return 0;
}


/* ------------------------------------------------------------------------- */
/* ------------------------ Oscillator Calibration ------------------------- */
/* ------------------------------------------------------------------------- */

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET.
 * More info: /home/sil/avr/vusb/projects/tinycalibration/readme.txt
 */
static void calibrateOscillator(void)
{
uchar       step = 64;
uchar       trialValue, optimumValue;
int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

    if(OSCCAL < 116)         /* pre-programmed calibration value OSCCAL determines range to search */
        trialValue = 0;
    else if(OSCCAL < 128)    /* ranges are overlapping and this value is very high for the low range */
	trialValue = 128;    /*  so search high range */
    else if(OSCCAL < 140)    /* ranges are overlapping and this value is very low for the high range */
	trialValue = 0;      /*  so search low range */
    else
        trialValue = 128;
    /* do a binary search: */
    do{
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength();    /* proportional to current real frequency */
        if(x < targetValue)             /* frequency still too low */
            trialValue += step;
        step >>= 1;
    }while(step > 0);
    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    optimumDev = x; /* this is certainly far away from optimum */
    for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
        x = usbMeasureFrameLength() - targetValue;
        if(x < 0)
            x = -x;
        if(x < optimumDev){
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue;
}

void    usbEventResetReady(void)
{
    /* Disable interrupts during oscillator calibration since
     * usbMeasureFrameLength() counts CPU cycles.
     */
    cli();
    calibrateOscillator();
    sei();
}

/* ------------------------------------------------------------------------- */
/* --------------------------------- main ---------------------------------- */
/* ------------------------------------------------------------------------- */

int main(void)
{
uchar            i, adcHi, adcLo;
unsigned int     adcValue;

    defOSCCAL=OSCCAL;

    /* calibration value OSCCAL is fine tuned after USB reset. Refer to Oscillator Calibration above */

    odDebugInit();
    usbDeviceDisconnect();
    for(i=0;i<20;i++){  /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    adcInit();
    usbInit();
    sei();
    for(;;){    /* main event loop */

        timerPoll();

	if(intervalRunning){
	    if(adcPending == 0){
		adcPending = 1;
		ADCSRA |= (1 << ADSC);  /* start next conversion */
	    }
	    if(adcPending && !(ADCSRA & (1 << ADSC))){
		adcPending = 0;
		adcLo = ADCL;
		adcHi = ADCH;
		if(adcHi > 1){
		    /* result is negative so clear sign in ADC9 and process two's complement value */
		    adcHi -= 2;
		    adcValue = 512 - (256 * adcHi + adcLo);
		}else
		    adcValue = 256 * adcHi + adcLo;
		adcAccu += adcValue;
		adcCnt++;
	    }
	}

        usbPoll();
        if(usbInterruptIsReady() && state != STATE_WAIT){
            switch(state) {
            case STATE_SEND_KEY:
                buildReport();
                state = STATE_RELEASE_KEY; // release next
                break;
            case STATE_RELEASE_KEY:
                buildReport();
                state = STATE_WAIT; // go back to waiting
                break;
            default:
                state = STATE_WAIT; // should not happen
            }
            // start sending
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
    }
    return 0;
}
