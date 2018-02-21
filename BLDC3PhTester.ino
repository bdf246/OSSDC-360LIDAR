#include <LCD.h>
#include <LiquidCrystal_I2C.h>

// ----------------------------------------------------------------------
// Defines:
//
#define OUTPUT_PhaseA 6
#define OUTPUT_PhaseB 7
#define OUTPUT_PhaseC 8

#define DISPLAY_UPDATE_FREQ_IN_MS 300
#define DISPLAY_BLANKINGIME_IN_MS 100 // For updating ~4 digits, blank them for this time before update.
#define DISPLAY_UPDATE_DELAY_CHECK_IN_MS (DISPLAY_UPDATE_FREQ_IN_MS-DISPLAY_BLANKINGIME_IN_MS)

// ----------------------------------------------------------------------
// LCD Stuff
// Find your address from I2C Scanner function and add it here:
#define LCD_I2C 0x3F
#define LCD_BACKLIGHT_PIN 3
#define LCD_En_pin  2
#define LCD_Rw_pin  1
#define LCD_Rs_pin  0
#define LCD_D4_pin  4
#define LCD_D5_pin  5
#define LCD_D6_pin  6
#define LCD_D7_pin  7
LiquidCrystal_I2C  lcd(LCD_I2C,LCD_En_pin,LCD_Rw_pin,LCD_Rs_pin,LCD_D4_pin,LCD_D5_pin,LCD_D6_pin,LCD_D7_pin);


// ----------------------------------------------------------------------
// Main Logic Data Structures:

typedef struct {
    int powerLevel;
    int delay_in_ms;
    int curPhase;       // 1 to 6
} CONTROLCONTEXT_ST;

    
static CONTROLCONTEXT_ST controlContext = {0, 500, 1};
// ----------------------------------------------------------------------


void setup() {

    // From: https://forum.arduino.cc/index.php?topic=153645.0
    // Code for pins 9 and 10 to output at 20 kHz:
    // TCCR2B &= ~ _BV (CS22); // cancel pre-scaler of 64
    // TCCR2B |= _BV (CS21);   // use pre-scaler of 8 - Freq 20 kHz
    // TCCR2B |= _BV (CS20);   // no pre-scaler - Freq: 31.37 kHz
    // analogWrite (9, 50);    // 19.6 % duty cycle
    // analogWrite (10, 200);  // 78.4 % duty cycle

    // Code for pins 6, 7, 8
    TCCR4B &= ~ _BV (CS22); // cancel pre-scaler of 64
    // TCCR4B |= _BV (CS21);   // use pre-scaler of 8 - Freq 20 kHz
    TCCR4B |= _BV (CS20);   // no pre-scaler - Freq: 31.37 kHz
    // analogWrite (9, 50);    // 19.6 % duty cycle
    // analogWrite (10, 200);  // 78.4 % duty cycle
    
    // Debug:
    Serial.begin(9600);
    Serial.println("Starting!!!");


    // Setup PWM outputs:
    pinMode(OUTPUT_PhaseA, OUTPUT);
    pinMode(OUTPUT_PhaseB, OUTPUT);
    pinMode(OUTPUT_PhaseC, OUTPUT);

    // Setup POT outputs:
    pinMode(50,OUTPUT);
    pinMode(51,OUTPUT);
    pinMode(52,OUTPUT);
    pinMode(53,OUTPUT);
    digitalWrite(50,LOW);
    digitalWrite(51,HIGH);
    digitalWrite(52,LOW);
    digitalWrite(53,HIGH);

    // Setup LCD:
    lcd.begin (20,4);
    lcd.setBacklightPin(LCD_BACKLIGHT_PIN,POSITIVE);
    lcd.setBacklight(HIGH);
    lcd.home (); // go home
    lcd.print("BLDC Control");
    lcd.setCursor(0,1);
    lcd.print("Initializing..."); 
    delay(3000);
    lcdReset();
}

