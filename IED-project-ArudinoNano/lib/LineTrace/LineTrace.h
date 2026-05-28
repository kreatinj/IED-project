#ifndef Linetrace_h
#define Linetrace_h
#include <stdint.h>
#include <stddef.h>
#include <SoftwareSerial.h>

class MotorControl
{
public:
  const uint8_t ena; // Motor A enable pin
  const uint8_t in1; // Motor A input pin 1
  const uint8_t in2; // Motor A input pin 2
  const uint8_t enb; // Motor B enable pin
  const uint8_t in3; // Motor B input pin 1
  const uint8_t in4; // Motor B input pin 2
  int left_speed;    // Speed for left motor
  int right_speed;   // Speed for right motor
  void rotate_left_motor() const;
  void rotate_right_motor() const;

public:
  MotorControl(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
  void setSpeed(int, int);
  void rotate() const;
};

class LineTraceSensor
{
private:
  const uint8_t pin;              // Pin number
  const uint16_t white_threshold; // White threshold value
  const uint16_t black_threshold; // Black threshold value
  const uint8_t sample_size;      // Number of samples to read for averaging
  uint16_t sampled_value;         // Last sampled value

public:
  LineTraceSensor(uint8_t, uint16_t, uint16_t, uint8_t = 2U);
  LineTraceSensor(uint8_t, uint16_t, uint8_t = 2U);
  void read();
  bool isOnBlack() const;
  bool isOnWhite() const;
};

class SensorControl
{
public:
  enum SensorPosition
  {
    LEFTJUNCTIONSENSOR, // left junction sensor
    LEFTLINESENSOR,     // left line sensor
    RIGHTLINESENSOR,    // right line sensor
    RIGHTJUNCTIONSENSOR // right junction sensor
  };

private:
  inline bool areOnBlack() const
  {
    return true;
  }
  inline bool areOnWhite() const
  {
    return true;
  }

public:
  SensorControl(LineTraceSensor, LineTraceSensor, LineTraceSensor, LineTraceSensor);
  LineTraceSensor left_junction;         // left junction sensor
  LineTraceSensor left_line_sensor;      // left line sensor
  LineTraceSensor right_line_sensor;     // right line sensor
  LineTraceSensor right_junction_sensor; // right junction sensor
  void read();
  template <typename... Args>
  bool areOnBlack(SensorPosition sensor, Args... sensors) const
  {
    switch (sensor)
    {
    case SensorPosition::LEFTJUNCTIONSENSOR:
      if (!left_junction.isOnBlack())
        return false;
      break;
    case SensorPosition::LEFTLINESENSOR:
      if (!left_line_sensor.isOnBlack())
        return false;
      break;
    case SensorPosition::RIGHTLINESENSOR:
      if (!right_line_sensor.isOnBlack())
        return false;
      break;
    case SensorPosition::RIGHTJUNCTIONSENSOR:
      if (!right_junction_sensor.isOnBlack())
        return false;
    }
    return areOnBlack(sensors...);
  }
  template <typename... Args>
  bool areOnWhite(SensorPosition sensor, Args... sensors) const
  {
    switch (sensor)
    {
    case SensorPosition::LEFTJUNCTIONSENSOR:
      if (!left_junction.isOnWhite())
        return false;
      break;
    case SensorPosition::LEFTLINESENSOR:
      if (!left_line_sensor.isOnWhite())
        return false;
      break;
    case SensorPosition::RIGHTLINESENSOR:
      if (!right_line_sensor.isOnWhite())
        return false;
      break;
    case SensorPosition::RIGHTJUNCTIONSENSOR:
      if (!right_junction_sensor.isOnWhite())
        return false;
    }
    return areOnWhite(sensors...);
  }
};

class Buzzer
{
private:
  const uint8_t pin; // Buzzer pin
  unsigned long start_time;
  unsigned long end_time;

public:
  Buzzer(uint8_t);
  void set(unsigned long);
  void play() const;
};

class Map
{
public:
  enum class Direction
  {
    NORTH,
    EAST,
    SOUTH,
    WEST,
  };
  enum class NodeType
  {
    EMPTY,    // Empty tile
    OBSTACLE, // Obstacle tile
  };
  struct Position
  {
    uint8_t x; // X coordinate
    uint8_t y; // Y coordinate
  };
  struct Pose
  {
    Position position;   // Position on the map
    Direction direction; // Direction facing
  };
  static constexpr size_t SIZE = 4;                          // Map size (4x4 grid)
  static constexpr size_t MAX_PATH_LENGTH = SIZE * SIZE + 1; // Maximum possible path length

private:
  NodeType tiles[SIZE + 2][SIZE + 2]; // Map tiles, with padding for boundaries
  Position destination;               // Destination position
  Pose me;                            // Current position and direction

public:
  Map(Position, Pose);
  Map();

  // Position and direction accessors
  Position getCurrentPosition() const;
  Direction getCurrentDirection() const;
  void setCurrentPosition(Position);
  void setCurrentDirection(Direction);
  void moveOneStep();
  void setDestination(Position);
  Position getDestination() const;
  void setObstacle(Position, bool);
  bool isObstacle(Position) const;
  size_t searchPath(Position (&)[MAX_PATH_LENGTH], Pose) const; // Search path from current position to destination
};

class LineTrace
{
public:
  enum class Command
  {
    DRIVE_TO_LEFT_JUNCTION,  // go to the left side junction
    DRIVE_TO_RIGHT_JUNCTION, // go to the right side junction
    TURN_LEFT,               // turn left
    TURN_RIGHT,              // turn right
    HALT
  };
  static constexpr size_t MAX_COMMANDS = 64; // Maximum number of commands

private:
  size_t command_index;           // Current command index
  Command commands[MAX_COMMANDS]; // List of commands to execute
  uint8_t step;                   // Current step in the command
  int drive_speed;                // Speed for driving
  int turn_speed;                 // Speed for turning
  void drive(int);
  void drive();
  void debug();
  void checkJunctions(Map::Position, Map::Direction, bool &, bool &) const;

public:
  LineTrace(MotorControl, SensorControl, Buzzer, SoftwareSerial, int = 180, int = 150);
  MotorControl motor;    // Motor control object
  SensorControl sensors; // Sensor control object
  Buzzer debug_buzzer;   // Debug buzzer object
  Map map;               // Map object
  SoftwareSerial serial; // Serial communication object
  void setCommands(Command[], size_t);
  void sendLocation();
  size_t pathToCommands(const Map::Position (&)[Map::MAX_PATH_LENGTH], size_t, Command (&)[MAX_COMMANDS]) const;
  void navigateToDestination(Map::Position);
  void addObstacle(Map::Position);
  void removeObstacle(Map::Position);
  void execute(int, int);
  void execute();
};

#endif