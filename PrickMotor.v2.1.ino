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

int xpos = 0;
int xmax_pos = 400;
int xwalk;

int ypos = 0;
int ymax_pos = 200;
int ywalk;

int * current_pos;
int * current_max_pos;

bool cal_active = false;
int unsigned set_cnt = 0;

// Steps to go down from zero at skin, twice up
int zrange = 60;
// Minimum Steps to walk randomly in XY-Direction
int minwalk = 100;
// Elasticity-Compensation for XY-Direction (moving a bit farther and then back)
int elast_comp = 40;
//Velocity in microseconds for z-direction
int zdly = 4000;
//Velocity in microseconds for xy-direction
int xydly = 4000;
//Step-Count for one Calibration step
int calsteps = 25;
// Delay for Calibration in microseconds
int caldly = 4000;
//Latency between Down and Up in miliseconds
int ontime = 1200;
//Latency between Z-Movement and XY-Movement in miliseconds
int after_stim = 4000;

void set_microstepping() {
  // Set Microstepping-Mode (in combination with hardwired Mode 1 and Mode 2)
  int micro_mode = 8;
  int mode3 = HIGH;
  int mode4 = HIGH;
  
  digitalWrite(STEPX, mode3);
  digitalWrite(DIRX, mode4);

  digitalWrite(STEPY, mode3);
  digitalWrite(DIRY, mode4);

  digitalWrite(STEPZ, mode3);
  digitalWrite(DIRZ, mode4);

  digitalWrite(ALLSTBY, HIGH);

  zdly /= micro_mode;
  xydly /= micro_mode;
  caldly /= micro_mode;
}

void move_motor(char motor_type, int dly, int step_count, char direct) {
  int dir;
  int stp;
  int en;

  if(motor_type=='x'){
    dir = DIRX;
    stp = STEPX;
    en = ENX;
  }else if(motor_type=='y'){
    dir = DIRY;
    stp = STEPY;
    en = ENY;
  }else if(motor_type=='z'){
    dir = DIRZ;
    stp = STEPZ;
    en = ENZ;
  }

  // Set Direction
  if(direct=="left"){
    digitalWrite(dir, HIGH);
  }else{
    digitalWrite(dir, LOW);
  }

  // Enable Motor
  digitalWrite(en, LOW);
  
  for(int step = 0; step <= step_count; step++){
    digitalWrite(stp, HIGH);
    delayMicroseconds(dly);
    digitalWrite(stp, LOW);
    delayMicroseconds(dly);
  }

  if(motor_type!="z"){
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

void motor_calibration(char motor_type) {
  int calstp = calsteps;
  int caldl = caldly;

  if(motor_type=='x'){
    // Adjust to double StepCount for X-Motor
    calstp = calsteps * 2;
    caldl = caldly / 2;
  }
  
  while(cal_active==true){
    // Increase Direction for calsteps
    if(bin_chan()==52){
      move_motor(motor_type, caldl, calstp, "left");
      *current_pos += calstp;
    }
    
    // Decrease Direction for calsteps
    if(bin_chan()==48){
      move_motor(motor_type, caldl, calstp, "right");
      *current_pos -= calstp;
    }
    
    // Set Min/Max depending on set_cnt and motor_type
    if(bin_chan()==56){
      if(motor_type == 'z'){
        move_motor(motor_type, caldl, *current_max_pos, "right");
        delay(500);
        move_motor(motor_type, caldl, *current_max_pos * 3, "left");
        cal_active = false;
      }else if(set_cnt == 0){
        *current_pos = 0;
        *current_max_pos = 0;
        set_cnt = 1;
        delay(500);
      }else if(set_cnt == 1){
        *current_max_pos = *current_pos;
        set_cnt = 0;
        cal_active = false;
        delay(500);
      }
    }
  }
}


void setup() {
  // put your setup code here, to run once:

  // Control IO
  pinMode(A0, OUTPUT); // Start-Motor-Signal (from Arduino)
  pinMode(A1, OUTPUT); // Stop-Motor-Signal (from Arduino)
  pinMode(A2, INPUT); // Binary Inputs from Matlab 4
  pinMode(A3, INPUT); // Binary Inputs from Matlab 8
  pinMode(A4, INPUT); // Binary Inputs from Matlab 16
  pinMode(A5, INPUT); // Binary Inputs from Matlab 32
  
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

  // Set Microstepping
  set_microstepping();
  
}

void loop() {
// put your main code here, to run repeatedly:

  // Select Motors
  // Motor X-Direction
  if(bin_chan()==36){

    cal_active = true;
    current_pos = &xpos;
    current_max_pos = &xmax_pos;
    
    motor_calibration('x');
  }
  
  // Motor Y-Direction
  if(bin_chan()==40){

    cal_active = true;

    current_pos = &ypos;
    current_max_pos = &ymax_pos;
    
    motor_calibration('y');
  }
  
  // Motor Z-Direction
  if(bin_chan()==44){

    cal_active = true;
    current_pos = 0;
    current_max_pos = &zrange;
    
    motor_calibration('z');
    
  }

  // Center XY-Direction
  if(bin_chan()==60){
    // Center X-Direction
    xwalk = xmax_pos / 2 - xpos;
    if(xwalk > 0){
      move_motor('x', xydly, abs(xwalk), "left");
    }else{
      move_motor('x', xydly, abs(xwalk), "right");
    }
    xpos = xmax_pos / 2;

    // Center Y-Direction
    ywalk = ymax_pos / 2 - ypos;
    if(ywalk > 0){
      move_motor('y', xydly, abs(ywalk), "left");
    }else{
      move_motor('y', xydly, abs(ywalk), "right");
    }
    ypos = ymax_pos / 2;
  }


//  // uses old Matlab Prickstim to drive range of X and Y
//  if(bin_chan()==40){
//    move_motor('x', xydly, xmax_pos, "left");
//    delay(1000);
//    move_motor('x', xydly, xmax_pos, "right");
//
//    move_motor('y', xydly, xmax_pos, "left");
//    delay(1000);
//    move_motor('y', xydly, xmax_pos, "right");
//  }

  // Start Sequence
  if(bin_chan()==32){
    
    // Send Down-Trigger
    digitalWrite(A0, HIGH);
    delay(10);
    digitalWrite(A0, LOW);
    
    // Move down in Z-Axis
    move_motor('z', zdly, zrange * 3, "right");
    
    delay(ontime);
    
    // Send Up-Trigger
    digitalWrite(A1, HIGH);
    delay(10);
    digitalWrite(A1, LOW);
    
    // Move up in Z-Axis
    move_motor('z', zdly, zrange * 3, "left");

    // Randomly move X-Axis in given boundaries
    xwalk = random(0 - xpos + minwalk, xmax_pos - xpos);
    xpos += xwalk;

    if(xwalk > 0){
      move_motor('x', xydly, abs(xwalk), "left");
    }else{
      move_motor('x', xydly, abs(xwalk), "right");
    }

    // Randomly move Y-Axis in given boundaries
    ywalk = random(0 - ypos + minwalk, ymax_pos - ypos);
    ypos += ywalk;

    if(ywalk > 0){
      move_motor('y', xydly, abs(ywalk), "left");
    }else{
      move_motor('y', xydly, abs(ywalk), "right");
    }
    
  }

}