void lcdReset() {
    lcd.clear();
    lcd.home();
    // lcd.print("(X:");lcd.print(PacketsRX[1]);lcd.print(",Y:");lcd.print(PacketsRX[2]);lcd.print(") sum=");lcd.print(PacketsRX[0]);

    Serial.println("LCD Reset!!!");
    char line[41];

    lcd.clear();
    lcd.home();
    lcd.setCursor(0,0);
    sprintf(line, "Power:     [0-255]");
    lcd.print(line);

    lcd.setCursor(0,1);
    sprintf(line, "Delay:     [ms]");
    lcd.print(line);

    lcd.setCursor(0,2);
    sprintf(line, "Phase:     ");
    lcd.print(line);
}


static CONTROLCONTEXT_ST dispPrevContext = controlContext;
static CONTROLCONTEXT_ST dispPendingContext = controlContext;
static bool pendingUpdate = false;

// Blank out parts of display that need to change, then delay...
void UpdateDisplayBlankValues(CONTROLCONTEXT_ST & newContext) {
    char * text = "    ";

    if (newContext.powerLevel != dispPrevContext.powerLevel) {
        lcd.setCursor(6,0);
        lcd.print(text);
        pendingUpdate = true;
    }

    if (newContext.delay_in_ms != dispPrevContext.delay_in_ms) {
        lcd.setCursor(6,1);
        lcd.print(text);
        pendingUpdate = true;
    }

    if (newContext.curPhase != dispPrevContext.curPhase) {
        lcd.setCursor(6,2);
        lcd.print(text);
        pendingUpdate = true;
    }

    if (pendingUpdate) dispPendingContext = newContext;
}

// Update parts of display that have pending change...
void UpdateDisplay() {
    char text[10];

    if (dispPendingContext.powerLevel != dispPrevContext.powerLevel) {
        lcd.setCursor(6,0);
        sprintf(text, "%4d", dispPendingContext.powerLevel);
        lcd.print(text);
    }

    if (dispPendingContext.delay_in_ms != dispPrevContext.delay_in_ms) {
        lcd.setCursor(6,1);
        sprintf(text, "%4d", dispPendingContext.delay_in_ms);
        lcd.print(text);
    }

    if (dispPendingContext.curPhase != dispPrevContext.curPhase) {
        lcd.setCursor(6,2);
        sprintf(text, "%4d", dispPendingContext.curPhase);
        lcd.print(text);
    }

    dispPrevContext = dispPendingContext;
    pendingUpdate = false;
}

void loop()
{
    unsigned long currentTime = millis();
    static unsigned long prevDispTime = 0;
    static unsigned long prevPhaseTime = 0;
 
    // Read Pots:
    int pot1 = analogRead(A15);
    int pot2 = analogRead(A14);
    controlContext.powerLevel = pot1/4;
    controlContext.delay_in_ms = 10 + pot2*2;

    // Adjust power output:
    if ((currentTime - prevPhaseTime) > controlContext.delay_in_ms) {
        if (controlContext.curPhase < 6) controlContext.curPhase++;
        else                             controlContext.curPhase = 1;

        digitalWrite(OUTPUT_PhaseA, controlContext.powerLevel);

        prevPhaseTime = currentTime;
    }

    // Update display/debug:
    if (pendingUpdate) {
        // Write the pending update if delay has passed:
        if ((currentTime - prevDispTime) > DISPLAY_BLANKINGIME_IN_MS) {
            UpdateDisplay();
            prevDispTime = currentTime;
        }
    }
    else {
        // See if there is a change to get displayed:
        if ((currentTime - prevDispTime) > DISPLAY_UPDATE_DELAY_CHECK_IN_MS) {
            UpdateDisplayBlankValues(controlContext);
            prevDispTime = currentTime;
        }
    }
    
    // Debug info...
    // char buffer[200];
    // sprintf(buffer, "Power:%d, Delay:%d\n", controlContext.powerLevel, controlContext.delay_in_ms);
    // Serial.print(buffer);

    delay(1);
}

