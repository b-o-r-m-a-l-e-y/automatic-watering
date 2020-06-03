#define __AVR_ATtiny13__
#include <avr/io.h>
#include <avr/interrupt.h>


#define WATER_PUPM_PIN 3

#define BATTERY_MASK          (0<<MUX1)|(1<<MUX0)
#define SENSOR_MASK           (1<<MUX1)|(0<<MUX0)

//Resistor divider Up = 680k and Down = 150k
#define ADC_MAX 1024
#define BATTERY_LOW_LIMIT   768 //The lowest voltage for battery = 3.4 Volts
#define SOIL_LOW_LIMIT      800 //The lower, the more dusty soil is

uint8_t waterPumpFlag = 0;

void configuration()
{
    cli();
    DDRB = (1<<PIN0)|(1<<PIN1)|(1<<PIN3);
    //Pull-up for button NEED CORRECT FUSE
    PORTB = (1<<PORTB5);

    WDTCR = WDCE<<1;
    //Watchdog interrupt each 8s
    WDTCR |= (1<<WDP3)|(0<<WDP2)|(0<<WDP1)|(1<<WDP0);
    //Enable WD interrupt
    WDTCR |= (1<<WDTIE);

    //ADC internal reference, right adjust PB2 (soil sensor)
    ADMUX = (1<<REFS0)|(0<<ADLAR)|(0<<MUX1)|(1<<MUX0);
    //Enable ADC
    ADCSRA |= (1<<ADEN);
    ADCSRA &= ~(1<<ADIF);
    sei();
}

uint16_t getADCResult(uint8_t mask)
{
    ADMUX ^= mask;
    //Start convertion
    ADCSRA |= (1<<ADSC);
    while (!(ADCSRA & 1<<ADIF)); //Wait util conversion complete
    ADCSRA &= ~(1<<ADIF);
    uint16_t data = 0;
    data = (ADCH<<8)|ADCL;
    return data;
}

void batteryCheck()
{
    uint16_t batVoltage = 0;
    batVoltage = getADCResult(BATTERY_MASK);
    if (batVoltage<BATTERY_LOW_LIMIT) PORTB |= (1<<PORTB0);
    else PORTB &= (~1<<PORTB0);
}

void soilSensorCheck()
{
    uint16_t soilVoltage = 1024;
    soilVoltage = getADCResult(SENSOR_MASK);
    if (soilVoltage<SOIL_LOW_LIMIT) waterPumpFlag=1;
}

int main()
{
    configuration();
    while(1)
    {
        batteryCheck();        
    }
    return 0;
}