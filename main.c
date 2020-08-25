/* --------0.Project information--------------------
 * NRF24L01 Master
 * Protocol communication: SPI (SSI0)
 * Debug through LEDs
 * Send data to slave, band width 1 byte
 * Author : TRAN MINH THUAN
---------------------------------------------------*/

/* ------------------------------1.System requirement-------------------------------
 * 1.Create a simple SSI communication system send and receive 1 byte(SPI.c)
 * 2.Delay function using SYSTICK timer
 * 3.Monitor the system through LEDs
 * 4.Create a protocol to send data through a Master NRF24L01 to NRF24L01 slave
     using Enhanced ShockBurst with IRQ pin Falling edge interrupt
   5.System operating
     * Master - TX - Click Button->Send 0x01 then monitor IRQ Flags(TX_DS or RT_MAX)
     * Slave  - RX - Monitor IRQ Flag (RX_DR)
 * 6.Debugging Notes
6.1- Can't read the TX_DR and MAX_RT flags in GPIO (IRQ) interrupt
Solution: Change nrf24l01_CONFIG_DEFAULT_VAL 0x08 - 0x28 to disable TX_DR interrupt
-----------------------------------------------------------------------------------*/

/* -----------------2.Pre-processor Directives Section-------------------*/
#include "include.h"
//-----------------Debugging LEDs---------------------
#define LEDs_DATA_R    (*((volatile unsigned long *)0x40025038)) //LEDs Data Register Addressing
#define LEDs_PORT      SYSCTL_PERIPH_GPIOF
#define LEDs_BASE      GPIO_PORTF_BASE
#define RED_PIN        GPIO_PIN_1
#define BLUE_PIN       GPIO_PIN_2
#define GREEN_PIN      GPIO_PIN_3
//Color from mixing RBG LEDs
#define RED            0x02
#define GREEN          0x08
#define YELLOW         0x0A
#define BLUE           0x04
//-----------------Buttons-----------------------
#define BUTTON_PORT    SYSCTL_PERIPH_GPIOF
#define BUTTON_BASE    GPIO_PORTF_BASE
#define BUTTON_PIN     GPIO_PIN_4
//-----------------System States----------------------
#define NORMAL      0x00
#define WARNING     0x01
#define FAIL        0x02
//-----------------NRF24L01---------------------------
//                  CSN (Chip Not Select- active LOW)
#define CSN_PORT            SYSCTL_PERIPH_GPIOA
#define CSN_BASE            GPIO_PORTA_BASE
#define CSN_PIN             GPIO_PIN_3
//                  CSN (Chip Enable)
#define CE_PORT             SYSCTL_PERIPH_GPIOA
#define CE_BASE             GPIO_PORTA_BASE
#define CE_PIN              GPIO_PIN_6
//                  IRQ (Interrupt Request)
#define IRQ_PORT            SYSCTL_PERIPH_GPIOA
#define IRQ_BASE            GPIO_PORTA_BASE
#define IRQ_PIN             GPIO_PIN_7
#define IRQ_MASK            GPIO_INT_PIN_7
/*-------------------------------------------------------------------------------*/

/* -------------3. Global Declarations Section---------------------*/

//Global variable for the whole project
unsigned long Tick;
unsigned long Tick_monitor;

//Global variable for main.c
static unsigned int  State=0;           //System state
static unsigned char Data_NRF[50];      //Debugging NRF24L01 Registers
//Function declaration
void Monitor_Init(void);
void Monitor(void);
void NRF24L01_Init();
void Button_Init(void);
/*------------------------------------------------------------------*/

/* ---------------------------4. Subroutines Section--------------------------*/

/* SYSTICK Overflow Interrupt Handler
 * The SYSTICK timer automatically load value from RELOAD_R so there no need to update new value
 * Interrupt after each 1us
 * Input:  No
 * Output: No
 * Global variable affect:
                 - "Tick"
                 - "Tick_monitor"
*/
void Systick_ISR(){
    Tick++;             //Increase every 1 us corresponding to Reload value
    Tick_monitor++;
    Monitor();
};

/* NRF ISR Handler
 * Caused by NRF24L01 PIN (FALLING EDGE)
 * Interrupt when IRQ PIN CHANGE FROM HIGH  -> LOW
                     +MASTER: TX_DS     - Transmit complete
                              MAX_RT    - Fail to transmit
                     +SLAVE:  RX_DR     - Receive complete
*/
void NRF_ISRHandler()
{
    nrf24l01_irq_clear_all();                    //clear all interrupt flags in the 24L01
    nrf24l01_flush_rx();                         //Clear RX FIFO
    GPIOIntClear(IRQ_BASE,IRQ_MASK);             //Acknowledge interrupt
}

