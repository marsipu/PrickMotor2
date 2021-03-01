#include <Arduino.h>

// define Arduino-Pins
// Motor X-Direction (400 Steps/round)
#define DIRX 2
#define STEPX 3
#define ENX 4  // Enable Pin, inverted loginc

// Motor Y-Direction (200 Steps/round)
#define DIRY 5
#define STEPY 6
#define ENY 7  // Enable Pin, inverted loginc

// Motor Z-Direction (200 Steps/round)
#define DIRZ 8
#define STEPZ 9
#define ENZ 10  // Enable Pin, inverted loginc

// All Standby-Inputs from all drivers together
#define ALLSTBY 11

#define MOTOR_STOP_DELAY_MS 20
static const int micro_mode = 8;
static const int mode3 = HIGH;
static const int mode4 = HIGH;

int xpos = 0;
int xmax_pos = 0;
int xwalk;

int ypos = 0;
int ymax_pos = 0;
int ywalk;

int zpos = 0;

int * current_pos;
int * current_max_pos;
int unsigned set_cnt = 0;

// Number of Repetitions for calibration tests
int n_repetitions = 40;

// Steps to go down from zero at skin, twice up
int zrange = 60 * micro_mode;
// Multiplicator for Z
static const int zmulti = 5;
// Minimum time (ms) for Random Start-Delay
static const int start_dly_min = 100;
// Maximum time (ms) for Random Start-Delay
static const int start_dly_max = 2000;
// Minimum Steps to walk randomly in XY-Direction
static const int minwalk = 10 * micro_mode;
// Elasticity-Compensation for XY-Direction (moving a bit farther and then back)
static const int elast_comp = 20 * micro_mode;
//Step-Count for one Calibration step
static const int calsteps = 25 * micro_mode;
//Velocity in microseconds for z-direction
int zdly = 4000 / micro_mode;
//Velocity in microseconds for xy-direction
static const int xydly = 4000 / micro_mode;
// Delay for Calibration in microseconds
static const int caldly = 4000 / micro_mode;
//Latency between Down and Up in miliseconds
static const int ontime = 1200;
//Latency between Z-Movement and XY-Movement in miliseconds
static const int after_stim = 4000;

void set_microstepping() {
  // Set Microstepping-Mode (in combination with hardwired Mode 1 and Mode 2)
  digitalWrite(STEPX, mode3);
  digitalWrite(DIRX, mode4);

  digitalWrite(STEPY, mode3);
  digitalWrite(DIRY, mode4);

  digitalWrite(STEPZ, mode3);
  digitalWrite(DIRZ, mode4);

  digitalWrite(ALLSTBY, HIGH);
}

void move_motor(char motor_type, int dly, int step_count, char direct) {
  int dir;
  int stp;
  int en;
  int el_comp = elast_comp;

  if(motor_type=='x'){
    dir = DIRX;
    stp = STEPX;
    en = ENX;
    // Adjust to double step-count for motor x
    step_count *= 2;
    dly /= 2;
    el_comp *= 2;
  }else if(motor_type=='y'){
    dir = DIRY;
    stp = STEPY;
    en = ENY;
  }else if(motor_type=='z'){
    dir = DIRZ;
    stp = STEPZ;
    en = ENZ;
  }

  if(motor_type!='z'){
    // Increase Step-Count to account for Fiber-Elasticity in X and Y
    step_count += el_comp;
  }

  // Set Direction
  if(direct=='l'){
    digitalWrite(dir, HIGH);
  }else{
    digitalWrite(dir, LOW);
  }

  // Enable Motor
  digitalWrite(en, LOW);
  
  for(int step = 0; step < step_count; step++){
    digitalWrite(stp, HIGH);
    delayMicroseconds(dly);
    digitalWrite(stp, LOW);
    delayMicroseconds(dly);
  }

  if(motor_type!='z'){
    // wait shortly before changing direction
    delay(MOTOR_STOP_DELAY_MS);

    // Reverse Movemont to account for Fiber-Elasticity in X and Y
    if(direct=='l'){
      digitalWrite(dir, LOW);
    }else{
      digitalWrite(dir, HIGH);
    }
    
    for(int step = 0; step < el_comp; step++){
      digitalWrite(stp, HIGH);
      delayMicroseconds(dly);
      digitalWrite(stp, LOW);
      delayMicroseconds(dly);
    }
    // not disabling drivers before motor has come to standstill
    delay(MOTOR_STOP_DELAY_MS);

    // Only X and Y are disabled after movement, Z stays on since calibration
    digitalWrite(en, HIGH);
  }
  
}

int bin_chan() {
  int input1 = digitalRead(A2) * 4 + digitalRead(A3) * 8 + digitalRead(A4) * 16 + digitalRead(A5) * 32;
  delayMicroseconds(100);
  int input2 = digitalRead(A2) * 4 + digitalRead(A3) * 8 + digitalRead(A4) * 16 + digitalRead(A5) * 32;
  if(input1 == input2){
    return input1;
  }else{
    return 0;
  }
}

