#define __AVR_ATtiny13A__
#define F_CPU 1200000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

/*
 * TODO:
 * Battery LED shold be turned on for some time
 * Disable ADC before sleep
 */

//Freq CPUio 9.6 MHz/8=1.2MHz

//Resistor divider Up = 680k and Down = 200k
#define BATTERY_LOW_LIMIT       768 //The lowest voltage for battery = 3.4 Volts
#define SOIL_LOW_LIMIT          800 //The lower, the more dusty soil is

#define WATER_PUMP_TIME_MAX     1000 //How much time water pump os on in milliseconds

#define UPDATE_TIME             3 //In seconds*8. Should be divided by 8

uint8_t waterPumpFlag = 0;
uint8_t goToSleepFlag = 0;
volatile uint16_t waterPumpTime = 0;

uint8_t volatile WDT8sCounter = 0; // Varible will be incremented in WDG timer

#define DEBUG_LED_ON        PORTB |= (1<<PORTB0)
#define DEBUG_LED_OFF       PORTB &= ~(1<<PORTB0)

void toggleDebugLED()
{
    if (PORTB & 1<<PORTB0) PORTB &= ~(1<<PORTB0);
    else PORTB |= 1<<PORTB0;
}

inline void configuration()
{
    cli();
    DDRB = (1<<PIN0)|(1<<PIN1)|(1<<PIN3);
    //Pull-up for button NEED CORRECT FUSE
    PORTB = (1<<PORTB5);

    MCUSR &= ~(1<<WDRF);
    WDTCR = (1<<WDCE);
    //Watchdog interrupt each 8s
    //WDTCR |= (1<<WDP3)|(0<<WDP2)|(0<<WDP1)|(1<<WDP0);
    WDTCR |= (0<<WDP3)|(1<<WDP2)|(1<<WDP1)|(0<<WDP0); //1 ms for debug
    //Enable WD interrupt
    WDTCR |= (1<<WDTIE);
    WDTCR &= ~(1<<WDE);

    //ADC internal reference, right adjust PB2 (soil sensor)
    ADMUX = (1<<REFS0)|(0<<ADLAR)|(0<<MUX1)|(1<<MUX0);
    //Enable ADC, Prescaler 128
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);

    //Timer for 1 ms CTC Mode
    //Enable interrupt on Output Compare Match A
    TIMSK0=(1<<OCIE0A);
    TCCR0A=(1<<WGM01)|(0<<WGM00);
    //Prescaler 8
    TCCR0B=(1<<CS01);
    TCNT0=0;
    OCR0A=150;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sei();
}

void blink(uint8_t n)
{
    for (uint8_t i=0; i<n; i++)
    {
        _delay_ms(100);
        DEBUG_LED_ON;
        _delay_ms(100);
        DEBUG_LED_OFF;
    }
}

uint16_t getADCResult()
{
    //Start convertion
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & 1<<ADSC) {blink(1);} //Wait util conversion is complete
    uint16_t data = 0;
    data = (ADCH<<8)| ADCL;
    return data;
}

/*
 * Checking battery voltage. If low, light up LED on PB0
 */
void batteryCheck()
{
    uint16_t batVoltage = 0;
    ADMUX |= (1<<MUX0);
    ADMUX &= ~(1<<MUX1);
    batVoltage = getADCResult();
    //Debug LED LOL
    /*
    if (batVoltage<BATTERY_LOW_LIMIT) PORTB |= (1<<PORTB0);
    else PORTB &= (~1<<PORTB0);*/ 
}

/*
 * Check voltage on resistor divider
 */
void soilSensorCheck()
{
    PORTB |= (1<<PORTB1); //Open transistor for sensor
    uint16_t soilVoltage = 0;
    ADMUX |= (1<<MUX1);
    ADMUX &= ~(1<<MUX0);
    soilVoltage = getADCResult();
    if (soilVoltage>SOIL_LOW_LIMIT) 
    {
        waterPumpFlag=1;
        blink(2);
    }
    else blink(1);
    //PORTB &= ~(1<<PORTB1); //Close transistor
}

int main()
{
    configuration();
    while(1)
    {
        if (WDT8sCounter>UPDATE_TIME)
        {
            batteryCheck();
            soilSensorCheck();
            cli();
            WDT8sCounter=0;
            sei();
        }
        if(waterPumpFlag)
        {
            PORTB |= (1<<PORTB3); //Turn on water pump
            if(waterPumpTime>=WATER_PUMP_TIME_MAX)
            {
                PORTB &= !(1<<PORTB3);
                cli();
                waterPumpFlag=0;
                waterPumpTime=0;
                goToSleepFlag=1;
                sei();
            }
        }
        else goToSleepFlag = 1;
        if(goToSleepFlag) 
        {
            DEBUG_LED_ON;
            cli();
            sleep_enable();
            sei();
            sleep_cpu();
            goToSleepFlag = 0;
            DEBUG_LED_OFF;
        }
    }
    return 0;
}

ISR(TIM0_COMPA_vect)
{
    cli();
    if(waterPumpFlag) waterPumpTime++;
    else waterPumpTime = 0;
    sei();
}

ISR(WDT_vect)
{
    cli();
    WDT8sCounter++;
    sei();
}