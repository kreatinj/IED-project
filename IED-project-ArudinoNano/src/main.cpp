#include <Arduino.h>
#include <ArduinoJson.h>
#include <NewPing.h>
#include "LineTrace.h"

#define ENA 9   // 모터 제어 드라이버의 ENA
#define IN1 4   // 모터 제어 드라이버의 IN1
#define IN2 6   // 모터 제어 드라이버의 IN2
#define ENB 5   // 모터 제어 드라이버의 ENB
#define IN3 7   // 모터 제어 드라이버의 IN3
#define IN4 8   // 모터 제어 드라이버의 IN4
#define SLL A0  // 왼쪽 라인트레이서 센서
#define SCL A1  // 중앙 왼쪽 라인트레이서 센서
#define SCR A2  // 중앙 오른쪽 라인트레이서 센서
#define SRR A3  // 오른쪽 라인트레이서 센서
#define TRIG 11 // 초음파센서 TRIG
#define ECHO 10 // 초음파센서 ECHO
#define RX 2    // 소프트웨어 시리얼 RX 핀
#define TX 3    // 소프트웨어 시리얼 TX 핀
#define BUZ 12  // DEBUG 부저 핀

LineTrace line_trace(
    MotorControl(ENA, IN1, IN2, ENB, IN3, IN4),
    SensorControl(
        LineTraceSensor(SLL, 70, (uint16_t)200), // 왼쪽 라인트레이서 센서
        LineTraceSensor(SCL, 70, (uint16_t)200), // 중앙 왼쪽 라인트레이서 센서
        LineTraceSensor(SCR, 70, (uint16_t)200), // 중앙 오른쪽 라인트레이서 센서
        LineTraceSensor(SRR, 70, (uint16_t)200)  // 오른쪽 라인트레이서 센서
        ),
    Buzzer(BUZ),
    SoftwareSerial(RX, TX), // 소프트웨어 시리얼 포트 (RX, TX)
    140, 150);              // 라인트레이서 객체 생성

NewPing sonar(TRIG, ECHO, 500); // 초음파 센서 객체 생성

const String DEST_COMMAND = "DEST: ";
const String INSERT_OBS_COMMAND = "INSERT_OBS: ";
const String REMOVE_OBS_COMMAND = "REMOVE_OBS: ";

void handleDestinationCommand(const String &);
void handleInsertObstacleCommand(const String &);
void handleRemoveObstacleCommand(const String &);

void setup()
{
  Serial.begin(115200); // TODO: remove this line after debugging
  line_trace.serial.begin(38400);
}

void loop()
{
  if (line_trace.serial.available())
  {
    auto str = line_trace.serial.readStringUntil('\n');
    if (str.length() > 0)
    {
      if (str.startsWith(DEST_COMMAND))
        handleDestinationCommand(str);
      else if (str.startsWith(INSERT_OBS_COMMAND))
        handleInsertObstacleCommand(str);
      else if (str.startsWith(REMOVE_OBS_COMMAND))
        handleRemoveObstacleCommand(str);
    }
  }
  line_trace.sendLocation();

  line_trace.sensors.read();

  auto distance = sonar.ping_cm();
  if (0 < distance && distance < 10) // 10cm 이하일 때
    line_trace.motor.setSpeed(0, 0); // 모터 정지
  else
    line_trace.execute();

  line_trace.motor.rotate();

  line_trace.debug_buzzer.play();
}

void handleDestinationCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(DEST_COMMAND.length()));
  uint8_t x = doc["x"];
  uint8_t y = doc["y"];

  // Set destination and navigate
  line_trace.navigateToDestination({x, y});
}

void handleInsertObstacleCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(INSERT_OBS_COMMAND.length()));
  uint8_t x = doc["x"];
  uint8_t y = doc["y"];

  // Add obstacle and recalculate route
  line_trace.addObstacle({x, y});
}

void handleRemoveObstacleCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(REMOVE_OBS_COMMAND.length()));
  uint8_t x = doc["x"];
  uint8_t y = doc["y"];

  // Remove obstacle and recalculate route
  line_trace.removeObstacle({x, y});
}
