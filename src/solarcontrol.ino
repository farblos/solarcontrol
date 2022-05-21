//
// solarcontrol.ino - new life for an old solar hot water system.
//

#include <DallasTemperature.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <RTClib.h>
#include <SdFat.h>
#include <Wire.h>
#include <avr/wdt.h>

#ifdef DEBUG
#define debug( message ) Serial.println( message );
#else
#define debug( message )
#endif

//{{{ miscellaneous constants

// main loop delay in ms per cycle.  This should be small enough
// to not trigger the watchdog timer in regular operation.
#define MAIN_DELAY              1000

// debounce delay in ms
#define DEBOUNCE_DELAY          200

// light levels to accumulate before starting pump
#define INIT_LIGHT_LEVEL        3000

// number of cycles to wait before checking supply flow
// temperature when pumping
#define INIT_PUMP_CYCLES        150

// minimum temperature difference between supply flow temperature
// and tank temperature in centidegrees Celsius to keep the pump
// working
#define TEMP_DELTA              500

//}}}

//{{{ physical pin-out

// in addition to the pins mentioned below use:
//
// - A4, A5  for LCD and RTC access on I2C
//
// - 10 - 13 for SD card access on SPI
//
// The control button must be on a pin that can trigger
// interrupts.
#define INPUT_LIGHT_SENSOR      A0
#define INPUT_CONTROL_BUTTON    2
#define INOUT_ONE_WIRE          5
#define OUTPUT_ERROR_LED        6
#define OUTPUT_PUMP_RELAIS_N    7
#define OUTPUT_PUMP_RELAIS_L    8
#define OUTPUT_AUX_RELAIS       9

//}}}

//{{{ subsystem and error constants and variables

#define SUBSYS_SENSORS          1
#define SUBSYS_LCD              2
#define SUBSYS_RTC              4
#define SUBSYS_SD               8

#define fail( subsys ) (error |= subsys)

#define okp( subsys )  (! (error & subsys))

byte error = 0;

//}}}

//{{{ control button items

volatile bool cbtnp = false;

unsigned long lits = 0;                         // last-interrupt-timestamp

void ocbtnp()                                   // on-control-button-pressed
{
  // ignore interrupts coming too fast to be real button presses
  unsigned long its = millis();
  if ( its - lits > DEBOUNCE_DELAY ) {
    cbtnp = true;
    lits  = its;
  }
}

//}}}

//{{{ state items

#define STATE_STARTING          '^'
#define STATE_ERROR             '!'
#define STATE_FORCE_ON          '1'
#define STATE_FORCE_OFF         '0'
#define STATE_PUMPING           '@'
#define STATE_WAITING           '*'

char state;
bool ledon;
int  ltlvl;
int  pumpc;

// sets the current processing state to the specified new state.
// Initializes variables related to the new state.  The new state
// must be different from the current state.
void setstate( char newstate )
{
  state = newstate;

  switch ( state ) {
  case STATE_ERROR:
    ledon = true;
    break;

  case STATE_PUMPING:
    pumpc = INIT_PUMP_CYCLES;
    break;

  case STATE_WAITING:
    ltlvl = INIT_LIGHT_LEVEL;
    break;
  }
}

//}}}

//{{{ sensor items

// supply flow, return flow, tank sensors
const DeviceAddress SENSOR_SUPPLY = { 0x28, 0xaa, 0xac, 0xdb, 0x3c, 0x14, 0x01, 0xe6 };
const DeviceAddress SENSOR_RETURN = { 0x28, 0xaa, 0xe9, 0x12, 0x3d, 0x14, 0x01, 0xdc };
const DeviceAddress SENSOR_TANK   = { 0x28, 0xaa, 0x33, 0x11, 0x3d, 0x14, 0x01, 0xb7 };

OneWire onewire( INOUT_ONE_WIRE );

DallasTemperature sensors( &onewire );

// returns temperature reading of the specified sensor
int getTemp( const DeviceAddress sensor )
{
  int raw = sensors.getTemp( sensor );
  if ( raw > DEVICE_DISCONNECTED_RAW ) {
    return (float)raw * 0.78125;
  }
  else {
    fail( SUBSYS_SENSORS );
    return -1000;
  }
}

//}}}

//{{{ LCD items

#define LCD_I2C_ADDRESS         0x27

#define LCD_CHAR_A_UMLAUT       "\xe1"
#define LCD_CHAR_O_UMLAUT       "\xef"
#define LCD_CHAR_U_UMLAUT       "\xf5"
#define LCD_CHAR_S_SHARP        "\xe2"
#define LCD_CHAR_DEGREE         "\xdf"

LiquidCrystal_I2C lcd( LCD_I2C_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE );

//}}}

//{{{ RTC items

RTC_DS1307 rtc;

//}}}

//{{{ SD card items

#define LOG_FILE_MAXSIZE        10485760
#define LOG_FILE_SYNC_DELTA     1024

