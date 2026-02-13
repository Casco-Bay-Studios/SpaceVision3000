//SpaceVision3000
//Casco Bay Studios 2026
//cascobaystudios.com
//Hardware based on the Mini Video Player from Super Make Something: https://github.com/SuperMakeSomething/mini-video-player
//software is original, made with AI assistance (don't hurt me)

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <time.h>

#include <WiFiManager.h> // creates hotspot

#include <LuckiestGuyRegular20pt.h> //custom font

// Go to https://api.nasa.gov/ to get a free NASA API key
const char* nasaKey = "YOUR NASA API HERE";

const unsigned long REFRESH_INTERVAL = 2 * 60 * 60 * 1000; // 2 hours in milliseconds
unsigned long lastFetchTime = 0;

// Button pins
const int BUTTON_PREV = 15;  // Cycle through metadata screens / manual frame advance
const int BUTTON_NEXT = 21;  // Switch between modes

int currentScreen = 1;

// Launch data from thespacedevs.com
const int MAX_LAUNCHES = 3;
struct LaunchInfo {
  String name;
  String agency;
  String pad;
  String date;
  String status;
};
LaunchInfo launches[MAX_LAUNCHES];
int totalLaunches = 0;
int currentLaunchScreen = 0;


// Animation control
bool animationMode = false;
bool animationPaused = true; // Toggle to false to use the animation mode. I decided I liked it better just manuall advancing the images so I turned it off
unsigned long lastAnimationFrame = 0;
const unsigned long ANIMATION_DELAY = 500;

//star bitmap
const uint8_t starBitmap[] PROGMEM = {
  0b000010000,
  0b000010000,
  0b001111100,
  0b000111000,
  0b011111110,
  0b000111000,
  0b001010100,
  0b010000010,
  0b000000000
};



TFT_eSPI tft = TFT_eSPI();

// Screen dimensions - Change to your display size
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128
#define MAX_STARS 100


// Structure to hold star data for screensaver v2
struct Star {
    float x, y, z; // z is depth
    uint16_t color;
};
Star stars[MAX_STARS];

// --- ADDED BLACK HOLE VIEWMODE 4 VARIABLES ---
const int BH_MAX_STARS = 150;//150 good
float bh_starX[BH_MAX_STARS], bh_starY[BH_MAX_STARS], bh_starSpd[BH_MAX_STARS];
int bh_starSize[BH_MAX_STARS];

const int BH_MAX_SWIRLS = 100;//90 good
float bh_angle[BH_MAX_SWIRLS], bh_dist[BH_MAX_SWIRLS], bh_spd[BH_MAX_SWIRLS];
const float BH_CX = 80; // Center X
const float BH_CY = 64; // Center Y
const float BH_RAD = 7; // Event horizon size, was 7
//const float BH_CX = 80, BH_CY = 64, BH_RAD = 7;

int bh_colorTheme = 0; // Toggled by double-tap on Pin 21 -- could not really figure this out for the screensaver version


const int numDust = 50;
float dustX[numDust], dustY[numDust];

// --- END BLACK HOLE VARIABLES ---

int ssType = 1; //alternate screen savers


// Initialize stars with random positions and depths
void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(-SCREEN_WIDTH, SCREEN_WIDTH);
        stars[i].y = random(-SCREEN_HEIGHT, SCREEN_HEIGHT);
        stars[i].z = random(1, 10); // Depth
        // Map depth to color (closer = brighter)
        uint8_t br = map(stars[i].z, 1, 10, 255, 100);
        stars[i].color = tft.color565(br, br, br);//br
    }
}

// Function to update and draw the star field (2nd screensaver)
void updateAndDrawStarField() {
currentScreen = 0;
    for (int i = 0; i < MAX_STARS; i++) {
        // 1. Erase previous star
        int oldX = (int)stars[i].x;
        int oldY = (int)stars[i].y;
        tft.drawPixel(oldX, oldY, TFT_BLACK);

        // 2. Move star (diagonal motion)
        stars[i].x += stars[i].z * 0.5;//was * 0.5
        stars[i].y += stars[i].z * 0.5;// higher number speeds up and spreads out
        // 3. Reset star if it goes off screen
        if (stars[i].x >= SCREEN_WIDTH || stars[i].y >= SCREEN_HEIGHT) {
            stars[i].x = random(-160, 160); // Respawn off-left// was -100, 0
            stars[i].y = random(-128, 128); // Respawn off-top// these numbers cover the whole screen! 
            stars[i].z = random(1, 10);
            uint8_t br = map(stars[i].z, 1, 10, 255, 100);
            stars[i].color = tft.color565(br, br, br);
        }

        // 4. Draw new star
        tft.drawPixel((int)stars[i].x, (int)stars[i].y, stars[i].color);
    }
}



// Earth data - multiple images for animation
const int MAX_IMAGES = 13;
String imageNames[MAX_IMAGES];
String imageDates[MAX_IMAGES];
int totalImages = 0;
int currentImageIndex = 0;
float satelliteDistance = 0;
String lastImageDate = "";










// Sun data
const int MAX_SUN_IMAGES = 5;
String sunImageURLs[MAX_SUN_IMAGES] = {
  "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_0171.jpg",
  "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_HMIIC.jpg",
  "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_0335.jpg",
  "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_0094.jpg",
  "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_HMIBC.jpg"
};

String sunImageNames[MAX_SUN_IMAGES] = {
  "AIA 171A Corona",
  "HMI Intensitygram",
  "AIA 335A Active region",
  "AIA 94A Flaring regions",
  "HMI Magnetogram"
};

int currentSunImageIndex = 0;
String sunImageDate = "";

// APOD data
String apodImageURL = "";
String apodTitle = "";
String apodDate = "";

// View mode: 0 = Earth, 1 = Sun, 2 = APOD, 3 = Launches, 4 = Exoplanets
int viewMode = 2; // Start with APOD


// Info screen tracking
//int currentScreen = 1;
const int NUM_SCREENS_EARTH = 6;
const int NUM_SCREENS_SUN = 3;
const int NUM_SCREENS_APOD = 4;
const int NUM_SCREENS_LAUNCHES = 1; // Just one screen showing all 3 launches
const int NUM_SCREENS_EXOPLANETS = 1; // Just one screen


unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// NTP setup
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Callback function to render JPEG blocks to the screen
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}


//TIMER trigger

unsigned long currentMillis; //define the variable "currentMillis" but it has no value yet
unsigned long prevMillis = 0; // define previous millisecond count as 0 for mathing
const long screensaverGoTime = 600000; // 600,000 = 10 minutes
unsigned long lastButtPressMillis = 0;
unsigned long currentButtPressMillis = 0;
bool screenSaverActive = false;
//
//
// Exoplanet data

const int MAX_EXOPLANETS = 3;
struct ExoplanetInfo {
  String name;
  String discoveryMethod;
  String discoveryYear;
  String hostStar;
    String distanceLY;   // NEW
};
ExoplanetInfo exoplanets[MAX_EXOPLANETS];
int totalExoplanets = 0;


void fetchRecentExoplanets() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // NASA Exoplanet Archive TAP service - query for 3 most recent confirmed planets
  // Using simple query format, sorted by discovery publication date descending


String apiURL =
"https://exoplanetarchive.ipac.caltech.edu/TAP/sync?"
"LANG=ADQL&QUERY=select+pl_name%2Chostname%2Cdisc_year%2Cdiscoverymethod%2C"
"round(sy_dist*3.26156%2C1)+as+sy_dist_ly+from+"
"(select+pl_name%2Chostname%2Cdisc_year%2Cdiscoverymethod%2Csy_dist+"
"from+ps+where+default_flag=1+and+disc_pubdate+is+not+null+order+by+disc_pubdate+desc)"
"+where+rownum+%3C%3D+3&FORMAT=json";


  Serial.println("Fetching recent exoplanets...");
  
  if (http.begin(client, apiURL)) {
    http.setTimeout(20000);
    
    int httpCode = http.GET();
    Serial.printf("Exoplanet API response: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.printf("Exoplanet payload size: %d bytes\n", payload.length());
      
      // Allocate based on payload size
      DynamicJsonDocument doc(payload.length() + 1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      // Response is a JSON array
      JsonArray results = doc.as<JsonArray>();
      totalExoplanets = min((int)results.size(), MAX_EXOPLANETS);
      Serial.printf("Found %d recent exoplanets\n", totalExoplanets);
      
      for (int i = 0; i < totalExoplanets; i++) {
        exoplanets[i].name = results[i]["pl_name"].as<String>();
        exoplanets[i].discoveryMethod = results[i]["discoverymethod"].as<String>();
        
        // Convert year to string
        if (results[i]["disc_year"].is<int>()) {
          exoplanets[i].discoveryYear = String(results[i]["disc_year"].as<int>());
        } else {
          exoplanets[i].discoveryYear = results[i]["disc_year"].as<String>();
        }
        
        exoplanets[i].hostStar = results[i]["hostname"].as<String>();
        
        Serial.printf("Exoplanet %d: %s (%s) - %s\n", 
          i, 
          exoplanets[i].name.c_str(), 
          exoplanets[i].discoveryYear.c_str(),
          exoplanets[i].discoveryMethod.c_str());

           //add distance in light years 
 if (!results[i]["sy_dist_ly"].isNull()) {
  exoplanets[i].distanceLY = String(results[i]["sy_dist_ly"].as<float>(), 1);
} else {
  exoplanets[i].distanceLY = "Unknown";
}
      }
    } else {
      Serial.printf("Exoplanet API failed: %d\n", httpCode);
    }
    http.end();
  }
}



//uses bitmap
void drawStarIcon(int x, int y) {
  for (int row = 0; row < 9; row++) {
    uint16_t line = pgm_read_word(&(starBitmap[row]));
    for (int col = 0; col < 9; col++) {
      if (line & (1 << (8 - col))) {
        tft.drawPixel(x + col, y + row, TFT_YELLOW);
      }
    }
  }
}



