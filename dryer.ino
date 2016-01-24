#include "PietteTech_DHT.h"

// system defines
#define DHTPIN   4        // Digital pin for communications
#define DHTTYPE  DHT22
#define DHT_SAMPLE_INTERVAL   5000
#define PUSHBULLET_NOTIF_HOME "pushbulletHOME"
#define AWS_EMAIL "awsEmail"

//declaration
void dht_wrapper();

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
unsigned int DHTnextSampleTime;  // Next time we want to start sample
bool bDHTstarted;                // flag to indicate we started acquisition
int n;                           // counter

//String to store the sensor temp
char resultstr[64];
//String to store the sensor humidity
char humiditystr[64];

char VERSION[64] = "0.12";
bool dryer_on = false;
String dryer_stat = "";
int humidity_samples_below_10 = 0;

void setup() {

  // Start the first sample immediately
  DHTnextSampleTime = 0;  
  Particle.variable("temp", resultstr, STRING);
  Particle.variable("humidity", humiditystr, STRING);
  Particle.variable("dryer_stat", dryer_stat);

  Particle.publish("Dryer - firmware version", VERSION, 60, PRIVATE);

  bool success = Particle.function("resetDryer", resetDryer);
  if (not success) {
    Particle.publish("ERROR", "Failed to register function resetDryer", 60, PRIVATE);
  }

  success = Particle.function("sendEmail", sendEmail);
  if (not success) {
    Particle.publish("ERROR", "Failed to register function sendEmail", 60, PRIVATE);
  }
 
}

//wrapper for the DHT lib
void dht_wrapper() {
    DHT.isrCallback();
}

void loop() {

  // Check if we need to start the next sample
  if (millis() > DHTnextSampleTime) {
      
    if (!bDHTstarted) {		// start the sample
      DHT.acquire();
      bDHTstarted = true;
    }

    if (!DHT.acquiring()) {		// has sample completed?

      float temp = (float)DHT.getCelsius();
      int temp1 = (temp - (int)temp) * 100;

      char tempInChar[32];
      sprintf(tempInChar,"%0d.%d", (int)temp, temp1);

      //google docs will get this variable - if you are storing the value in a google sheet
      sprintf(resultstr, "{\"t\":%s}", tempInChar);

      char humiInChar[32];
      float humid = (float)DHT.getHumidity();
      int humid1 = (humid - (int)humid) * 100;

      sprintf(humiInChar,"%0d.%d", (int)humid, humid1);

      //google docs will get this variable - if you are storing the value in a google sheet
      sprintf(humiditystr, "{\"t\":%s}", humiInChar);

      n++;  // increment counter
      bDHTstarted = false;  // reset the sample flag so we can take another
      DHTnextSampleTime = millis() + DHT_SAMPLE_INTERVAL;  // set the time for next sample

      //if humidity goes above 50% then we believe the dryer has just started a cycle
      if ( ( not dryer_on ) and ( humid > 50 ) and ( temp > 30 ) ) {
        dryer_on = true;
        humidity_samples_below_10 = 0;
        Particle.publish(PUSHBULLET_NOTIF_HOME, "Starting drying cycle", 60, PRIVATE);
        Particle.publish(AWS_EMAIL, "Starting drying cycle", 60, PRIVATE);
      }
 
      //if humidity goes below 10% and temperature goes over 50 degrees for a number of samples
      // we believe the clothes are dry
      // modify these parameters if you want to dry even more your clothes
      // example: to have clothes drier modify to "if ( dryer_on and ( humid < 8) and ( temp > 50 ) ) {"
      if ( dryer_on and ( humid < 10) and ( temp > 50 ) ) {
        humidity_samples_below_10 = humidity_samples_below_10 +1;
      }

      //if there are 5 samples below 10% then we are sure it's almost done
      if ( dryer_on and (humidity_samples_below_10 >= 5) ) {
        Particle.publish(PUSHBULLET_NOTIF_HOME, "Your clothes are dry", 60, PRIVATE);
        Particle.publish(AWS_EMAIL, "Your clothes are dry", 60, PRIVATE);
        dryer_on = false;
      }

      if( dryer_on ) {
        dryer_stat = "dryer_on";
      } else {
        dryer_stat = "dryer_off";
      }

    }
 
  }
 
}

//call this function when the algorithm gets stuck (For instance, when you aborted the dryer cycle)
//you could call this function with the DO Button, the blynk app, the Porter app or even Particle dev
int resetDryer(String args) {
  dryer_on = false;
  return 0;
}

//testing AWS email notifications
int sendEmail(String args) {
  Particle.publish(AWS_EMAIL, "Your clothes are dry", 60, PRIVATE);
  return 0;
}