// log directory name.  Create log directory structure as
// follows:
//
//   rm -rf /media/usd00/solarlog
//   mkdir  /media/usd00/solarlog
//   for i in 9 8 7 6 5 4 3 2 1 0; do
//     test $i == 0 && sleep 2
//     touch /media/usd00/solarlog/log$i.csv
//   done
const char LOG_DIR_NAME[] = "/solarlog";

// log file name, which is calculated from the log file template
// and the specified log file index
#define lfn( lfidx ) (LOG_FILE_TEMPLATE[3] = '0' + lfidx, LOG_FILE_TEMPLATE)
char LOG_FILE_TEMPLATE[] = "log0.csv";

SdFat    sd;
SdFile   lfile;
byte     lfidx;
uint32_t lfssz;

void rtc2sd( uint16_t* date, uint16_t* time )
{
  DateTime now = rtc.now();
  *date = FAT_DATE( now.year(), now.month(),  now.day() );
  *time = FAT_TIME( now.hour(), now.minute(), now.second() );
}

//}}}

//{{{ updtlog

// write the specified sensor data and the current state to the
// current log file.  Rotate or sync log files as needed.
void updtlog( int temps, int tempr, int tempt, int light )
{
  if ( (! okp( SUBSYS_SD )) )
    return;

  lfile.print( rtc.now().unixtime() );
  lfile.print( "," );
  lfile.print( state );
  lfile.print( "," );
  lfile.print( temps );
  lfile.print( "," );
  lfile.print( tempr );
  lfile.print( "," );
  lfile.print( tempt );
  lfile.print( "," );
  lfile.print( light );
  lfile.print( "," );
  lfile.print( (state == STATE_WAITING) ? ltlvl : 0 );
  lfile.print( "," );
  lfile.print( (state == STATE_PUMPING) ? pumpc : 0 );
  lfile.print( "," );
  lfile.print( error );
  lfile.println();

  if ( lfile.getWriteError() ) {
    lfile.clearWriteError();
    fail( SUBSYS_SD );
    debug( "Cannot write to log file." );
  }

  uint32_t lfsz = lfile.fileSize();
  if ( lfsz > LOG_FILE_MAXSIZE ) {
    // switch to next log file
    if ( (! lfile.close() ) ) {
      fail( SUBSYS_SD );
      debug( "Cannot close log file after writing." );
    }
    if ( lfidx == 9 )
      lfidx = 0;
    else
      lfidx++;
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.open( lfn( lfidx ), O_WRITE | O_TRUNC | O_APPEND ) ) ) {
      fail( SUBSYS_SD );
      debug( "Cannot open next log file for writing." );
    }
    lfssz = 0;
  }
  else if ( (lfsz - lfssz) > LOG_FILE_SYNC_DELTA ) {
    // sync log file if unsynched delta gets too large
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.sync() ) ) {
      fail( SUBSYS_SD );
      debug( "Cannot sync log file." );
    }
    lfssz = lfsz;
  }

  // try to close the log file as a last resort in case of errors
  if ( (! okp( SUBSYS_SD )) ) {
    lfile.close();
  }
}

//}}}

//{{{ updtlcd

// (incomplete)
void updtlcd( int temps, int tempr, int tempt, int light )
{
  if ( (! okp( SUBSYS_LCD )) )
    return;

  lcd.setCursor( 0, 0 );
  lcd.print( "V" );
  lcd.print( temps );
  lcd.print( " " );
  lcd.print( "R" );
  lcd.print( tempr );
  lcd.setCursor( 15, 0 );
  lcd.print( state );
  lcd.setCursor( 0, 1 );
  lcd.print( "S" );
  lcd.print( tempt );
  lcd.print( " " );
  lcd.print( "L" );
  lcd.print( light );
  lcd.print( " " );
  lcd.print( " " );
  lcd.setCursor( 15, 1 );
  lcd.print( error );
}

//}}}

//{{{ setup