void displayExoplanetInfo() {
  tft.fillScreen(TFT_BLACK);

  if (totalExoplanets == 0) {
    tft.setCursor(10, 60);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.println("No exoplanet data");
    return;
  }

  tft.setTextSize(1);

  // ===== HEADER =====
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setCursor(2, 4);
  tft.println("RECENT EXOPLANETS");

  tft.drawFastHLine(0, 14, tft.width(), TFT_MAGENTA);

  int yBase = 18;
  const int blockHeight = 32;

  for (int i = 0; i < totalExoplanets && i < 3; i++) {

    int y = yBase + (i * blockHeight) + 2;

    // Planet number + name
    // Background panel
//tft.fillRoundRect(2, y - 2, 156, 28, 4, TFT_DARKGREY);
tft.drawRoundRect(2, y - 2, 156, 28, 4, TFT_NAVY);

    tft.setCursor(3, y);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.printf("%d. ", i + 1);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String name = exoplanets[i].name;
    if (name.length() > 20) name = name.substring(0, 17) + "...";
    tft.println(name);

    // Discovery year + method
    // Background panel
//tft.fillRoundRect(2, y - 2, 156, 28, 4, TFT_DARKGREY);
tft.drawRoundRect(2, y - 2, 156, 28, 4, TFT_NAVY);

    tft.setCursor(8, y + 10);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

    String method = exoplanets[i].discoveryMethod;
    if (method == "Radial Velocity") method = "RadVel";
    if (method == "Transit Timing Variations") method = "TTV";
    if (method.length() > 12) method = method.substring(0, 10) + "...";

    tft.printf("%s | %s",
      exoplanets[i].discoveryYear.c_str(),
      method.c_str()
    );

    // Host star + distance line
    // Background panel
//tft.fillRoundRect(2, y - 2, 156, 28, 4, TFT_DARKGREY);
tft.drawRoundRect(2, y - 2, 156, 28, 4, TFT_NAVY);

    int starLineY = y + 19;

    drawStarIcon(6, starLineY - 2);

    tft.setCursor(16, starLineY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    String host = exoplanets[i].hostStar;
    if (host.length() > 14) host = host.substring(0, 11) + "...";
    tft.print(host);

    // Distance right-aligned
 float dist = exoplanets[i].distanceLY.toFloat();
uint16_t distColor = TFT_GREEN;

if (dist >= 100 && dist <= 500) distColor = TFT_CYAN;
if (dist > 500) distColor = TFT_ORANGE;

String distText = exoplanets[i].distanceLY + " ly";
int w = tft.textWidth(distText);

tft.setTextColor(distColor, TFT_BLACK);
tft.setCursor(tft.width() - w - 6, starLineY);
tft.print(distText);

 

    // Separator (but not after last planet)
    if (i < totalExoplanets - 1) {
      tft.drawFastHLine(0, yBase + ((i + 1) * blockHeight) - 2, tft.width(), TFT_DARKGREY);

    }
  }
}


//
//
//
// BLACK HOLE  STUFF

unsigned long lastActivityMillis = 0;  // Track any button press
//
//
//
//
//



int currentSS = 1;      // 0 = Original Starfield, 1 = Black Hole
unsigned long lastDoubleTapTime = 0; //not used

float currentWarp = 2.0;

// --- NEW BLACK HOLE FUNCTIONS ---

void initBlackHole() {
   // Initialize Space Dust (Tiny/Dim)
  for (int i = 0; i < numDust; i++) {
    dustX[i] = random(0, 160);
    dustY[i] = random(0, 128);
  }
 
 
  for (int i = 0; i < BH_MAX_STARS; i++) {
    bh_starX[i] = random(0, 160);
    bh_starY[i] = random(0, 128);
    bh_starSize[i] = (random(0, 10) > 8) ? 2 : 1;
    bh_starSpd[i] = (bh_starSize[i] == 2) ? 1.2 : 0.6; // Parallax //? (random(5, 8) / 10.0) : (random(10, 15) / 10.0)
  }
  for (int i = 0; i < BH_MAX_SWIRLS; i++) {
    bh_angle[i] = (float)random(0, 628) / 100.0;
    bh_dist[i] = random(BH_RAD + 2, BH_RAD + 12);
    bh_spd[i] = 0.05 + (random(0, 10) / 400.0); // was 0.02
  }
}

// Lensing math for TFT_eSPI
bool getLensedPos(float x, float y, float dx, float dy, float dSq, int &outX, int &outY) {
  if (dSq < (BH_RAD * BH_RAD)) return false; 
  float shift = 45.0 / (dSq * 0.25 + 6); 
  outX = (int)(x + dx * shift);
  outY = (int)(y + dy * shift);
  return (outX >= 0 && outX < 160 && outY >= 0 && outY < 128);
}

void runBlackHoleFrame() {
  
    // 1. Space Dust (Deep Background)
  for (int i = 0; i < numDust; i++) {
    tft.drawPixel((int)dustX[i], (int)dustY[i], ST7735_BLACK); // Erase
    dustX[i] -= (0.2 * currentWarp); 
    if (dustX[i] < 0) dustX[i] = 160;
    tft.drawPixel((int)dustX[i], (int)dustY[i], 0x3186); // Dim Gray Dust
  }
  
  // 1a. Accretion Swirl
  for (int i = 0; i < BH_MAX_SWIRLS; i++) {
    // Erase old
    int ox = (int)(BH_CX + cos(bh_angle[i]) * bh_dist[i]);
    int oy = (int)(BH_CY + sin(bh_angle[i]) * bh_dist[i] * 0.4);
    tft.drawPixel(ox, oy, TFT_BLACK);

   // bh_angle[i] += bh_spd[i];
bh_angle[i] += (bh_spd[i] * (currentWarp));// * 1.5 + 1.5));
    // Doppler colors
    float m = sin(bh_angle[i]);
    uint16_t col;
    if (bh_colorTheme == 0) col = (m > 0.3) ? 0xAD7F : (m < -0.3 ? TFT_RED : TFT_ORANGE);
    else if (bh_colorTheme == 1) col = (m > 0.3) ? TFT_WHITE : (m < -0.3 ? 0x8010 : 0xA01F);
    else col = (m > 0.3) ? TFT_WHITE : (m < -0.3 ? 0x03EF : TFT_CYAN);

    int nx = (int)(BH_CX + cos(bh_angle[i]) * bh_dist[i]);
    int ny = (int)(BH_CY + sin(bh_angle[i]) * bh_dist[i] * 0.4);
    tft.drawPixel(nx, ny, col);
  }

  // 2. Stars with Lensing
  for (int i = 0; i < BH_MAX_STARS; i++) {
    float dx = bh_starX[i] - BH_CX; float dy = bh_starY[i] - BH_CY; float dSq = dx*dx + dy*dy;
    int ex, ey;
    if (getLensedPos(bh_starX[i], bh_starY[i], dx, dy, dSq, ex, ey)) {
      if (bh_starSize[i] == 1) tft.drawPixel(ex, ey, TFT_BLACK);
      else tft.fillRect(ex, ey, 2, 2, TFT_BLACK);
    }

   // bh_starX[i] -= bh_starSpd[i];
     bh_starX[i] -= (bh_starSpd[i] * currentWarp);// * 2.0); 
    //   if (bh_starX[i] < -10) bh_starX[i] = 170;
   if (bh_starX[i] < -15) { 
        // 1. Respawn just off the right edge with a randomized buffer
        bh_starX[i] = 160 + random(5, 40); 
        
        // 2. Randomize the height so the star doesn't follow the same path
        bh_starY[i] = random(0, 128); 
        
        // 3. Randomize the "depth" (size and speed) for parallax
        bh_starSize[i] = (random(0, 10) > 8) ? 2 : 1; 
        bh_starSpd[i] = (bh_starSize[i] == 2) ? (random(10, 15) / 10.0) : (random(5, 8) / 10.0);
    }


    float nDx = bh_starX[i] - BH_CX; float nDy = bh_starY[i] - BH_CY;
    int nx, ny;
    if (getLensedPos(bh_starX[i], bh_starY[i], nDx, nDy, (nDx*nDx+nDy*nDy), nx, ny)) {
      uint16_t c = (bh_starSize[i] == 2) ? TFT_WHITE : 0x7BEF;
      if (bh_starSize[i] == 1) tft.drawPixel(nx, ny, c);
      else tft.fillRect(nx, ny, 2, 2, c);
    }
  }
  tft.fillCircle(BH_CX, BH_CY, BH_RAD, TFT_BLACK);


//below was to have button 15 speed up the black hole and a double click changed the colors but I could not make it work
 // 1. INTERACTION: Check Button 15 while screensaver is running
    /*bool btnPressed = (digitalRead(BUTTON_PREV) == LOW);
    
    // WARP EFFECT: Increase speed while holding button
    if (btnPressed) {


      delay(100);//debounce 
      currentWarp = min(currentWarp + 0.15f, 6.0f); // Speed up
     
      lastButtPressMillis = millis(); // Keep resetting timer so it doesn't wake up instantly
    } else {
      currentWarp = max(currentWarp - 0.1f, 1.0f);  // Slow back down
    }

    // COLOR THEME: Double-tap logic (Pin 15)
    if (btnPressed && (millis() - lastButtonPress > DEBOUNCE_DELAY)) {
      unsigned long now = millis();
      if (now - lastDoubleTapTime < 400) { // Double tap detected
        bh_colorTheme = (bh_colorTheme + 1) % 3;
        tft.fillScreen(TFT_BLACK); // Flash clear for new theme
      }
      lastDoubleTapTime = now;
      lastButtonPress = now;
    }
*/





delay(12);

}







void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PREV, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);




  tft.begin();
  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  typewriterFX2();