void main(void)
{
    //-----------------Initialize System------------------------
    SysCtlClockSet(SYSCTL_SYSDIV_2_5| SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); //80MHz
    Monitor_Init();                             //LEDs from port F  (PF1,2,3)
    Systick_Init();                             //1us interrupt
    SSI0_Init();                                //Master,8bit,2Mhz,Mode0
    NRF24L01_Init();                            //PA3-CSN,PA6-CE,PA7-ISR, TX EN_AA
    //----------------Interrupt Enable----------------------------
    IntMasterEnable();
    //----------------Infinite Loop-------------------------------
    while(true)
    {
    }
}

/* NRF24L01 Initialization
 * Function:
         1. Settings for Input pin (IRQ) and Output pins (CE,CSN)
         2. Setup for IRQ interrupt
         3. Setup NRF24L01 Registers
 * Input: No
 * Output: No
 * Change:
         - Change the IRQ Interrupt handler
         - Change another configure for NRF registers
*/
void NRF24L01_Init(){
    //-------1. Setup for GPIO (CSN,CE,IRQ Pins)-----------
    //------------------CE Pin-----------------------
    if(!SysCtlPeripheralReady(CE_PORT))           //Check Clock peripheral
    {
        SysCtlPeripheralEnable(CE_PORT);            //Enable if not ready (busy or disable)
        while (!SysCtlPeripheralReady(CE_PORT));  //Wait for peripheral ready
    }
    GPIOPinTypeGPIOOutput(CE_BASE, CE_PIN);         //CE OUTPUT
    //------------------CSN Pin-----------------------
    if(!SysCtlPeripheralReady(CSN_PORT))          //Check Clock peripheral
    {
        SysCtlPeripheralEnable(CSN_PORT);           //Enable if not ready (busy or disable)
        while (!SysCtlPeripheralReady(CSN_PORT)); //Wait for peripheral ready
    }
    GPIOPinTypeGPIOOutput(CSN_BASE, CSN_PIN);       //CSN OUTPUT
    //------------------IRQ Pin-----------------------
    if(!SysCtlPeripheralReady(IRQ_PORT))          //Check Clock peripheral
    {
        SysCtlPeripheralEnable(IRQ_PORT);           //Enable if not ready (busy or disable)
        while (!SysCtlPeripheralReady(IRQ_PORT)); //Wait for peripheral ready
    }
    GPIOPinTypeGPIOInput(IRQ_BASE, IRQ_PIN);        //IRQ INPUT
//    //----------------2. Setup IRQ Interrupt --------------
    GPIOIntTypeSet(IRQ_BASE, IRQ_PIN, GPIO_FALLING_EDGE); // NRF24L01 ISR Active "LOW"
    GPIOIntRegister(IRQ_BASE, NRF_ISRHandler);            // Assign interrupt handler
    GPIOIntEnable(IRQ_BASE, IRQ_MASK);                    // Enable interrupt
    //---------------3.Setup NRF Register----------------------
    nrf24l01_initialize_debug(true, 1, true); //RX,1 Byte, Enhanced ShockBurst
    nrf24l01_flush_rx();
    nrf24l01_flush_tx();
}

/* Monitor Debugging System Initialization
 * Function: Initialize I/O for LED pins
 * Input: No
 * Output: No
*/
void Monitor_Init(void)
{
        if(!SysCtlPeripheralReady(LEDs_PORT))        //Check Clock peripheral
        {
            SysCtlPeripheralEnable(LEDs_PORT);       //Enable if not ready (busy or disable)
            while(!SysCtlPeripheralReady(LEDs_PORT));//Wait for peripheral ready
        }
        GPIOPinTypeGPIOOutput(LEDs_BASE, RED_PIN|BLUE_PIN|GREEN_PIN);
}

/* Monitor Debugging System
 * Function: Show the status of the system
               Flash Green:       OK
               Flash Yellow:      Warning
               Flash Red:         Dangerous
               Red:               System stop
 * Input: No
 * Output: No
 * Change:
          - Change the color of LEDs
          - Add more system states
          - Change monitor time
 * Affect global variable:
          1. "State" - system state variable
          2. "Tick_monitor"
 */
void Monitor(void)
{
        if(Tick_monitor>=100000) //Check system each 0.1s
        {
           Tick_monitor=0;      //Reset so check again after 0.1s
           if(State==NORMAL)       LEDs_DATA_R^=BLUE;
           if(State==WARNING)      LEDs_DATA_R^=YELLOW;
           if(State==FAIL)         LEDs_DATA_R=RED;
        }
}
/*-------------------------------------------------------------------------------*/
