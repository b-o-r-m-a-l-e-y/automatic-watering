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
#define BATTERY_LOW_LIMIT       140 //The lowest voltage for battery = 3.4 Volts
#define SOIL_LOW_LIMIT          105 //The lower, the more dusty soil is (120 for 5V)

#define WATER_PUMP_TIME_MAX     8000 //How much time water pump os on in milliseconds

#define UPDATE_TIME             3*60*60/8 //Once in 3 hours

uint8_t waterPumpFlag = 0;
uint8_t goToSleepFlag = 0;
volatile uint16_t waterPumpTime = 0;

uint16_t volatile WDT8sCounter = 1; // Varible will be incremented in WDG timer

#define DEBUG_LED_ON        PORTB |= (1<<PORTB0)
#define DEBUG_LED_OFF       PORTB &= ~(1<<PORTB0)
#define DEBUG_LED_TOGGGLE   PORTB ^= (1<<PORTB0)

inline void configuration()
{
    cli();
    DDRB = (1<<PIN0)|(1<<PIN1)|(1<<PIN3);
    //Pull-up for button NEED CORRECT FUSE
    PORTB = (1<<PORTB5);

    MCUSR &= ~(1<<WDRF);
    WDTCR = (1<<WDCE);
    //Watchdog interrupt each 8s
    WDTCR |= (1<<WDP3)|(0<<WDP2)|(0<<WDP1)|(1<<WDP0);
    //Enable WD interrupt
    WDTCR |= (1<<WDTIE);
    WDTCR &= ~(1<<WDE);

    //Disable digital input for PB2 and 4
    DIDR0 |= (1<<ADC2D)|(1<<ADC1D);
    //Enable ADC, Prescaler 128
    ADCSRA = (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
    //ADC internal reference, right adjust PB2 (soil sensor)
    ADMUX = (1<<REFS0)|(1<<ADLAR)|(0<<MUX1)|(1<<MUX0);
    ADCSRA = (1<<ADEN);

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
    while (ADCSRA & 1<<ADSC) {} //Wait util conversion is complete
    uint16_t data = 0;
    data = ADCH;
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
    
    if (batVoltage<BATTERY_LOW_LIMIT) blink(2);
    else PORTB &= (~1<<PORTB0);
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
    if (soilVoltage<SOIL_LOW_LIMIT) 
    {
        waterPumpFlag=1;
    }
    else blink(1);
    PORTB &= ~(1<<PORTB1); //Close transistor
}

int main()
{
    configuration();
    PORTB |= (1<<PORTB1); //Open transistor for sensor
    uint16_t soilVoltage = 0;
    ADMUX |= (1<<MUX1);
    ADMUX &= ~(1<<MUX0);
    soilVoltage = getADCResult();
    while(soilVoltage<SOIL_LOW_LIMIT)
    {
        soilVoltage = getADCResult();
        PORTB |= (1<<PORTB3); //Turn on water pump
    }
    PORTB &= ~(1<<PORTB3); //Turn off water pump
    PORTB &= ~(1<<PORTB1); //Close sensor transistor
    while(1)
    {
        if (WDT8sCounter>UPDATE_TIME)
        {
            batteryCheck();
            soilSensorCheck();
            blink(1);
            cli();
            WDT8sCounter=0;
            sei();
        }
        if(waterPumpFlag)
        {
            PORTB |= (1<<PORTB3); //Turn on water pump
            if(waterPumpTime>WATER_PUMP_TIME_MAX)
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
            //DEBUG_LED_ON;
            cli();
            sleep_enable();
            ADCSRA &= ~(1<<ADEN);
            sei();
            sleep_cpu();
            ADCSRA |= (1<<ADEN);
            waterPumpTime = 0;
            goToSleepFlag = 0;
            //DEBUG_LED_OFF;
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