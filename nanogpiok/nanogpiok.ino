#define RELAY_PIN 9
#define LEFT_BLINKER_PIN 5
#define RIGHT_BLINKER_PIN 6
#define HIGH_BEAM_PIN 7
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LEFT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(HIGH_BEAM_PIN, INPUT_PULLUP);
}

void loop() {
  bool left = !digitalRead(LEFT_BLINKER_PIN);
  bool right = !digitalRead(RIGHT_BLINKER_PIN);
  bool high = !digitalRead(HIGH_BEAM_PIN);

  Serial.print("LEFT:");  Serial.println(left);
  Serial.print("RIGHT:"); Serial.println(right);
  Serial.print("HIGH:");  Serial.println(high);

  while (Serial.available()) {                                  
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("RELAY:")) {
      int val = cmd.substring(6).toInt();
      digitalWrite(RELAY_PIN, val);
    }
  }

  delay(50);
}
