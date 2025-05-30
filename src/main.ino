#include <FastLED.h>

#define NUM_LEDS 23
#define DATA_PIN 8
#define BUTTON_PIN 2
#define BAUD 115200
#define CLOCK_SPEED 16000000
#define MYUBRR CLOCK_SPEED/16/BAUD-1


CRGB leds[NUM_LEDS]; // arr pt cculorile ledurilor 

volatile unsigned long millis_counter = 0;
volatile bool updateNeeded = false;
int mode = 0;
int fadeColorIndex = 0;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 1000000; // 1 s
int fadeBrightness = 0;
int fadeDirection = 1;

uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;

void USART0_init(unsigned int ubrr) {
  UBRR0H = (unsigned char)(ubrr >> 8);
  UBRR0L = (unsigned char)ubrr;

  UCSR0B = (1 << RXEN0) | (1 << TXEN0); // activeaza receptia si trasnmiterea 
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data
}

void USART0_transmit(char data) {
  while (!(UCSR0A & (1 << UDRE0)));
  UDR0 = data;
}

char USART0_receive() {
  while (!(UCSR0A & (1 << RXC0)));
  return UDR0;
}


ISR(TIMER1_COMPA_vect) {
  if (mode == 2) {  // doar în mode 2
    millis_counter++; // inc la fiecare ms
    if (millis_counter >= 30) { // la fiecare 30 ms
      millis_counter = 0;
      fadeBrightness += fadeDirection * 5;  // schimba intensitatea cu pas de 5

      if (fadeBrightness >= 255) {
        fadeBrightness = 255;
        fadeDirection = -1;  // incepe sa scada
      } else if (fadeBrightness <= 0) {
        fadeBrightness = 0;
        fadeDirection = 1;   // incepe sa creasca
        fadeColorIndex = (fadeColorIndex + 1) % 3;  // schimba culoarea dupa un ciclu complet
      }
      updateNeeded = true;
    }
  }
}


ISR(INT0_vect) {
  if ((long)(micros() - lastDebounceTime) >= debounceDelay) { // debounce
    mode = (mode + 1) % 4;
    lastDebounceTime = micros();
    updateNeeded = true;
  }
}

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(BAUD);

  USART0_init(MYUBBR); 

  // External Interrupt on INT0 (pin 2)
  EICRA |= (1 << ISC00) | (1 << ISC01);
  EIMSK |= (1 << INT0);

  // Timer1 setup
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 249;          // 1ms tick - 16MHz/64 prescaler
  TCCR1B |= (1 << WGM12); // mod ctc
  TCCR1B |= (1 << CS11) | (1 << CS10); // prescaler 64
  TIMSK1 |= (1 << OCIE1A); // activare intrerupere
  sei();

  updateNeeded = true;
}

void loop() {
  if (updateNeeded) {
    if (mode == 3) {
      PCMode();
    } else {
      updateLEDs();
      updateNeeded = false;
    }
  }
}

void updateLEDs() {
  CRGB color;

  switch (mode) {
    case 0:
      color = CRGB::Red;
      break;
    case 1:
      color = CRGB::Green;
      break;
    case 2:
      CRGB baseColor;
      switch (fadeColorIndex) {
        case 0: baseColor = CRGB::Magenta; break;
        case 1: baseColor = CRGB::Cyan; break;
        case 2: baseColor = CRGB::Blue; break;
      }
      color = baseColor.nscale8_video(fadeBrightness);

      break;
  }

  fill_solid(leds, NUM_LEDS, color); // seteaza culoarea pt toate led urile
  FastLED.show();
}

void PCMode() {
  if (UCSR0A & (1 << RXC0)) { // exista date primite
    for (i = 0; i < sizeof prefix; ++i) {
    waitLoop:
      while (!(UCSR0A & (1 << RXC0)));
      if (prefix[i] == USART0_receive()) continue;
      i = 0;
      goto waitLoop;
    }

    while (!(UCSR0A & (1 << RXC0)));
    hi = USART0_receive();
    while (!(UCSR0A & (1 << RXC0)));
    lo = USART0_receive();
    while (!(UCSR0A & (1 << RXC0)));
    chk = USART0_receive();

    if (chk != (hi ^ lo ^ 0x55)) {
      i = 0;
      goto waitLoop;
    }

    memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      uint8_t r, g, b;
      while (!(UCSR0A & (1 << RXC0))); r = USART0_receive();
      while (!(UCSR0A & (1 << RXC0))); g = USART0_receive();
      while (!(UCSR0A & (1 << RXC0))); b = USART0_receive();
      leds[i].r = r;
      leds[i].g = g;
      leds[i].b = b;
    }

    FastLED.show();
  }
}