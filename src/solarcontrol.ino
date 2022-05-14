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

OneWire onewire( ONE_WIRE_BUS );

// Sensor 1: 28AA  ACDB  3C14  01E6 (Vorlauf)
// Sensor 2: 28AA  E912  3D14  01DC (Rücklauf)
// Sensor 3: 28AA  3311  3D14  01B7 (Speicher)
DallasTemperature sensors( &onewire );

LiquidCrystal_I2C lcd( 0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE );

RTC_DS1307 rtc;

#define STARTING  's'
#define FORCE_ON  '1'
#define FORCE_OFF '0'
#define PUMPING   'p'
#define WAITING   'w'

#define WAIT_INIT 3000
#define PUMP_INIT 150

volatile char state;
volatile int  waitc;
volatile int  pumpc;

void onbutton()
{
  static unsigned long lits = 0;
  unsigned long its = millis();

  // if interrupts come too fast assume that they are caused by
  // bounces and ignore them.  It seems that the delay in the
  // main loop does not flush (implicitly nor explicitly) pending
  // interrupts caused by bounces, so the timespan used here has
  // to be significantly larger than the delay.
  if ( its - lits > 10000 ) {
    switch ( state ) {
    case STARTING:
      break;

    case FORCE_ON:
      state = FORCE_OFF;
      break;

    case FORCE_OFF:
      state = PUMPING;
      pumpc = PUMP_INIT;
      break;

    case WAITING:
      state = FORCE_ON;
      break;

    case PUMPING:
      state = FORCE_OFF;
      break;
    }

    lits  = its;
  }
}

void csvline( int tempv, int tempr, int temps, int light )
{
  File dataFile = SD.open( "log.csv", FILE_WRITE );
  if ( dataFile ) {
    dataFile.print( rtc.now().unixtime() );
    dataFile.print( "," );
    dataFile.print( tempv );
    dataFile.print( "," );
    dataFile.print( tempr );
    dataFile.print( "," );
    dataFile.print( temps );
    dataFile.print( "," );
    dataFile.print( light );
    dataFile.print( "," );
    dataFile.print( state );
    dataFile.print( "," );
    dataFile.print( waitc );
    dataFile.print( "," );
    dataFile.print( pumpc );
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

  // activate internal pull-up on pump button and attach
  // interrupt to it
  pinMode( PUMP_BUTTON_INPUT, INPUT_PULLUP );
  attachInterrupt( digitalPinToInterrupt( PUMP_BUTTON_INPUT ),
                   onbutton, RISING );

  // initialize pump relais MOS-FET
  pinMode( AUX_RELAIS_OUTPUT, OUTPUT );
  digitalWrite( AUX_RELAIS_OUTPUT, HIGH );

  // initialize state
  state = STARTING;

  // write initialization marker to CSV
  csvline( 0, 0, 0, 0 );
}

void loop()
{
  sensors.requestTemperatures();
  int tempv = sensors.getTempCByIndex( 0 ) * 100.0;
  int tempr = sensors.getTempCByIndex( 1 ) * 100.0;
  int temps = sensors.getTempCByIndex( 2 ) * 100.0;

  int light = analogRead( LIGHT_SENSOR_INPUT );

  noInterrupts();
  switch ( state ) {
  case STARTING:
    if ( (tempv != tempr) || (tempr != temps) ) {
      state = PUMPING;
      pumpc = PUMP_INIT;
    }
    break;

  case FORCE_ON:
    break;

  case FORCE_OFF:
    break;

  case WAITING:
    if ( waitc > 0 ) {
      waitc -= light / 64;
    }
    else {
      state = PUMPING;
      pumpc = PUMP_INIT;
    }
    break;

  case PUMPING:
    if ( pumpc > 0 ) {
      pumpc--;
    }
    else if ( tempv - temps <= 500 ) {
      state = WAITING;
      waitc = WAIT_INIT;
    }
    break;
  }
  interrupts();

  // update display
  lcd.clear();
  lcd.setCursor( 0, 0 );
  lcd.print( "V:" );
  lcd.print( tempv );
  lcd.setCursor( 7, 0 );
  lcd.print( "R:" );
  lcd.print( tempr );
  lcd.setCursor( 15, 0 );
  lcd.print( state );
  lcd.setCursor( 0, 1 );
  lcd.print( "S:" );
  lcd.print( temps );
  lcd.setCursor( 7, 1 );
  lcd.print( "L:" );
  lcd.print( light );

  // start logging only after completing start-up
  if ( state != STARTING ) {
    csvline( tempv, tempr, temps, light );
  }

  if ( (state == FORCE_ON) || (state == PUMPING) ) {
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