/*  tft.setTextColor(TFT_CYAN, TFT_BLACK);
tft.setCursor(55, 40);//x (horiz),y(vert)
tft.println("Dr. Cole's");
tft.setCursor(40, 60);//x (horiz),y(vert)
tft.println("SpaceVision 3000");
*/
delay(2000);
 tft.fillScreen(TFT_BLACK);
 tft.setCursor(0, 0);



  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  initStars();//screensaver 2.0
  initBlackHole();   // New Black Hole


//screen is 25wx16h size 1 characters

tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");


delay(500);
 tft.fillScreen(TFT_BLACK);
 tft.setCursor(0, 0);
tft.println("");
tft.println("");
tft.println("   o         '   .     * ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


 tft.setCursor(0, 0);
tft.println("");
tft.println("");
tft.println("   o         '   .     / ");
tft.println("        *           | *  ");
tft.println(" *           .    --o--  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);

  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .     / ");
tft.println("        *           | /  ");
tft.println(" *           .    --o*-  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           | /  ");
tft.println(" *           .    --o/-  ");
tft.println("              .     *    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o/-  ");
tft.println("              .     /    ");
tft.println("       o           *  '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  ");
tft.println("              .     /    ");
tft.println("       o           /  '  ");
tft.println("      ____________*_     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


  tft.setCursor(0, 0); 
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  "); 
tft.println("              .     |    ");
tft.println("       o           /  '  ");
tft.println("      ____________/_     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);

  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ____________/_     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(200);
 tft.fillScreen(TFT_BLACK);


  tft.setCursor(0, 0);
  tft.println("");
tft.println("");
tft.println("   o         '   .       ");
tft.println("        *           |    ");
tft.println(" *           .    --o--  ");
tft.println("              .     |    ");
tft.println("       o              '  ");
tft.println("      ______________     ");
tft.println("  ==c(___(o(_____(_()    ");
tft.println("         |=|             ");
tft.println("        //|\^            ");
tft.println("       //|| \^           ");
tft.println("      // ||  \^          ");
tft.println("     //  ||   \^         ");
tft.println("    //         \^        ");

delay(2000);
 tft.fillScreen(TFT_BLACK);






    WiFiManager wm; // Local initialization
// Optional: Wipes saved credentials for testing
  //  wm.resetSettings();


  tft.setCursor(0, 30);
 
  tft.println("Please connect to"); 
  tft.println("");
  tft.print("Wifi hotspot ");
   tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("SpaceVision3K");
   tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("");
  tft.print("Password: ");
   tft.setTextColor(TFT_CYAN, TFT_BLACK);   
  tft.println("marvin42");
   tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("");
  tft.println("to configure WiFi");
  
    // Auto-connect to saved credentials or start Access Point if needed
    bool res = wm.autoConnect("SpaceVision3K", "marvin42"); // AP name and optional password



if(!res) {
        Serial.println("Failed to connect or hit timeout");
        // ESP.restart();
    } else {
        Serial.println("Connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }

 tft.fillScreen(TFT_BLACK);
tft.setTextFont(1);

tft.setCursor(0, 50);
typewriterFX();
tft.setTextColor(TFT_ORANGE);
tft.println("");
tft.println("");
tft.print("I'm connected to Wifi!");
delay(2000);
 tft.fillScreen(TFT_BLACK);

// startup message DONT PANIC!
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
tft.setFreeFont(&LuckiestGuyRegular20pt);
tft.setCursor(40, 50);//x (horiz),y(vert)
tft.println("don't");
tft.setCursor(40, 80);
tft.println("panic");
tft.setTextFont(1);
delay(2000);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
  
 





  // Fetch data
  fetchAllEpicImages();
  fetchUpcomingLaunches(); // Fetch launches too

fetchRecentExoplanets(); // Added







  // Start with APOD
  fetchAPOD();
  
  lastFetchTime = millis();
}


//  $$          #########   %%%%%%%%%   @@@@@@@@@
//  $$          ##      #   %       %   @@      @
//  $$          ##      #   %       %   @@      @
//  $$          ##      #   %       %   @@      @
//  $$          ##      #   %       %   @@@@@@@@@
//  $$          ##      #   %       %   @@
//  $$$$$$$$$$  ##      #   %       %   @@
//  $$$$$$$$$$  #########   %%%%%%%%%   @@




void loop() {


  // Check buttons with debouncing
  if (millis() - lastButtonPress > DEBOUNCE_DELAY) {

  


// Button NEXT (pin 21) - Cycle through modes


if (digitalRead(BUTTON_NEXT) == LOW){
delay(100);//debounce


lastButtPressMillis = millis();//record the time since sketch  started

  if (screenSaverActive){
    delay(200);
  //  wakeUpFromSS();//added
   ssType = (ssType == 0) ? 1 : 0; 
    screenSaverActive = false;

// Reconnect WiFi using saved credentials
  Serial.println("Screensaver deactivated - reconnecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(); // No parameters - uses saved credentials
  
  // Wait for connection (with timeout)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
  } else {
    Serial.println("\nWiFi reconnection failed!");
  }
  }


 
  if (viewMode == 2) {

    currentScreen = 1;
    Serial.println("Switching to Launch viewer");

 // APOD → Launches
       viewMode = 3;
       // currentScreen = 1;
       // Serial.println("Switching to Launches view");
        displayLaunchInfo();


 } else if (viewMode == 3) {
    // Launches → Exoplanets
    viewMode = 4;
    currentScreen = 0;
    Serial.println("Switching to Exoplanets view");
    displayExoplanetInfo();





}else if (viewMode == 4) {
     // Exoplanets → Static Earth
    viewMode = 0;
        animationMode = false;
        currentScreen = 1;
        Serial.println("Switching to Static Earth view");
        displayEpicImage(0);
        displayMetadata();
  } else if (viewMode == 0 && !animationMode) {
        // Static Earth → Earth Animation
        animationMode = true;
        currentScreen = 1;
        Serial.println("Switching to Earth Animation");
        if (totalImages > 1) {
          currentImageIndex = 0;
          displayEpicImage(currentImageIndex);
          lastAnimationFrame = millis();
        } else {
          animationMode = false;
          displayEpicImage(0);
          displayMetadata();
        }
} else if (viewMode == 0 && animationMode) {
        // Earth Animation → Sun (first image)
        viewMode = 1;
        animationMode = false;
        currentScreen = 1;
        currentSunImageIndex = 0;
        Serial.println("Switching to Sun view");
        fetchSunImage(currentSunImageIndex);

} else if (viewMode == 1) {
        // Sun → Next sun image
        currentSunImageIndex = (currentSunImageIndex + 1) % MAX_SUN_IMAGES;
        
        if (currentSunImageIndex == 0) {
          // Cycled through all sun images → go to black hole

          //viewMode = 2;
          viewMode = 2;//APOD mode
          currentScreen = 1;
          Serial.println("Switching to APOD view");
          fetchAPOD();
        } else {
          Serial.printf("Next sun image: %s\n", sunImageNames[currentSunImageIndex].c_str());
          fetchSunImage(currentSunImageIndex);
          //viewMode = 4;//??????
        }

 }
 
    

      lastButtonPress = millis();
    }
  

 // Button PREV (pin 15) - Cycle through metadata screens (or manual Earth frame advance)
if (digitalRead(BUTTON_PREV) == LOW) {
  delay(100);
   //bump out of screensaver
    if (screenSaverActive){
    delay(200);
    wakeUpFromSS();//added
    screenSaverActive = false;
  //return;
  }

 Serial.printf("Button 15: viewMode=%d, animationMode=%d, animationPaused=%d\n", viewMode, animationMode, animationPaused);



  if (animationMode && viewMode == 0) {
    delay(100);// more debounce?
    // Pause animation and advance frame manually
    animationPaused = true;
    Serial.printf("Animation PAUSED, advancing to frame %d\n", (currentImageIndex + 1) % totalImages);
    currentImageIndex = (currentImageIndex + 1) % totalImages;
    displayEpicImage(currentImageIndex);
    lastAnimationFrame = millis(); // Reset timer
  } else if (viewMode == 3) {
    // Refresh launch data
    fetchUpcomingLaunches();
    displayLaunchInfo();
  
  } else if (viewMode == 4) {
    Serial.println("Refreshing exoplanets");
    fetchRecentExoplanets();
    displayExoplanetInfo();
  
  
  
  
  
  } else {
     Serial.println("Cycling metadata");
    int maxScreens = NUM_SCREENS_EARTH;
    if (viewMode == 1) maxScreens = NUM_SCREENS_SUN;
    else if (viewMode == 2) maxScreens = NUM_SCREENS_APOD;
    
    currentScreen = (currentScreen + 1) % maxScreens;
    displayMetadata();
  }
  lastButtonPress = millis();
  lastButtPressMillis = millis();
}
  //updated auto advance^
  }  




  
 // Auto-advance Earth animation frames only (if not paused)
if (animationMode && viewMode == 0 && totalImages > 1 && !animationPaused) {
  if (millis() - lastAnimationFrame >= ANIMATION_DELAY) {
    currentImageIndex = (currentImageIndex + 1) % totalImages;
    displayEpicImage(currentImageIndex);
    lastAnimationFrame = millis();
  }
}
  
  // Check if it's time to refresh
  if (millis() - lastFetchTime >= REFRESH_INTERVAL) {
    Serial.println("Time to refresh images...");
    if (viewMode == 0) {
      fetchAllEpicImages();
      if (totalImages > 0) {
        if (animationMode) {
          currentImageIndex = 0;
          displayEpicImage(currentImageIndex);
        } else {
          displayEpicImage(0);
          displayMetadata();
        }
      }
    } else if (viewMode == 1) {
      fetchSunImage(currentSunImageIndex);
    } else if (viewMode == 2) {
      fetchAPOD();
  
    } else if (viewMode == 3) {
      fetchUpcomingLaunches();
      displayLaunchInfo();
    } else if (viewMode == 4) {
    fetchRecentExoplanets();
    displayExoplanetInfo();
  }
    lastFetchTime = millis();
  }
  
  delay(50);



if (millis() - lastButtPressMillis >= screensaverGoTime){
  if(!screenSaverActive){
    screenSaverActive = true;
     Serial.println("Screensaver activated - turning off WiFi");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
   //screenSaverFunctionA(); 
    if(ssType ==1){
      tft.fillScreen(TFT_BLACK);
    }
      }//ends top if


currentScreen = 0;

//Serial.println ("Starting ");
//Serial.println (ssType);
//tft.fillScreen(TFT_BLACK);

 // ALTERNATE SCREENSAVERS
    if (ssType == 0) {
      updateAndDrawStarField(); // Your original function
    } else {
      //tft.fillScreen(TFT_BLACK);
      runBlackHoleFrame();      // Our new function
  }    
} 
//this down below is outside the above check to start screensaver
else {
  if (!screenSaverActive) {
    return;
    }
   }
  } //loop closer bracket
 



//
//
//
//
//      E N D  L O O P 
//
//
//
//
//







void displayMetadata() {
  tft.setTextSize(1);
  tft.fillRect(0, tft.height() - 10, tft.width(), 10, TFT_BLACK);
  
  if (currentScreen == 0) {
    return; // Blank screen
  }
  
  tft.setCursor(2, tft.height() - 8);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  if (viewMode == 0) {
    // Earth metadata
    switch(currentScreen) {
      case 1:
        tft.print("    Mostly harmless");
        break;
      case 2:
        tft.print(lastImageDate);
        tft.print(" UTC");
        break;
      case 3:
        tft.print("Dist: L1 ");
        tft.print(satelliteDistance / 1000000.0, 3);
        tft.print(" Mkm");
        break;
      case 4:
        tft.print("NASA EPIC/DSCOVR satellite");
        break;
     
        case 5:
        tft.print("NEXT: Earth Multi-View");
        break;
    }
  } else if (viewMode == 1) {
    // Sun metadata
    switch(currentScreen) {
      case 1:
        tft.print(sunImageNames[currentSunImageIndex]);
        break;
        
      case 2:
        tft.print(sunImageDate);
        tft.print(" UTC");
        break;
      case 3:
      if (currentSunImageIndex == 5){
      tft.print("NEXT: NASA APOD");
      }
      break;
    }
  } else if (viewMode == 2) {
    // APOD metadata
    switch(currentScreen) {
      case 1:
        tft.print("NASA APOD:");
        tft.print(apodDate);
        break;
      case 2:{
        String title = apodTitle;
        if (title.length() > 25) {
          title = title.substring(0, 22) + "...";
        }
        
        tft.print(title);
        break;
        }
      case 3:
        tft.print("NEXT: Upcoming launches");
        break;
    }
  }
}

String getCurrentDateString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "2026-01-29T12:00:00Z";
  }
  
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

void fetchAllEpicImages() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String apiURL = "https://epic.gsfc.nasa.gov/api/natural?api_key=" + String(nasaKey);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.println("Sorry, did I ");
  tft.println("say something wrong?");
    tft.println("");
    tft.println("Fetching EPIC data...");
  

  Serial.println("Fetching all EPIC images...");
  if (http.begin(client, apiURL)) {
    int httpCode = http.GET();
    Serial.printf("Metadata response: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(16384);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      totalImages = min((int)doc.size(), MAX_IMAGES);
      Serial.printf("Found %d images\n", totalImages);
      
      for (int i = 0; i < totalImages; i++) {
        imageNames[i] = doc[i]["image"].as<String>();
        imageDates[i] = doc[i]["date"].as<String>();
        Serial.printf("Image %d: %s at %s\n", i, imageNames[i].c_str(), imageDates[i].c_str());
      }
      
      if (totalImages > 0) {
        JsonObject coords = doc[0]["dscovr_j2000_position"];
        float x = coords["x"];
        float y = coords["y"];
        float z = coords["z"];
        satelliteDistance = sqrt(x*x + y*y + z*z);
        
        lastImageDate = imageDates[0].substring(0, 16);
        Serial.printf("Distance: %.0f km\n", satelliteDistance);
      }
    }
    http.end();
  }
}

void displayEpicImage(int index) {
  if (index < 0 || index >= totalImages) return;
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String dateStr = imageDates[index];
  String year = dateStr.substring(0, 4);
  String month = dateStr.substring(5, 7);
  String day = dateStr.substring(8, 10);
  
  String imgURL = "https://epic.gsfc.nasa.gov/archive/natural/" + year + "/" + month + "/" + day + "/thumbs/" + imageNames[index] + ".jpg";
  
  Serial.printf("Loading image %d/%d: %s\n", index + 1, totalImages, imageNames[index].c_str());
  
  if (http.begin(client, imgURL)) {
    int imgCode = http.GET();
    
    if (imgCode == HTTP_CODE_OK) {
      int total = http.getSize();
      
      uint8_t* buff = (uint8_t*)malloc(total);
      if (buff) {
        WiFiClient* stream = http.getStreamPtr();
        int bytesRead = stream->readBytes(buff, total);
        
        tft.fillScreen(TFT_BLACK);
        TJpgDec.drawJpg(20, 0, buff, total);
        
        // Show frame counter at bottom in animation mode
        if (animationMode) {
          tft.setTextSize(1);
          tft.fillRect(0, tft.height() - 10, tft.width(), 10, TFT_BLACK);
          tft.setCursor(2, tft.height() - 8);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.printf("EPIC/DSCOVR View %d/%d", index + 1, totalImages);
        }
        
        free(buff);
      } else {
        Serial.println("Failed to allocate memory for image!");
      }
    } else {
      Serial.printf("Failed to download image %d, code: %d\n", index, imgCode);
    }
    http.end();
  }
}

void fetchSunImage(int index) {
  if (index < 0 || index >= MAX_SUN_IMAGES) return;
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String currentDate = getCurrentDateString();
  sunImageDate = currentDate.substring(0, 16);
  sunImageDate.replace('T', ' ');
  
  TJpgDec.setJpgScale(2);
  
  String imgURL = sunImageURLs[index];
  
  Serial.printf("Fetching Sun image %d/%d: %s\n", index + 1, MAX_SUN_IMAGES, sunImageNames[index].c_str());
  Serial.println("URL: " + imgURL);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.println("Downloading");
    tft.println("");
  tft.printf("%s...", sunImageNames[index].c_str());
  
  client.setTimeout(15000);
  
  if (http.begin(client, imgURL)) {
    http.setTimeout(15000);
    
    int imgCode = http.GET();
    Serial.printf("Sun image response: %d\n", imgCode);
    
    if (imgCode == HTTP_CODE_OK) {
      int total = http.getSize();
      Serial.printf("Image size: %d bytes\n", total);
      
      if (total > 0 && total < 50000) {
        uint8_t* buff = (uint8_t*)malloc(total);
        
        if (buff) {
          WiFiClient* stream = http.getStreamPtr();
          stream->setTimeout(10000);
          
          int bytesRead = 0;
          int chunkSize = 1024;
          unsigned long lastRead = millis();
          
          while (bytesRead < total) {
            if (!stream->available()) {
              if (millis() - lastRead > 5000) {
                Serial.println("Stream timeout");
                break;
              }
              delay(10);
              continue;
            }
            
            int remaining = total - bytesRead;
            int toRead = min(chunkSize, remaining);
            int read = stream->readBytes(buff + bytesRead, toRead);
            
            if (read > 0) {
              bytesRead += read;
              lastRead = millis();
            }
          }
          
          Serial.printf("Total read: %d bytes\n", bytesRead);
          
          if (bytesRead >= total * 0.95) {
            tft.fillScreen(TFT_BLACK);
            
            uint16_t result = TJpgDec.drawJpg(16, 0, buff, bytesRead);
            Serial.printf("JPEG decode result: %d\n", result);
            
            if (result == 0) {
              displayMetadata();
            } else {
              tft.fillScreen(TFT_BLACK);
              tft.setCursor(0, 0);
              tft.printf("Decode error: %d", result);
            }
          } else {
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            tft.println("Download timeout!");
          }
          
          free(buff);
        } else {
          Serial.println("Failed to allocate memory!");
          tft.fillScreen(TFT_BLACK);
          tft.setCursor(0, 0);
          tft.println("Memory error!");
        }
      }
    }
    http.end();
  }
  
  TJpgDec.setJpgScale(1);
}

void fetchAPOD() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String apiURL = "https://api.nasa.gov/planetary/apod?api_key=" + String(nasaKey) + "&thumbs";
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(0, 0);
  tft.println("I'm just a menial robot,");
  tft.println("");
  tft.println("fetching the APOD...");
  
  Serial.println("Fetching Astronomy Picture of the Day...");
  if (http.begin(client, apiURL)) {
    int httpCode = http.GET();
    Serial.printf("APOD API response: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      String rawImageURL = doc["url"].as<String>();
      apodTitle = doc["title"].as<String>();
      apodDate = doc["date"].as<String>();
      
      Serial.printf("APOD: %s\n", apodTitle.c_str());
      Serial.printf("Date: %s\n", apodDate.c_str());
      Serial.printf("Raw URL: %s\n", rawImageURL.c_str());
      
      http.end();
      
      // Use resize proxy to get 160x128 image
      //apodImageURL = "https://images.weserv.nl/?url=" + rawImageURL + "&w=160&h=128";
        // Use resize proxy to get 160x128 image AND convert to JPEG
      // The output=jpg parameter forces JPEG conversion from PNG
      apodImageURL = "https://images.weserv.nl/?url=" + rawImageURL + "&w=160&h=128&output=jpg";


      Serial.printf("Resized URL: %s\n", apodImageURL.c_str());
      
      // Download resized APOD
      displayAPODImage();
    } else {
      http.end();
    }
  }
}

void displayAPODImage() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  Serial.println("Downloading resized APOD...");

  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  
  tft.setTextSize(1);
       tft.println("Don't mind me, I'm ");
  
      tft.println("");

  tft.println("Downloading the APOD...");
  
  client.setTimeout(20000);
  
  if (http.begin(client, apodImageURL)) {
    http.setTimeout(20000);
    
    int imgCode = http.GET();
    Serial.printf("APOD image response: %d\n", imgCode);
    
    if (imgCode == HTTP_CODE_OK) {
      int total = http.getSize();
      Serial.printf("Resized image size: %d bytes\n", total);
      
      int allocSize = (total > 0) ? total : 30000;
      
      if (allocSize < 80000) {
        uint8_t* buff = (uint8_t*)malloc(allocSize);
        
        if (buff) {
          WiFiClient* stream = http.getStreamPtr();
          stream->setTimeout(15000);
          
          int bytesRead = 0;
          int chunkSize = 1024;
          unsigned long lastRead = millis();
          
          while (bytesRead < allocSize) {
            if (!stream->available()) {
              if (millis() - lastRead > 8000) {
                Serial.println("Stream timeout");
                break;
              }
              delay(10);
              continue;
            }
            
            int remaining = allocSize - bytesRead;
            int toRead = min(chunkSize, remaining);
            int read = stream->readBytes(buff + bytesRead, toRead);
            
            if (read > 0) {
              bytesRead += read;
              lastRead = millis();
            }
          }
          
          Serial.printf("Total read: %d bytes\n", bytesRead);
          
          if (bytesRead > 100) {
            tft.fillScreen(TFT_BLACK);
            
            TJpgDec.setJpgScale(1);
            uint16_t result = TJpgDec.drawJpg(0, 0, buff, bytesRead);
            Serial.printf("JPEG decode result: %d\n", result);
            
            if (result == 0) {
              displayMetadata();
            } else {
              tft.fillScreen(TFT_BLACK);
              tft.setCursor(0, 0);
              tft.println("Decode error!");
            }
          }
          
          free(buff);
        } else {
          Serial.println("Failed to allocate memory!");
          tft.fillScreen(TFT_BLACK);
          tft.setCursor(0, 0);
          tft.println("Memory error!");
        }
      }
    }
    http.end();
  }
}

void fetchUpcomingLaunches() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String apiURL = "https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=3&mode=list&hide_recent_previous=true";
  
  Serial.println("Fetching upcoming launches...");
    tft.setCursor(2, 40);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_PINK, TFT_BLACK);
    tft.println("Here I am, brain");
    tft.println("the size of a planet,");
      tft.println("");
      tft.println("Fetching upcoming launches...");

  if (http.begin(client, apiURL)) {
    http.setTimeout(15000);
    
    int httpCode = http.GET();
    Serial.printf("Launch API response: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.printf("Payload size: %d bytes\n", payload.length());
      
      // Use larger doc size for launch API response
      DynamicJsonDocument doc(payload.length() + 512);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      JsonArray results = doc["results"];
      totalLaunches = min((int)results.size(), MAX_LAUNCHES);
      Serial.printf("Found %d upcoming launches\n", totalLaunches);
      
      for (int i = 0; i < totalLaunches; i++) {
        launches[i].name = results[i]["name"].as<String>();
        launches[i].date = results[i]["net"].as<String>();
        
        int statusId = results[i]["status"]["id"].as<int>();
        switch(statusId) {
          case 1: launches[i].status = "Go"; break;
          case 2: launches[i].status = "TBD"; break;
          case 3: launches[i].status = "Good"; break;
          case 4: launches[i].status = "Fail"; break;
          default: launches[i].status = "???"; break;
        }
        
        if (results[i].containsKey("launch_service_provider")) {
          launches[i].agency = results[i]["launch_service_provider"]["name"].as<String>();
        }
        if (results[i].containsKey("pad")) {
          launches[i].pad = results[i]["pad"]["name"].as<String>();
        }
        
        Serial.printf("Launch %d: %s - %s [%s]\n", i, launches[i].name.c_str(), launches[i].date.c_str(), launches[i].status.c_str());
      }
    }
    http.end();
  }
}
void displayLaunchInfo() {
  if (totalLaunches == 0) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(2, 2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("No launch data!");
    return;
  }
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Title
  tft.setCursor(2, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("UPCOMING LAUNCHES");
 // tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Draw separator line
  tft.drawFastHLine(0, 14, tft.width(), TFT_CYAN);
  
  int yPos = 18;
  for (int i = 0; i < totalLaunches; i++) {
    // Status color
    uint16_t statusColor = TFT_YELLOW;
    if (launches[i].status == "Go") statusColor = TFT_GREEN;
    else if (launches[i].status == "TBD") statusColor = TFT_YELLOW;
    
    // Launch number + status
    tft.setCursor(2, yPos);
    tft.setTextColor(statusColor, TFT_BLACK);
    tft.printf("[%s]", launches[i].status.c_str());//just show status, no number
    //tft.printf("%d. [%s]", i + 1, launches[i].status.c_str());
   // yPos += 10;
    
    // Mission name - split into two lines at closest space
    String name = launches[i].name;
    String line1 = "";
    String line2 = "";
    
    const int maxCharsPerLineA = 20; // Adjust based on your display width
    const int maxCharsPerLineB = 25; // Adjust based on your display width
    
    if (name.length() <= maxCharsPerLineA) {
      // Fits on one line
      line1 = name;
    } else {
      // Need to split - find last space before maxCharsPerLine
      int splitPos = name.lastIndexOf(' ', maxCharsPerLineA);
      if (splitPos == -1 || splitPos < maxCharsPerLineA / 2) {
        // No good space found, just hard split
        splitPos = maxCharsPerLineA;
      }
      
      line1 = name.substring(0, splitPos);
      line2 = name.substring(splitPos + 1); // Skip the space
      
      // Truncate line2 if still too long
      if (line2.length() > maxCharsPerLineB) {
        line2 = line2.substring(0, maxCharsPerLineB - 3) + "...";
      }
    }
    
    // Display line 1
    tft.setCursor(40, yPos);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println(line1);
    yPos += 10;
    
    // Display line 2 if exists
    if (line2.length() > 0) {
      tft.setCursor(5, yPos);
      tft.println(line2);
      yPos += 10;
    }
    
// Date (truncate to fit)
if (launches[i].date.length() > 0) {
  tft.setCursor(5, yPos);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  // Show just date and time: "2026-01-29 15:17"
  String dateStr = launches[i].date.substring(0, 16);
  dateStr.replace('T', ' '); // Replace T with space
//  tft.println(dateStr);
  tft.print(dateStr);           // Print the date first
tft.setTextColor(TFT_MAGENTA); // Change color to Purple
tft.println(" Zulu");          // Print UTC and move to the next line
  yPos += 10;
}
    
    // Separator between launches
    if (i < totalLaunches - 1) {
      tft.drawFastHLine(0, yPos + 1, tft.width(), TFT_DARKGREY);
      yPos += 5;
    }
  }
}
/*

**What changed:**
- Mission names now intelligently wrap to 2 lines
- Finds the last space before the 26-character limit to break naturally
- If no good space is found, does a hard break
- Second line gets truncated with "..." if still too long
- Adjusted spacing to fit 3 launches on the 128-pixel tall display
- Reduced line spacing from 10px to 9px to fit everything

The `maxCharsPerLine` is set to 26 which should work well for your 160px wide display with size 1 font. Adjust if needed!

Now launch names like "Falcon 9 Block 5 | Starlink Group 12-3" will display as:
```
1. [Go]
Falcon 9 Block 5 |
Starlink Group 12-3
2026-01-29 15:17
*/

void wakeUpFromSS() {
  screenSaverActive = false;
 
// Reconnect WiFi using saved credentials
  Serial.println("Screensaver deactivated - reconnecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(); // No parameters - uses saved credentials
  
  // Wait for connection (with timeout)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
  } else {
    Serial.println("\nWiFi reconnection failed!");
  }

 //  if (ssType = 1) {
//tft.fillScreen(TFT_BLACK);
//}  
  // Toggle for the NEXT time inactivity happens
  ssType = (ssType == 0) ? 1 : 0; 

  tft.fillScreen(ST7735_BLACK);
  currentScreen = 1;
  viewMode = 2;
  fetchAPOD();//start over

  // redrawYourMainUI(); // Call your function to restore info
}





void typewriterFX() {
  tft.setTextFont(1);
tft.setTextColor(TFT_YELLOW, TFT_BLACK);
tft.setCursor(0, 50);
  String message = "I think you ought to know";
  
  for (int i = 0; i < message.length(); i++) {
    tft.print(message[i]); // Prints character by character
    delay(75);            // The "typing" speed
  }
  // No need for loop() or return here; the function ends naturally
}

void typewriterFX2() {
  tft.setTextFont(1);
tft.setTextColor(TFT_CYAN, TFT_BLACK);
tft.setCursor(50, 40);
  String message = "Dr. Cole's";
  
  for (int i = 0; i < message.length(); i++) {
    tft.print(message[i]); // Prints character by character
    delay(75);            // The "typing" speed
  }
tft.setCursor(35, 70);
  String messageb = "SpaceVision 3000";
  
  for (int i = 0; i < messageb.length(); i++) {
    tft.print(messageb[i]); // Prints character by character
    delay(75);            // The "typing" speed
  }

  // No need for loop() or return here; the function ends naturally
}