void centerx() {
    Serial.println("Centering");
    // Center X-Direction
    xwalk = xmax_pos / 2 - xpos;
    if(xwalk > 0){
      move_motor('x', xydly, abs(xwalk), 'l');
    }else{
      move_motor('x', xydly, abs(xwalk), 'r');
    }
    xpos = xmax_pos / 2;
}

void centery() {
    // Center Y-Direction
    ywalk = ymax_pos / 2 - ypos;
    if(ywalk > 0){
      move_motor('y', xydly, abs(ywalk), 'l');
    }else{
      move_motor('y', xydly, abs(ywalk), 'r');
    }
    ypos = ymax_pos / 2;
}

void motor_calibration(char motor_type) {
  int calstp = calsteps;
  int caldl = caldly;
  bool moved = false;

  Serial.print("MotorCalib: ");
  Serial.println(motor_type);
  
  while(true){
    // Change mode if not synchronous to matlab
    if(bin_chan()==36){
      motor_type=='x';
      current_pos = &xpos;
      current_max_pos = &xmax_pos;
    }else if(bin_chan()==40){
      motor_type=='y';
      current_pos = &ypos;
      current_max_pos = &ymax_pos;
    }else if(bin_chan()==44){
      motor_type=='z';
      current_pos = &zpos;
      current_max_pos = &zrange;
    }
    
    // Break if quit
    if(bin_chan()==60){
      break;
    }
    
    // Increase Direction for calsteps
    if(bin_chan()==52){
      Serial.print("Increase Motor ");
      Serial.println(motor_type);
      move_motor(motor_type, caldl, calstp, 'l');
      *current_pos += calstp;
      moved = true;
    }
    
    // Decrease Direction for calsteps
    if(bin_chan()==48){
      Serial.print("Decrease Motor ");
      Serial.println(motor_type);
      move_motor(motor_type, caldl, calstp, 'r');
      *current_pos -= calstp;
      moved = true;
    }

    // Set Min/Max depending on set_cnt and motor_type
    if(bin_chan()==56){
      if(motor_type == 'z'){
        if(moved==false){
          Serial.println("Skip Set");
          break;
        }
        Serial.println("Z-Calib-Set");
        move_motor(motor_type, caldl, *current_max_pos, 'r');
        move_motor(motor_type, caldl, *current_max_pos * zmulti, 'l');
        break;
      }else if(set_cnt == 0){  // Motor-Type is X or Y if not Z
        if(moved==false){
          Serial.println("Skip Set");
          break;
        }
        Serial.println("XY-Calib 1");
        *current_pos = 0;
        *current_max_pos = 0;
        set_cnt = 1;
        delay(400);
      }else if(set_cnt == 1){
        if(moved==false){
          Serial.println("Skip Set");
          break;
        }
        Serial.println("XY-Calib 2");
        *current_max_pos = *current_pos;
        set_cnt = 0;
        delay(400);
        break;
      }
    }
  }
}


void setup() {
  // put your setup code here, to run once:

  // Control IO
  pinMode(A0, INPUT); // Stop
  pinMode(A1, INPUT); // Start
  pinMode(A2, INPUT); // Analog Input from Potentiometer for Velocity
  pinMode(A3, OUTPUT);  // Direction-Signal (from Arduino)

  // Motor IO
  pinMode(DIRX, OUTPUT);
  pinMode(STEPX, OUTPUT);
  pinMode(ENX, OUTPUT);
  pinMode(DIRY, OUTPUT);
  pinMode(STEPY, OUTPUT);
  pinMode(ENY, OUTPUT);
  pinMode(DIRZ, OUTPUT);
  pinMode(STEPZ, OUTPUT);
  pinMode(ENZ, OUTPUT);
  pinMode(ALLSTBY, OUTPUT);

  // Deactivate all Motors
  digitalWrite(ENX, HIGH);
  digitalWrite(ENY, HIGH);
  digitalWrite(ENZ, HIGH);

  // Set Direction-Signal to Low
  digitalWrite(A1, LOW);

  // Set Microstepping
  set_microstepping();

  Serial.begin(9600);
  
}

void loop() {
// put your main code here, to run repeatedly:

  // Disable Motor in Pauses to make movement possible
  digitalWrite(ENZ, HIGH);

  if(digitalRead(A1)==1){
    // Start Sequence
    for(int i=0; i<n_repetitions; i++){

      if(digitalRead(A0)==1){
        break;
      }
      
      // Read Speed-Value from Potentiometer
      int velo_value = analogRead(A2);
      if(velo_value<333){
        zdly = 4000 / micro_mode;
      }else if(333<velo_value && velo_value<666){
        zdly = 2000 / micro_mode;
      }else if(666<velo_value){
        zdly = 1000 / micro_mode;
      }
      Serial.println(velo_value);
      Serial.println(zdly);

      // Insert Random-Delay
      delay(random(start_dly_min, start_dly_max));
      Serial.println(i);
      // Send Down-Direction
      digitalWrite(A3, HIGH);
      
      // Move down in Z-Axis
      move_motor('z', zdly, zrange * zmulti, 'r');
      
      delay(ontime);
      
      // Send Up-Direction
      digitalWrite(A3, LOW);
      
      // Move up in Z-Axis
      move_motor('z', zdly, zrange * zmulti, 'l');
    }
  }

}