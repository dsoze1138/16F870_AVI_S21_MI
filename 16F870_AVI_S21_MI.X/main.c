/*
 * File:   main.c
 * Author: dan1138
 * Target: PIC16F870, PIC16F876A
 * Compiler: XC8 v2.31
 * IDE: MPLABX v5.45
 *
 * Created on June 24, 2021, 5:40 PM
 * 
 * Description:
 * 
 *      Front panel controller and infrared receiver decoder.
 * 
 *                      PIC16F870/PIC16F876A
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
 * 
 *  This is how the user interactions is implemented:
 * 
 *      There are seven normally open push buttons that are
 *      used to select one of six audio sources (disc) (video) 
 *      (cd) (a.v.) (tuner) (tape), for the amplifier and one 
 *      of five (disc) (video) (cd) (a.v.) (tuner) audio sources 
 *      for the tape recorder.
 * 
 *      At power start all inputs sources are off and the (mute) 
 *      is enabled. Pressing a source select button, (disc) (video) 
 *      (cd) (a.v.) (tuner) (tape) will select that source as the 
 *      amplifier input. Once an amplifier source is selected 
 *      another press of that source select button will toggle the 
 *      (mute) function.
 * 
 *      Pressing the (record) button will toggle the (record) indicator 
 *      for one of five (disc) (video) (cd) (a.v.) (tuner) inputs 
 *      and the (record) mode indicator.
 * 
 *      When in (record) mode pressing the (tape) input selector 
 *      will toggle between the (tape) output and the source selected
 *      for the tape recorder input as the source for the amplifier.
 * 
 *  Notes:
 * 
 *      As of 2021-JULY-3 there is no decoder implementation for 
 *      any infrared (IR) remote control transmitter.
 * 
 *      When a suitable IR transmitter has been selected and 
 *      the transmitter codes assigned to amplifier functions 
 *      for input selection, (record) mode, (mute), (volume) up 
 *      and (volume) down then the volume motor drive logic 
 *      can be implemented.
 * 
 *      The volume motor drive circuit is vulnerable to damage 
 *      when the (VOL+) and (VOL-) drive signals are high 
 *      at the same time. Any implementation must avoid 
 *      this condition.
 * 
 *      There may be enough buttons on the IR transmitter to 
 *      implement a less complex method to select between the
 *      the tape output and audio source when in (record) mode.
 */

#pragma config FOSC = XT        // Oscillator Selection bits (XT oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable bit (BOR disabled)
#pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

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

#define LED_REC_TOGGLE() PORTB^=(1<<6)
#define LED_MUTEn_TOGGLE() PORTC^=(1<<7)
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
    uint8_t LED_RecToggleCount;
    SelectSwitch_t SW_Sample = SW_none;
    SelectSwitch_t SW_Stable = SW_none;
    uint8_t SW_Changed = 0;
    uint8_t SW_BounceCount = 0;
    
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
        /* sample switch inputs */
        SW_Sample = PollSwitches();
        /* did switch state change */
        if(SW_Sample != SW_Stable)
        {
            SW_Stable = SW_Sample;
            SW_BounceCount = 20;
        }
        /* has the switch been pressed for 20 milliseconds */
        if(SW_BounceCount)
        {
            SW_BounceCount--;
            if(SW_BounceCount == 0)
            {
                SW_Changed = 1;
            }            
        }
        /* process a switch state change */
        if(SW_Changed) 
        {
            switch (SW_Stable)
            {
                case SW_1:      /* disc */
                    if(PORTB & (1<<0)) LED_MUTEn_TOGGLE();
                    PORTB &= (1<<0)|(1<<6);
                    PORTB |= (1<<0);
                    break;
                case SW_2:      /* video */
                    if(PORTB & (1<<1)) LED_MUTEn_TOGGLE();
                    PORTB &= (1<<1)|(1<<6);
                    PORTB |= (1<<1);
                    break;
                case SW_3:      /* cd */
                    if(PORTB & (1<<2)) LED_MUTEn_TOGGLE();
                    PORTB &= (1<<2)|(1<<6);
                    PORTB |= (1<<2);
                    break;
                case SW_4:      /* a.v. */
                    if(PORTB & (1<<3)) LED_MUTEn_TOGGLE();
                    PORTB &= (1<<3)|(1<<6);
                    PORTB |= (1<<3);
                    break;
                case SW_5:      /* tuner */
                    if(PORTB & (1<<4)) LED_MUTEn_TOGGLE();
                    PORTB &= (1<<4)|(1<<6);
                    PORTB |= (1<<4);
                    break;
                case SW_6:      /* tape */
                    if(PORTBbits.RB6) /* if the record mode is active toggle between tape output and recode source as the input */
                    {
                        PORTB = (PORTB ^ (1<<5)) ^ (PORTC & 0b00011111); 
                    }
                    else /* else treat the tape selection like the other inputs */
                    {
                        if(PORTB & (1<<5)) LED_MUTEn_TOGGLE();
                        PORTB &= (1<<5)|(1<<6);
                        PORTB |= (1<<5);
                    }
                    break;
                default:
                    break;
            }
            if (SW_Stable == SW_REC)
            {
                LED_REC_TOGGLE();
            }
            
            /* 
             * On any switch press:
             *  if the button pressed is (SW1 to SW5 or SW_REC) then 
             *    if the record mode is on then
             *      if the input selected is not (tape) then
             *        select that input as the tape recorder input.
             *    else
             *      then turn off record mode.
             */
            if ((SW_Stable != SW_6) && (SW_Stable != SW_none))
            {
                if(PORTBbits.RB6)
                {
                    if ((PORTB & 0b00011111) != 0)
                    {
                        PORTC ^= ((PORTC ^ PORTB) & 0b00011111);
                    }
                }
                else 
                {
                    PORTC &= 0b11100000;
                }
            }
            SW_Changed = 0;
        }
        /*
         * This delay sets the minimum time
         * for one iteration of the process loop
         */
        __delay_ms(1);
    }
}