void setup()
{
#ifdef DEBUG
  // initialize serial line and wait for input on it
  Serial.begin( 9600 );
  while ( ! Serial.available() ) {
  }
#endif

  // initialize state
  state = STATE_STARTING;

  // initialize temperature sensors
  sensors.begin();
  if ( (sensors.isConnected( SENSOR_SUPPLY )) &&
       (sensors.isConnected( SENSOR_RETURN )) &&
       (sensors.isConnected( SENSOR_TANK   )) ) {
    sensors.setResolution( 12 );
  }
  else {
    fail( SUBSYS_SENSORS );
    debug( "Cannot initialize sensor." );
  }

  // initialize LCD
  Wire.begin();
  Wire.beginTransmission( LCD_I2C_ADDRESS );
  if ( Wire.endTransmission() == 0 ) {
    lcd.begin( 16, 2 );
    lcd.noBacklight();
  }
  else {
    fail( SUBSYS_LCD );
    debug( "Cannot initialize LCD." );
  }

  // initialize RTC
  if ( (rtc.begin()) ) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  else {
    fail( SUBSYS_RTC );
    debug( "Cannot initialize RTC." );
  }

  // initialize SD card and change to log directory
  SdFile::dateTimeCallback( rtc2sd );
  if ( (okp( SUBSYS_SD )) &&
       (! sd.begin( SS, SPI_HALF_SPEED )) ) {
    fail( SUBSYS_SD );
    debug( "Cannot initialize SD." );
  }
  if ( (okp( SUBSYS_SD )) &&
       (! sd.chdir( LOG_DIR_NAME )) ) {
    fail( SUBSYS_SD );
    debug( "Cannot change to log directory." );
  }

  // ensure all log files exist and determine most recent one
  uint16_t mrpdate = 0;
  uint16_t mrptime = 0;
  byte     mrlfidx = 0;
  for ( byte i = 0; i < 10; i++ ) {
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.open( lfn( i ), O_READ )) ) {
      fail( SUBSYS_SD );
      debug( "Cannot open log file for reading." );
    }
    uint16_t pdate = 0;
    uint16_t ptime = 0;
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.getModifyDateTime( &pdate, &ptime )) ) {
      fail( SUBSYS_SD );
      debug( "Cannot determine modification timestamp." );
    }
    if ( (! lfile.close() ) ) {
      fail( SUBSYS_SD );
      debug( "Cannot close log file after reading." );
    }
    if ( (pdate > mrpdate) ||
         ((pdate == mrpdate) && (ptime > mrptime)) ) {
      mrpdate = pdate;
      mrptime = ptime;
      mrlfidx = i;
    }
  }

  // open newest log file for appending
  lfidx = mrlfidx;
  if ( (okp( SUBSYS_SD )) &&
       (! lfile.open( lfn( lfidx ), O_WRITE | O_APPEND )) ) {
    fail( SUBSYS_SD );
    debug( "Cannot open initial log file for writing." );
  }
  if ( (okp( SUBSYS_SD )) ) {
    lfssz = lfile.fileSize();
  }

  // activate internal pull-up on control button
  pinMode( INPUT_CONTROL_BUTTON, INPUT_PULLUP );

  // initialize error LED
  pinMode( OUTPUT_ERROR_LED, OUTPUT );
  digitalWrite( OUTPUT_ERROR_LED, LOW );

  // initialize relais MOS-FETs
  pinMode( OUTPUT_PUMP_RELAIS_N, OUTPUT );
  digitalWrite( OUTPUT_PUMP_RELAIS_N, LOW );
  pinMode( OUTPUT_PUMP_RELAIS_L, OUTPUT );
  digitalWrite( OUTPUT_PUMP_RELAIS_L, LOW );
  pinMode( OUTPUT_AUX_RELAIS, OUTPUT );
  digitalWrite( OUTPUT_AUX_RELAIS, LOW );

  // enable watchdog
  wdt_enable( WDTO_8S );
}

//}}}

//{{{ loop

void loop()
{
  sensors.requestTemperatures();
  int temps = getTemp( SENSOR_SUPPLY );
  int tempr = getTemp( SENSOR_RETURN );
  int tempt = getTemp( SENSOR_TANK );

  int light = analogRead( INPUT_LIGHT_SENSOR );

  // determine new state
  switch ( state ) {
  case STATE_STARTING:
    if ( error ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      // should not happen
      cbtnp = false;
    }
    else if ( (temps != tempr) || (tempr != tempt) ) {
      attachInterrupt( digitalPinToInterrupt( INPUT_CONTROL_BUTTON ),
                       ocbtnp, FALLING );
      updtlog( 0, 0, 0, 0 );
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_ERROR:
    cbtnp = false;
    ledon = ! ledon;
    break;

  case STATE_FORCE_ON:
    if ( error ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_WAITING );
    }
    break;

  case STATE_FORCE_OFF:
    if ( error ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_WAITING:
    if ( error ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_FORCE_OFF );
    }
    else if ( ltlvl > 0 ) {
      ltlvl -= light / 64;
      if ( ltlvl < 0 ) ltlvl = 0;
    }
    else {
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_PUMPING:
    if ( error ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_FORCE_ON );
    }
    else if ( pumpc > 0 ) {
      pumpc--;
    }
    else if ( temps - tempt < TEMP_DELTA ) {
      setstate( STATE_WAITING );
    }
    break;
  }

  // update display right away after start-up
  updtlcd( temps, tempr, tempt, light );

  // update log file only after start-up and if not in an error
  // state
  if ( (state != STATE_STARTING) && (state != STATE_ERROR) ) {
    updtlog( temps, tempr, tempt, light );
  }

  // operate error LED depending on state
  if ( (state == STATE_ERROR) && (ledon) ) {
    digitalWrite( OUTPUT_ERROR_LED, HIGH );
  }
  else {
    digitalWrite( OUTPUT_ERROR_LED, LOW );
  }

  // operate pump depending on state
  if ( (state == STATE_FORCE_ON) || (state == STATE_PUMPING) ) {
    digitalWrite( OUTPUT_AUX_RELAIS, HIGH );
  }
  else {
    digitalWrite( OUTPUT_AUX_RELAIS, LOW );
  }

  wdt_reset();

  delay( MAIN_DELAY );
}

//}}}

// Local Variables:
// mode: c++
// End:
