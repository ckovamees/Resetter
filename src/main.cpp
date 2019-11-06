#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>

#include <Ticker.h>
#include <Preferences.h>
#include <OneButton.h>


#define LED_PIN 13
#define PUSH_BUTTON_PIN 12

//Preferences preferences;
Ticker ledTicker;
Ticker buttonTicker;

OneButton button(PUSH_BUTTON_PIN, false, false);
Preferences preferences;

static bool restart = false;

enum State {Good, Pending, Restart};
String StateNames [] = {"Good", "Pending", "Restart"};

int pendingRetries = 0;
static int RetriesBeforeAction = 4;
State state = Good;

static String VERSION = "2019103102";
static String FIRMEWARE_VERSION_URL = "https://raw.githubusercontent.com/ckovamees/OTA/master/resetter/firmware.ver";

void ledTick()
{  
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));     // set pin to the opposite state
}

void buttonTick()
{
    button.tick();    
}

// Gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    // If you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());
    // Entered config mode, make led toggle faster
    ledTicker.attach(0.2, ledTick);
}

void buttonPressed()
{    
    Serial.println("Button pressed");
}

void buttonDoublePressed()
{
    Serial.println("Double pressed");
}

void buttonLongPressed()
{
    // Button is long-pressed indicating a reset    
    Serial.println("Prepare for resetting");
    restart = true;    
}

void setup()
{
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    
    //set led pin as output
    pinMode(LED_PIN, OUTPUT);
    //pinMode(PUSH_BUTTON_PIN, INPUT_PULLUP); 

    Serial.println("Startup");

    WiFiManager wm;

    Serial.println("Create WiFiManager");
    
    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wm.setAPCallback(configModeCallback);

     if (!wm.autoConnect()) {
        Serial.println("failed to connect and hit timeout");
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(1000);
    }    
    
    //Serial.println("Local ip: " + WiFi.localIP().toString());

    // Set indicator LED to Connected
    ledTicker.detach();    
    digitalWrite(LED_PIN, LOW);
    Serial.println("Wifi connected");

    // Setup button actions and check interval
    buttonTicker.attach(0.1, buttonTick);  
    button.attachClick(buttonPressed);
    button.attachDoubleClick(buttonDoublePressed);
    button.attachLongPressStart(buttonLongPressed);
    button.setPressTicks(10000);
    Serial.println("Configured button");

    // Read state
    preferences.begin("RESETTER");
    state = (State)preferences.getInt("state", 0);
    Serial.println("State: " + StateNames[state]);
    if (state == Pending) {
        pendingRetries = preferences.getInt("retries", 0);
        Serial.println("Retries: " + String(pendingRetries));
    }
    if (state == Restart)
    {
        state = Pending;
        pendingRetries = 0;
    }    
    preferences.end();

    // preferences.begin("RESETTER");
    // int count = preferences.getInt("count", 0);      
    // Serial.println("Count: " + String(count));
    // count++;    
    // preferences.putInt("count", count);
    // preferences.end();
}

void checkReset()
{
     if (restart)
    {
        // Restart if config is reset
        Serial.println("Restarting");
        WiFiManager wm;    
        wm.resetSettings();
        // Clear Preferences
        preferences.begin("RESETTER");
        preferences.clear();
        preferences.end();
        ESP.restart();
        delay(1000);
    }     
}

// bool timeForOTACheck()
// {
//     preferences.begin("RESETTER");
//     int count = preferences.getInt("OTACheck", 0);      
//     Serial.println("OTA Check counter: " + String(count));    
//     if (count++ > CHECKOTATIME)
//     preferences.putInt("count", count);
//     preferences.end();
// }

void checkOTA()
{
    HTTPClient http;
    http.begin(FIRMEWARE_VERSION_URL); 
    int httpCode = http.GET();
    Serial.println("Version check HTTP request result: " + String(httpCode));

    if (httpCode == HTTP_CODE_OK) { //Check for the returning code

        // Set state and counter
        state = State::Good;
        pendingRetries = 0;

        // Get response and decode version
        String payload = http.getString();        
        //Serial.println(payload);
        long newVersion = payload.toInt();
        long thisVersion = VERSION.toInt();
        Serial.println("Installed version: " + String(thisVersion) + ", new version: " + String(newVersion));
        
        if (newVersion>thisVersion)
        {
            // Upgrade
            Serial.println("Upgrade is availabe");
            String binUrl = FIRMEWARE_VERSION_URL;
            binUrl.replace(".ver", ".bin");
            http.begin(binUrl);
            httpCode = http.GET();
            Serial.println("Downloading HTTP request result: " + String(httpCode));
        
            if (httpCode == HTTP_CODE_OK) { //Check for the returning code
                int contentLength = http.getSize();
                Serial.println("Firmeware size: " + String(contentLength));
        
                if (Update.begin(contentLength))
                {
                    Serial.println("Starting OTA update...");
                    size_t written = Update.writeStream(http.getStream());
                    if (written == contentLength)
                    {
                        Serial.println("Written : " + String(written) + " successfully");
                    } else {
                        Serial.println("Written only : " + String(written) + "/" + String(contentLength));                            
                    }

                    if (Update.end())
                    {
                        if (Update.isFinished())
                        {
                            Serial.println("OTA update has successfully completed. Rebooting ...");
                            ESP.restart();
                        } else {
                            Serial.println("Something went wrong! OTA update hasn't been finished properly.");
                        }
                    } else {
                        Serial.println("An error Occurred. Error #: " + String(Update.getError()));
                    }                    
                }
            } else {
                Serial.println("Could not download newer firmeware, error " + String(httpCode));
            }

        } // New version exists
    }        
    else {
        Serial.println("Error on HTTP request");

        // Set state and increase counter
        state = Pending;
        pendingRetries++;
    }

    http.end();        
}

void loop()
{    
    int waitPeriod = 10000;

    checkReset();
    
    if (WiFi.status()!=WL_CONNECTED)
    {
        Serial.println("Failed to connect");
        
        // Set state and increase counter
        state = Pending;
        pendingRetries++;
    } 
    else
    {
        checkOTA();

    } // WiFi connected

    // Check states
    switch (state)
    {
    case Good:        
        break;
    case Pending:
        waitPeriod = 5000;
        if (pendingRetries > RetriesBeforeAction) {
            state = Restart;
        }
        break;
    default:
        break;
    }

    // Persist state
    preferences.begin("RESETTER");    
    Serial.print("Persisting state: " + StateNames[state]);
    if (state == Pending)
    {
        Serial.print(", retries: " + String(pendingRetries));
    }
    Serial.println();
    preferences.putInt("state", (int)state);
    preferences.putInt("retries", pendingRetries);
    preferences.end();

    delay(waitPeriod);
}