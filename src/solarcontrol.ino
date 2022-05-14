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
#define ONE_WIRE_BUS            2
#define PUMP_RELAIS_N_OUTPUT    7
#define PUMP_RELAIS_L_OUTPUT    8
#define AUX_RELAIS_OUTPUT       9

// Sensor 1 : 28AA  ACDB  3C14  01E6
// Sensor 2 : 28AA  E912  3D14  01DC
// Sensor 3 : 28AA  3311  3D14  01B7

OneWire onewire( ONE_WIRE_BUS );

DallasTemperature sensors( &onewire );

LiquidCrystal_I2C lcd( 0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE );

RTC_DS1307 rtc;

int thermometern = 0;

void setup() {
  sensors.begin();
  sensors.setResolution( 12 );
  thermometern = sensors.getDeviceCount();

  lcd.begin( 16,2 );

  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  SD.begin();
}

void loop() {
  sensors.requestTemperatures();
  int temp1 = sensors.getTempCByIndex( 0 ) * 100.0;
  int temp2 = sensors.getTempCByIndex( 1 ) * 100.0;
  int temp3 = sensors.getTempCByIndex( 2 ) * 100.0;

  int light = analogRead( LIGHT_SENSOR_INPUT );

  int epoch = rtc.now().unixtime();

  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( "1:" );
  lcd.print( temp1 );
  lcd.setCursor( 7, 0 );
  lcd.print( "2:" );
  lcd.print( temp2 );
  lcd.setCursor( 0, 1 );
  lcd.print( "3:" );
  lcd.print( temp3 );
  lcd.setCursor( 7, 1 );
  lcd.print( "L:" );
  lcd.print( light );

  File dataFile = SD.open( "log.csv", FILE_WRITE );
  if ( dataFile ) {
    dataFile.print( epoch );
    dataFile.print( "," );
    dataFile.print( temp1 );
    dataFile.print( "," );
    dataFile.print( temp2 );
    dataFile.print( "," );
    dataFile.print( temp3 );
    dataFile.print( "," );
    dataFile.print( light );
    dataFile.println();
    dataFile.close();
  }

  delay( 1000 );
}

// Local Variables:
// mode: c++
// End:
