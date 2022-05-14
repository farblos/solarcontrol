#include <DallasTemperature.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <RTClib.h>
#include <SD.h>
#include <Wire.h>

#define a_umlaut                "\xe1"
#define o_umlaut                "\xef"
#define u_umlaut                "\xf5"
#define s_sharp                 "\xe2"
#define degree                  "\xdf"

#define LIGHT_SENSOR_INPUT      A0
#define PUMP_BUTTON_INPUT       2
#define ONE_WIRE_BUS            6
#define PUMP_RELAIS_N_OUTPUT    7
#define PUMP_RELAIS_L_OUTPUT    8
#define AUX_RELAIS_OUTPUT       9

// Sensor 1: 28AA  ACDB  3C14  01E6 (Vorlauf)
// Sensor 2: 28AA  E912  3D14  01DC (Rücklauf)
// Sensor 3: 28AA  3311  3D14  01B7 (Speicher)

OneWire onewire( ONE_WIRE_BUS );

DallasTemperature sensors( &onewire );

LiquidCrystal_I2C lcd( 0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE );

RTC_DS1307 rtc;

bool logp;

volatile bool pumpp;

void onbutton()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  // if interrupts come faster than one second, assume it is a
  // bounce and ignore
  if ( interrupt_time - last_interrupt_time > 1000 ) {
    pumpp = ! pumpp;
    last_interrupt_time = interrupt_time;
  }
}

void csvline( int temp1, int temp2, int temp3, int light, bool pumpp )
{
  File dataFile = SD.open( "log.csv", FILE_WRITE );
  if ( dataFile ) {
    dataFile.print( rtc.now().unixtime() );
    dataFile.print( "," );
    dataFile.print( temp1 );
    dataFile.print( "," );
    dataFile.print( temp2 );
    dataFile.print( "," );
    dataFile.print( temp3 );
    dataFile.print( "," );
    dataFile.print( light );
    dataFile.print( "," );
    dataFile.print( pumpp );
    dataFile.println();
    dataFile.close();
  }
}

void setup()
{
  sensors.begin();
  sensors.setResolution( 12 );

  lcd.begin( 16,2 );

  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  SD.begin();
  logp = false;

  // initialize pump state and pump relais MOS-FET
  pinMode( AUX_RELAIS_OUTPUT, OUTPUT );
  pumpp = false;
  digitalWrite( AUX_RELAIS_OUTPUT, HIGH );

  // activate internal pull-up on pump button and attach
  // interrupt to it
  pinMode( PUMP_BUTTON_INPUT, INPUT_PULLUP );
  attachInterrupt( digitalPinToInterrupt( PUMP_BUTTON_INPUT ),
                   onbutton, FALLING );

  // write initialization marker to CSV
  csvline( 0, 0, 0, 0, 0 );
}

void loop()
{
  sensors.requestTemperatures();
  int temp1 = sensors.getTempCByIndex( 0 ) * 100.0;
  int temp2 = sensors.getTempCByIndex( 1 ) * 100.0;
  int temp3 = sensors.getTempCByIndex( 2 ) * 100.0;

  int light = analogRead( LIGHT_SENSOR_INPUT );

  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( "V:" );
  lcd.print( temp1 );
  lcd.setCursor( 7, 0 );
  lcd.print( "R:" );
  lcd.print( temp2 );
  lcd.setCursor( 0, 1 );
  lcd.print( "S:" );
  lcd.print( temp3 );
  lcd.setCursor( 7, 1 );
  lcd.print( "L:" );
  lcd.print( light );
  lcd.setCursor( 15, 1 );
  lcd.print( pumpp );

  // start logging only when temperature readings differ
  if ( (temp1 != temp2) || (temp2 != temp3) || (temp1 != temp3) ) {
    logp = true;
  }
  if ( logp ) {
    csvline( temp1, temp2, temp3, light, pumpp );
  }

  if ( pumpp ) {
    digitalWrite( AUX_RELAIS_OUTPUT, LOW );
  }
  else {
    digitalWrite( AUX_RELAIS_OUTPUT, HIGH );
  }

  delay( 1000 );
}

// Local Variables:
// mode: c++
// End:
