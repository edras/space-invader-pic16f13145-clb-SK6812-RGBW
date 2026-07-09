/**
 * Generated Pins header File
 * 
 * @file pins.h
 * 
 * @defgroup  pinsdriver Pins Driver
 * 
 * @brief This is generated driver header for pins. 
 *        This header file provides APIs for all pins selected in the GUI.
 *
 * @version Driver Version  3.0.0
*/

/*
© [2026] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/

#ifndef PINS_H
#define PINS_H

#include <xc.h>

#define INPUT   1
#define OUTPUT  0

#define HIGH    1
#define LOW     0

#define ANALOG      1
#define DIGITAL     0

#define PULL_UP_ENABLED      1
#define PULL_UP_DISABLED     0

// get/set IO_RA0 aliases
#define IO_RA0_TRIS                 TRISAbits.TRISA0
#define IO_RA0_LAT                  LATAbits.LATA0
#define IO_RA0_PORT                 PORTAbits.RA0
#define IO_RA0_WPU                  WPUAbits.WPUA0
#define IO_RA0_OD                   ODCONAbits.ODCA0
#define IO_RA0_ANS                  ANSELAbits.ANSA0
#define IO_RA0_SetHigh()            do { LATAbits.LATA0 = 1; } while(0)
#define IO_RA0_SetLow()             do { LATAbits.LATA0 = 0; } while(0)
#define IO_RA0_Toggle()             do { LATAbits.LATA0 = ~LATAbits.LATA0; } while(0)
#define IO_RA0_GetValue()           PORTAbits.RA0
#define IO_RA0_SetDigitalInput()    do { TRISAbits.TRISA0 = 1; } while(0)
#define IO_RA0_SetDigitalOutput()   do { TRISAbits.TRISA0 = 0; } while(0)
#define IO_RA0_SetPullup()          do { WPUAbits.WPUA0 = 1; } while(0)
#define IO_RA0_ResetPullup()        do { WPUAbits.WPUA0 = 0; } while(0)
#define IO_RA0_SetPushPull()        do { ODCONAbits.ODCA0 = 0; } while(0)
#define IO_RA0_SetOpenDrain()       do { ODCONAbits.ODCA0 = 1; } while(0)
#define IO_RA0_SetAnalogMode()      do { ANSELAbits.ANSA0 = 1; } while(0)
#define IO_RA0_SetDigitalMode()     do { ANSELAbits.ANSA0 = 0; } while(0)
// get/set IO_RA4 aliases
#define PB3_TRIS                 TRISAbits.TRISA4
#define PB3_LAT                  LATAbits.LATA4
#define PB3_PORT                 PORTAbits.RA4
#define PB3_WPU                  WPUAbits.WPUA4
#define PB3_OD                   ODCONAbits.ODCA4
#define PB3_ANS                  ANSELAbits.ANSA4
#define PB3_SetHigh()            do { LATAbits.LATA4 = 1; } while(0)
#define PB3_SetLow()             do { LATAbits.LATA4 = 0; } while(0)
#define PB3_Toggle()             do { LATAbits.LATA4 = ~LATAbits.LATA4; } while(0)
#define PB3_GetValue()           PORTAbits.RA4
#define PB3_SetDigitalInput()    do { TRISAbits.TRISA4 = 1; } while(0)
#define PB3_SetDigitalOutput()   do { TRISAbits.TRISA4 = 0; } while(0)
#define PB3_SetPullup()          do { WPUAbits.WPUA4 = 1; } while(0)
#define PB3_ResetPullup()        do { WPUAbits.WPUA4 = 0; } while(0)
#define PB3_SetPushPull()        do { ODCONAbits.ODCA4 = 0; } while(0)
#define PB3_SetOpenDrain()       do { ODCONAbits.ODCA4 = 1; } while(0)
#define PB3_SetAnalogMode()      do { ANSELAbits.ANSA4 = 1; } while(0)
#define PB3_SetDigitalMode()     do { ANSELAbits.ANSA4 = 0; } while(0)
// get/set IO_RB4 aliases
#define LED5_TRIS                 TRISBbits.TRISB4
#define LED5_LAT                  LATBbits.LATB4
#define LED5_PORT                 PORTBbits.RB4
#define LED5_WPU                  WPUBbits.WPUB4
#define LED5_OD                   ODCONBbits.ODCB4
#define LED5_ANS                  ANSELBbits.ANSB4
#define LED5_SetHigh()            do { LATBbits.LATB4 = 1; } while(0)
#define LED5_SetLow()             do { LATBbits.LATB4 = 0; } while(0)
#define LED5_Toggle()             do { LATBbits.LATB4 = ~LATBbits.LATB4; } while(0)
#define LED5_GetValue()           PORTBbits.RB4
#define LED5_SetDigitalInput()    do { TRISBbits.TRISB4 = 1; } while(0)
#define LED5_SetDigitalOutput()   do { TRISBbits.TRISB4 = 0; } while(0)
#define LED5_SetPullup()          do { WPUBbits.WPUB4 = 1; } while(0)
#define LED5_ResetPullup()        do { WPUBbits.WPUB4 = 0; } while(0)
#define LED5_SetPushPull()        do { ODCONBbits.ODCB4 = 0; } while(0)
#define LED5_SetOpenDrain()       do { ODCONBbits.ODCB4 = 1; } while(0)
#define LED5_SetAnalogMode()      do { ANSELBbits.ANSB4 = 1; } while(0)
#define LED5_SetDigitalMode()     do { ANSELBbits.ANSB4 = 0; } while(0)
// get/set IO_RC1 aliases
#define LED3_TRIS                 TRISCbits.TRISC1
#define LED3_LAT                  LATCbits.LATC1
#define LED3_PORT                 PORTCbits.RC1
#define LED3_WPU                  WPUCbits.WPUC1
#define LED3_OD                   ODCONCbits.ODCC1
#define LED3_ANS                  ANSELCbits.ANSC1
#define LED3_SetHigh()            do { LATCbits.LATC1 = 1; } while(0)
#define LED3_SetLow()             do { LATCbits.LATC1 = 0; } while(0)
#define LED3_Toggle()             do { LATCbits.LATC1 = ~LATCbits.LATC1; } while(0)
#define LED3_GetValue()           PORTCbits.RC1
#define LED3_SetDigitalInput()    do { TRISCbits.TRISC1 = 1; } while(0)
#define LED3_SetDigitalOutput()   do { TRISCbits.TRISC1 = 0; } while(0)
#define LED3_SetPullup()          do { WPUCbits.WPUC1 = 1; } while(0)
#define LED3_ResetPullup()        do { WPUCbits.WPUC1 = 0; } while(0)
#define LED3_SetPushPull()        do { ODCONCbits.ODCC1 = 0; } while(0)
#define LED3_SetOpenDrain()       do { ODCONCbits.ODCC1 = 1; } while(0)
#define LED3_SetAnalogMode()      do { ANSELCbits.ANSC1 = 1; } while(0)
#define LED3_SetDigitalMode()     do { ANSELCbits.ANSC1 = 0; } while(0)
// get/set IO_RC2 aliases
#define LED4_TRIS                 TRISCbits.TRISC2
#define LED4_LAT                  LATCbits.LATC2
#define LED4_PORT                 PORTCbits.RC2
#define LED4_WPU                  WPUCbits.WPUC2
#define LED4_OD                   ODCONCbits.ODCC2
#define LED4_ANS                  ANSELCbits.ANSC2
#define LED4_SetHigh()            do { LATCbits.LATC2 = 1; } while(0)
#define LED4_SetLow()             do { LATCbits.LATC2 = 0; } while(0)
#define LED4_Toggle()             do { LATCbits.LATC2 = ~LATCbits.LATC2; } while(0)
#define LED4_GetValue()           PORTCbits.RC2
#define LED4_SetDigitalInput()    do { TRISCbits.TRISC2 = 1; } while(0)
#define LED4_SetDigitalOutput()   do { TRISCbits.TRISC2 = 0; } while(0)
#define LED4_SetPullup()          do { WPUCbits.WPUC2 = 1; } while(0)
#define LED4_ResetPullup()        do { WPUCbits.WPUC2 = 0; } while(0)
#define LED4_SetPushPull()        do { ODCONCbits.ODCC2 = 0; } while(0)
#define LED4_SetOpenDrain()       do { ODCONCbits.ODCC2 = 1; } while(0)
#define LED4_SetAnalogMode()      do { ANSELCbits.ANSC2 = 1; } while(0)
#define LED4_SetDigitalMode()     do { ANSELCbits.ANSC2 = 0; } while(0)
// get/set IO_RC3 aliases
#define PB4_TRIS                 TRISCbits.TRISC3
#define PB4_LAT                  LATCbits.LATC3
#define PB4_PORT                 PORTCbits.RC3
#define PB4_WPU                  WPUCbits.WPUC3
#define PB4_OD                   ODCONCbits.ODCC3
#define PB4_ANS                  ANSELCbits.ANSC3
#define PB4_SetHigh()            do { LATCbits.LATC3 = 1; } while(0)
#define PB4_SetLow()             do { LATCbits.LATC3 = 0; } while(0)
#define PB4_Toggle()             do { LATCbits.LATC3 = ~LATCbits.LATC3; } while(0)
#define PB4_GetValue()           PORTCbits.RC3
#define PB4_SetDigitalInput()    do { TRISCbits.TRISC3 = 1; } while(0)
#define PB4_SetDigitalOutput()   do { TRISCbits.TRISC3 = 0; } while(0)
#define PB4_SetPullup()          do { WPUCbits.WPUC3 = 1; } while(0)
#define PB4_ResetPullup()        do { WPUCbits.WPUC3 = 0; } while(0)
#define PB4_SetPushPull()        do { ODCONCbits.ODCC3 = 0; } while(0)
#define PB4_SetOpenDrain()       do { ODCONCbits.ODCC3 = 1; } while(0)
#define PB4_SetAnalogMode()      do { ANSELCbits.ANSC3 = 1; } while(0)
#define PB4_SetDigitalMode()     do { ANSELCbits.ANSC3 = 0; } while(0)
// get/set IO_RC4 aliases
#define PB1_TRIS                 TRISCbits.TRISC4
#define PB1_LAT                  LATCbits.LATC4
#define PB1_PORT                 PORTCbits.RC4
#define PB1_WPU                  WPUCbits.WPUC4
#define PB1_OD                   ODCONCbits.ODCC4
#define PB1_ANS                  ANSELCbits.ANSC4
#define PB1_SetHigh()            do { LATCbits.LATC4 = 1; } while(0)
#define PB1_SetLow()             do { LATCbits.LATC4 = 0; } while(0)
#define PB1_Toggle()             do { LATCbits.LATC4 = ~LATCbits.LATC4; } while(0)
#define PB1_GetValue()           PORTCbits.RC4
#define PB1_SetDigitalInput()    do { TRISCbits.TRISC4 = 1; } while(0)
#define PB1_SetDigitalOutput()   do { TRISCbits.TRISC4 = 0; } while(0)
#define PB1_SetPullup()          do { WPUCbits.WPUC4 = 1; } while(0)
#define PB1_ResetPullup()        do { WPUCbits.WPUC4 = 0; } while(0)
#define PB1_SetPushPull()        do { ODCONCbits.ODCC4 = 0; } while(0)
#define PB1_SetOpenDrain()       do { ODCONCbits.ODCC4 = 1; } while(0)
#define PB1_SetAnalogMode()      do { ANSELCbits.ANSC4 = 1; } while(0)
#define PB1_SetDigitalMode()     do { ANSELCbits.ANSC4 = 0; } while(0)
// get/set IO_RC5 aliases
#define PB2_TRIS                 TRISCbits.TRISC5
#define PB2_LAT                  LATCbits.LATC5
#define PB2_PORT                 PORTCbits.RC5
#define PB2_WPU                  WPUCbits.WPUC5
#define PB2_OD                   ODCONCbits.ODCC5
#define PB2_ANS                  ANSELCbits.ANSC5
#define PB2_SetHigh()            do { LATCbits.LATC5 = 1; } while(0)
#define PB2_SetLow()             do { LATCbits.LATC5 = 0; } while(0)
#define PB2_Toggle()             do { LATCbits.LATC5 = ~LATCbits.LATC5; } while(0)
#define PB2_GetValue()           PORTCbits.RC5
#define PB2_SetDigitalInput()    do { TRISCbits.TRISC5 = 1; } while(0)
#define PB2_SetDigitalOutput()   do { TRISCbits.TRISC5 = 0; } while(0)
#define PB2_SetPullup()          do { WPUCbits.WPUC5 = 1; } while(0)
#define PB2_ResetPullup()        do { WPUCbits.WPUC5 = 0; } while(0)
#define PB2_SetPushPull()        do { ODCONCbits.ODCC5 = 0; } while(0)
#define PB2_SetOpenDrain()       do { ODCONCbits.ODCC5 = 1; } while(0)
#define PB2_SetAnalogMode()      do { ANSELCbits.ANSC5 = 1; } while(0)
#define PB2_SetDigitalMode()     do { ANSELCbits.ANSC5 = 0; } while(0)
/**
 * @ingroup  pinsdriver
 * @brief GPIO and peripheral I/O initialization
 * @param none
 * @return none
 */
void PIN_MANAGER_Initialize (void);

/**
 * @ingroup  pinsdriver
 * @brief Interrupt on Change Handling routine
 * @param none
 * @return none
 */
void PIN_MANAGER_IOC(void);


#endif // PINS_H
/**
 End of File
*/