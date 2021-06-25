/*
 * File:   main.c
 * Author: dan1138
 * Target: PIC16F870
 * Compiler: XC8 v2.31
 * IDE: MPLABX v5.45
 *
 * Created on June 24, 2021, 5:40 PM
 * 
 * Description:
 * 
 *      Front panel controller and infrared receiver decoder.
 * 
 *                            PIC16F870
 *                  +-----------:_:-----------+
 *       ICD_VPP -> :  1 MCLRn         PGD 28 : <> RB7         ICD_PGD
 *   SW_EN_a RA0 <> :  2 AN0           PGC 27 : <> RB6 LED_REC/ICD_PGC
 *   SW_EN_b RA1 <> :  3 AN1               26 : <> RB5 LED_IN6 (tape)
 *   SW_EN_c RA2 <> :  4 AN2               25 : <> RB4 LED_IN5 (tuner)
 *  SW7_RECn RA3 <> :  5 AN3           PGM 24 : <> RB3 LED_IN4 (a.v.)
 * IR_IN_RC5 RA4 <> :  6 T0CKI             23 : <> RB2 LED_IN3 (cd)
 *  DEBUG_IO RA5 <> :  7 AN4               22 : <> RB1 LED_IN2 (video)
 *           GND <> :  8 VSS          INT0 21 : <> RB0 LED_IN1 (disc)
 *     4MHz XTAL -> :  9 OSC1          VDD 20 : <- 5v0
 *     4MHz XTAL <- : 10 OSC2          VSS 19 : <- GND
 *  LED_REC1 RC0 <> : 11 T1OSO          RX 18 : <> RC7 MUTEn
 *  LED_REC2 RC1 <> : 12 T1OSI          TX 17 : <> RC6 MOTOR_A (VOL+)
 *  LED_REC3 RC2 <> : 13 CCP1              16 : <> RC5 MOTOR_B (VOL-)
 *  LED_REC4 RC3 <> : 14                   15 : <> RC4 LED_REC5
 *                  +-------------------------+
 *                            DIP-28 
 */

#pragma config FOSC = XT        // Oscillator Selection bits (XT oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config CP = OFF         // FLASH Program Memory Code Protection bits (Code protection off)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF        // Low Voltage In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection (Code Protection off)
#pragma config WRT = ALL        // FLASH Program Memory Write Enable (Unprotected program memory may be written to by EECON control)

/* Include definitions for device specific special function registers */
#include <xc.h>
#include <stdint.h>

/* Tell compiler the system oscillator frequency we will setup */
#define _XTAL_FREQ (4000000ul)

/*
 * Application specific defines
 */
#define SW_EN_PORT PORTA
#define SW_EN_MASK (0x07)
#define SW_RECn() PORTAbits.RA3
#define SW_RECn_ASSERTED (0)
#define SW_RECn_RELEASED (1)

#define LED_REC(x) PORTBbits.RB6=x
#define LED_REC_ON  (1)
#define LED_REC_OFF (0)
/*
 * Interrupt vector handler
 */
void __interrupt() ISR(void)
{
    
}
/*
 * Initialize this PIC
 */
void PIC_Init(void)
{
    /* Disable all interrupt sources */
    INTCON = 0;
    PIE1 = 0;
    PIE2 = 0;
    
    /* Make all GPIOs inputs */
    TRISA = 0xFF;
    TRISB = 0xFF;
    TRISC = 0xFF;
    
    /* Make all GPIOs digital I/O */
    ADCON1 = 0x06;
    
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
}
/*
 * Function: PollSwitches
 * 
 * Description:
 * There are seven push button switches. When one of six is pressed 
 * a code is asserted on bits 0-2 of PORTA. The seventh switch is 
 * connected to bit 3 of PORTA.
 * 
 * Sample the hardware switches and return which one is pressed.
 * 
 * The logic of this implementation causes the "lower numbered" 
 * switches to have a higher priority. This means that when more 
 * than one switch is pressed a lower number switch assertion is
 * returned. For example if SW_2 and SW_3 are both pressed then 
 * SW_1 will be the state returned.
 * 
 * This is less than ideal but this is the way the hardware works.
 * 
 * This amplifier was designed 20 years ago in the U.K. so I expect 
 * a few more of these "Richards" to float up.
 */
typedef enum {SW_none, SW_1, SW_2, SW_3, SW_4, SW_5, SW_6, SW_REC} SelectSwitch_t;
SelectSwitch_t PollSwitches(void)
{
    SelectSwitch_t Result = SW_none;
    
    switch (SW_EN_PORT & SW_EN_MASK)
    {
        case 0:
            Result = SW_1;      /* disc */
            break;
        case 1:
            Result = SW_2;      /* video */
            break;
        case 2:
            Result = SW_3;      /* cd */
            break;
        case 3:
            Result = SW_4;      /* a.v. */
            break;
        case 4:
            Result = SW_5;      /* tuner */
            break;
        case 5:
            Result = SW_6;      /* tape */
            break;
        default:
            break;
    }
    
    if(Result == SW_none)
    {
        if(SW_RECn() == SW_RECn_ASSERTED)
        {
            Result = SW_REC;    /* record */
        }
    }
    
    return Result;
}
/*
 * Main application
 */
void main(void) 
{
    /*
     * Initialize main application
     */
    PIC_Init();
    
    /* Set GPIO directions for S21 */
    TRISB = 0b10000000;
    TRISC = 0b01100000;
    /*
     * Application process loop
     */
    while(1)
    {
        PollSwitches();
        /*
         * Initial debug code to check that we have 
         * the project and tools working together.
         * 
         * The expected behavior is the (mute) light should be on.
         * The (record) light should flash 2 times per second.
         * 
         * This code will be used later to indicate a critical fault in the application .
         */
        LED_REC(LED_REC_ON);
        __delay_ms(250);
        LED_REC(LED_REC_OFF);
        __delay_ms(250);
    }
}
