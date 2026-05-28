#include "LineTrace.h"
#include <Arduino.h>
#include <ArduinoJson.h>

MotorControl::MotorControl(uint8_t ena, uint8_t in1, uint8_t in2, uint8_t enb, uint8_t in3, uint8_t in4)
    : ena(ena), in1(in1), in2(in2), enb(enb), in3(in3), in4(in4), left_speed(0), right_speed(0)
{
  pinMode(ena, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(enb, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
}
void MotorControl::rotate_left_motor() const
{
  digitalWrite(in1, left_speed > 0 ? HIGH : LOW);
  digitalWrite(in2, left_speed > 0 ? LOW : HIGH);
  analogWrite(ena, abs(left_speed));
}
void MotorControl::rotate_right_motor() const
{
  digitalWrite(in3, right_speed > 0 ? HIGH : LOW);
  digitalWrite(in4, right_speed > 0 ? LOW : HIGH);
  analogWrite(enb, abs(right_speed));
}
void MotorControl::setSpeed(int left_speed, int right_speed)
{
  this->left_speed = max(-255, min(255, left_speed));
  this->right_speed = max(-255, min(255, right_speed));
}
void MotorControl::rotate() const
{
  rotate_left_motor();
  rotate_right_motor();
}

LineTraceSensor::LineTraceSensor(uint8_t pin, uint16_t white_threshold, uint16_t black_threshold, uint8_t sample_size)
    : pin(pin), white_threshold(white_threshold), black_threshold(black_threshold), sample_size(sample_size)
{
  pinMode(pin, INPUT);
  read();
}
LineTraceSensor::LineTraceSensor(uint8_t pin, uint16_t threshold, uint8_t sample_size)
    : LineTraceSensor(pin, threshold, threshold, sample_size)
{
}
void LineTraceSensor::read()
{
  uint16_t total = 0;
  for (uint8_t i = 0; i < sample_size; ++i)
    total += analogRead(pin);

  sampled_value = total / sample_size;
}
bool LineTraceSensor::isOnBlack() const
{
  return sampled_value > black_threshold;
}
bool LineTraceSensor::isOnWhite() const
{
  return sampled_value < white_threshold;
}

SensorControl::SensorControl(LineTraceSensor left_junction, LineTraceSensor left_line_sensor, LineTraceSensor right_line_sensor, LineTraceSensor right_junction_sensor)
    : left_junction(left_junction), left_line_sensor(left_line_sensor), right_line_sensor(right_line_sensor), right_junction_sensor(right_junction_sensor)
{
}
void SensorControl::read()
{
  left_junction.read();
  left_line_sensor.read();
  right_line_sensor.read();
  right_junction_sensor.read();
}

Buzzer::Buzzer(uint8_t pin) : pin(pin)
{
  pinMode(pin, OUTPUT);
  start_time = millis();
  end_time = 0;
}
void Buzzer::set(unsigned long duration)
{
  start_time = millis();
  end_time = start_time + duration;
}
void Buzzer::play() const
{
  unsigned long current_time = millis();
  digitalWrite(pin, start_time <= current_time && current_time < end_time ? HIGH : LOW);
}

Map::Map(Position destination, Pose me)
    : destination(destination), me(me)
{
  for (size_t i = 0; i < SIZE; ++i)
    tiles[i][0] = tiles[i][SIZE + 1] = tiles[0][i] = tiles[SIZE + 1][i] = NodeType::OBSTACLE; // Set boundaries as obstacles

  for (size_t i = 0; i < SIZE; ++i)
    for (size_t j = 0; j < SIZE; ++j)
      tiles[i + 1][j + 1] = NodeType::EMPTY; // Set as empty tile
}
Map::Map() : Map(Position{1, 1}, Pose{Position{1, 1}, Map::Direction::NORTH})
{
}

Map::Position Map::getCurrentPosition() const
{
  return me.position;
}

Map::Direction Map::getCurrentDirection() const
{
  return me.direction;
}

void Map::setCurrentPosition(Position pos)
{
  me.position = pos;
}

void Map::setCurrentDirection(Direction dir)
{
  me.direction = dir;
}

void Map::moveOneStep()
{
  switch (me.direction)
  {
  case Direction::NORTH:
    me.position.y++;
    break;
  case Direction::EAST:
    me.position.x++;
    break;
  case Direction::SOUTH:
    me.position.y--;
    break;
  case Direction::WEST:
    me.position.x--;
    break;
  }
}

void Map::setDestination(Position dest)
{
  destination = dest;
}

Map::Position Map::getDestination() const
{
  return destination;
}

void Map::setObstacle(Position pos, bool is_obstacle)
{
  tiles[pos.x][pos.y] = is_obstacle ? NodeType::OBSTACLE : NodeType::EMPTY;
}
bool Map::isObstacle(Position pos) const
{
  return tiles[pos.x][pos.y] == NodeType::OBSTACLE;
}
size_t Map::searchPath(Position (&path)[MAX_PATH_LENGTH], Pose start) const
{
  struct Node
  {
    Position pos;
    Direction dir;
    uint8_t parent_index;
  };

  static constexpr size_t QUEUE_SIZE = MAX_PATH_LENGTH; // Maximum possible nodes in BFS
  Node queue[QUEUE_SIZE];
  size_t queue_front = 0;
  size_t queue_back = 0;
  bool visited[SIZE][SIZE] = {}; // [x][y]

  // Add starting position to queue
  queue[queue_back] = {start.position, start.direction, QUEUE_SIZE}; // 255 = no parent
  queue_back = (queue_back + 1) % QUEUE_SIZE;
  visited[start.position.x][start.position.y] = true;

  uint8_t target_node_index = QUEUE_SIZE;

  // BFS with rotation priority
  while (queue_front != queue_back)
  {
    Node current = queue[queue_front];
    uint8_t current_index = queue_front;
    queue_front = (queue_front + 1) % QUEUE_SIZE;

    // Check if reached destination
    if (current.pos.x == destination.x && current.pos.y == destination.y)
    {
      target_node_index = current_index;
      break;
    }

    // Generate moves in priority order: 직진, 좌회전, 우회전, U턴
    Direction moves[4];
    moves[0] = current.dir;                                 // 직진
    moves[1] = (Direction)(((uint8_t)current.dir + 3) % 4); // 좌회전 (-90°)
    moves[2] = (Direction)(((uint8_t)current.dir + 1) % 4); // 우회전 (+90°)
    moves[3] = (Direction)(((uint8_t)current.dir + 2) % 4); // U턴 (180°)

    for (uint8_t i = 0; i < 4; ++i)
    {
      Position next_pos = current.pos;
      Direction next_dir = moves[i];

      // Calculate next position based on direction
      switch (next_dir)
      {
      case Direction::NORTH:
        next_pos.y++;
        break;
      case Direction::EAST:
        next_pos.x++;
        break;
      case Direction::SOUTH:
        next_pos.y--;
        break;
      case Direction::WEST:
        next_pos.x--;
        break;
      }

      // Check bounds, obstacles, and if already visited
      if (tiles[next_pos.x][next_pos.y] == NodeType::OBSTACLE || visited[next_pos.x][next_pos.y])
        continue;

      // Add to queue if space available
      if (((queue_back + 1) % QUEUE_SIZE) != queue_front)
      {
        queue[queue_back] = {next_pos, next_dir, current_index};
        visited[next_pos.x][next_pos.y] = true;
        queue_back = (queue_back + 1) % QUEUE_SIZE;
      }
    }
  }

  // Reconstruct path if found
  if (target_node_index != QUEUE_SIZE)
  {
    Position temp_path[QUEUE_SIZE];
    size_t path_length = 0;
    uint8_t node_index = target_node_index;

    // Build path backwards
    while (node_index != QUEUE_SIZE && path_length < QUEUE_SIZE)
    {
      temp_path[path_length++] = queue[node_index].pos;
      node_index = queue[node_index].parent_index;
    }

    // Reverse path to get correct order
    path[0] = me.position;
    for (size_t i = 0; i < path_length && i + 1 < MAX_PATH_LENGTH; ++i)
      path[i + 1] = temp_path[path_length - 1 - i];

    return path_length;
  }
  else
  {
    // No path found - return current position only
    path[0] = me.position;
    return 1;
  }
}

LineTrace::LineTrace(MotorControl motor, SensorControl sensors, Buzzer debug_buzzer, SoftwareSerial serial, int drive_speed, int turn_speed)
    : command_index(0), commands{Command::HALT}, step(0), drive_speed(drive_speed), turn_speed(turn_speed), motor(motor), sensors(sensors), debug_buzzer(debug_buzzer), map(Map()), serial(serial)
{
}
void LineTrace::drive(int speed)
{
  if (sensors.left_line_sensor.isOnBlack() && sensors.right_line_sensor.isOnWhite())
    motor.setSpeed(-speed, speed);
  else if (sensors.left_line_sensor.isOnWhite() && sensors.right_line_sensor.isOnBlack())
    motor.setSpeed(speed, -speed);
  else
    motor.setSpeed(speed, speed);
}
void LineTrace::drive()
{
  drive(drive_speed);
}
void LineTrace::debug()
{
  debug_buzzer.set(100);
}

void LineTrace::checkJunctions(Map::Position pos, Map::Direction dir, bool &has_left, bool &has_right) const
{
  switch (dir)
  {
  case Map::Direction::NORTH:
    has_left = (pos.x > 1);          // Can go west (left)
    has_right = (pos.x < Map::SIZE); // Can go east (right)
    break;
  case Map::Direction::EAST:
    has_left = (pos.y < Map::SIZE); // Can go north (left)
    has_right = (pos.y > 1);        // Can go south (right)
    break;
  case Map::Direction::SOUTH:
    has_left = (pos.x < Map::SIZE); // Can go east (left)
    has_right = (pos.x > 1);        // Can go west (right)
    break;
  case Map::Direction::WEST:
    has_left = (pos.y > 1);          // Can go south (left)
    has_right = (pos.y < Map::SIZE); // Can go north (right)
    break;
  }
}
void LineTrace::setCommands(Command commands[], size_t command_count)
{
  // auto command = this->commands[command_index];
  // auto in_progress = command != Command::HALT;
  // auto offset = in_progress ? 1 : 0; // If in progress, keep the current command and shift others

  // this->commands[0] = command;
  size_t offset = 0; // No offset needed, always start from the beginning
  for (size_t i = 0; i < command_count && i + offset < MAX_COMMANDS; ++i)
    this->commands[i + offset] = commands[i];
  this->commands[min(command_count + offset, MAX_COMMANDS - 1)] = Command::HALT;

  this->command_index = 0;
}

void LineTrace::sendLocation()
{
  JsonDocument doc;
  auto pos = map.getCurrentPosition();
  auto dir = map.getCurrentDirection();
  doc["x"] = pos.x;
  doc["y"] = pos.y;
  doc["direction"] = static_cast<int>(dir);
  String json;
  serializeJson(doc, json);
  serial.print("LOC: ");
  serial.println(json);
  Serial.print("LOC: ");
  Serial.println(json);
}
void LineTrace::execute(int drive_speed, int turn_speed)
{
  auto command = commands[command_index];
  switch (command)
  {
  case Command::DRIVE_TO_LEFT_JUNCTION:
    switch (step)
    {
    case 0: // Drive until left junction sensor is on black
      if (sensors.left_junction.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        drive(drive_speed);
      break;
    case 1: // Drive until both junction sensor is on white
      if (sensors.areOnWhite(SensorControl::SensorPosition::LEFTJUNCTIONSENSOR, SensorControl::SensorPosition::RIGHTJUNCTIONSENSOR))
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        drive(drive_speed);
      break;
    case 2: // Done
      motor.setSpeed(0, 0);
      command_index++;
      debug();
      map.moveOneStep();
      step = 0; // Reset step for the next command
      break;
    default:
      // This case should not happen
      break;
    }
    break;
  case Command::DRIVE_TO_RIGHT_JUNCTION:
    switch (step)
    {
    case 0: // Drive until right junction sensor is on black
      if (sensors.right_junction_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        drive(drive_speed);
      break;
    case 1: // Drive until right junction sensor is on white
      if (sensors.areOnWhite(SensorControl::SensorPosition::LEFTJUNCTIONSENSOR, SensorControl::SensorPosition::RIGHTJUNCTIONSENSOR))
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        drive(drive_speed);
      break;
    case 2: // Done
      motor.setSpeed(0, 0);
      command_index++;
      debug();
      map.moveOneStep();
      step = 0; // Reset step for the next command
      break;
    default:
      // This case should not happen
      break;
    }
    break;
  case Command::TURN_LEFT:
    switch (step)
    {
    case 0: // Turn left until left junction sensor is on black
      if (sensors.left_junction.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(-turn_speed, turn_speed);
      break;
    case 1: // Turn left until left line sensor is on black
      if (sensors.left_line_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(-turn_speed, turn_speed);
      break;
    case 2: // Turn left until left line sensor is on white or right line sensor is on black
      if (sensors.left_line_sensor.isOnWhite() || sensors.right_line_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(-turn_speed, turn_speed);
      break;
    case 3: // Done
      motor.setSpeed(0, 0);
      command_index++;
      debug();
      map.setCurrentDirection((Map::Direction)(((uint8_t)map.getCurrentDirection() + 3) % 4)); // Turn left
      step = 0;                                                                                // Reset step for the next command
      break;
    default:
      // This case should not happen
      break;
    }
    break;
  case Command::TURN_RIGHT:
    switch (step)
    {
    case 0: // Turn right until right junction sensor is on black
      if (sensors.right_junction_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(turn_speed, -turn_speed);
      break;
    case 1: // Turn right until right line sensor is on black
      if (sensors.right_line_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(turn_speed, -turn_speed);
      break;
    case 2: // Turn right until right line sensor is on white or left line sensor is on black
      if (sensors.right_line_sensor.isOnWhite() || sensors.left_line_sensor.isOnBlack())
      {
        motor.setSpeed(0, 0);
        step++;
      }
      else
        motor.setSpeed(turn_speed, -turn_speed);
      break;
    case 3: // Done
      motor.setSpeed(0, 0);
      command_index++;
      debug();
      map.setCurrentDirection((Map::Direction)(((uint8_t)map.getCurrentDirection() + 1) % 4)); // Turn left
      step = 0;                                                                                // Reset step for the next command
      break;
    default:
      // This case should not happen
      break;
    }
    break;
  case Command::HALT:
    motor.setSpeed(0, 0);
    break;
  }
}
size_t LineTrace::pathToCommands(const Map::Position (&path)[Map::MAX_PATH_LENGTH], size_t path_length, Command (&commands)[MAX_COMMANDS]) const
{
  size_t command_count = 0;
  Map::Direction current_dir = map.getCurrentDirection();

  for (size_t i = 1; i < path_length && command_count < MAX_COMMANDS - 1; ++i)
  {
    Map::Position current_pos = path[i - 1];
    Map::Position next_pos = path[i];

    // Calculate required direction to move to next position
    Map::Direction required_dir;
    if (next_pos.x > current_pos.x)
      required_dir = Map::Direction::EAST;
    else if (next_pos.x < current_pos.x)
      required_dir = Map::Direction::WEST;
    else if (next_pos.y > current_pos.y)
      required_dir = Map::Direction::NORTH;
    else if (next_pos.y < current_pos.y)
      required_dir = Map::Direction::SOUTH;
    else
      continue; // Same position, skip

    // Add rotation commands if needed
    while (current_dir != required_dir && command_count < MAX_COMMANDS - 1)
    {
      int current_int = (int)current_dir;
      int required_int = (int)required_dir;
      int diff = (required_int - current_int + 4) % 4;

      if (diff == 1)
      { // Right turn
        commands[command_count++] = Command::TURN_RIGHT;
        current_dir = (Map::Direction)((current_int + 1) % 4);
      }
      else if (diff == 3)
      { // Left turn
        commands[command_count++] = Command::TURN_LEFT;
        current_dir = (Map::Direction)((current_int + 3) % 4);
      }
      else if (diff == 2)
      { // U-turn - check which direction has junction
        bool can_turn_left = false;
        bool can_turn_right = false;

        checkJunctions(current_pos, current_dir, can_turn_left, can_turn_right);

        // Choose available direction for U-turn (prefer left for consistency)
        if (can_turn_left)
        {
          commands[command_count++] = Command::TURN_LEFT;
          current_dir = (Map::Direction)((current_int + 3) % 4);
        }
        else if (can_turn_right)
        {
          commands[command_count++] = Command::TURN_RIGHT;
          current_dir = (Map::Direction)((current_int + 1) % 4);
        }
        else
        {
          // No junction available - this shouldn't happen, but default to left
          commands[command_count++] = Command::TURN_LEFT;
          current_dir = (Map::Direction)((current_int + 3) % 4);
        }
      }
    }

    // Determine appropriate movement command based on current position and direction
    if (command_count < MAX_COMMANDS - 1)
    {
      bool has_left_junction = false;
      bool has_right_junction = false;

      checkJunctions(current_pos, current_dir, has_left_junction, has_right_junction);

      // Choose appropriate command
      if (has_right_junction)
        commands[command_count++] = Command::DRIVE_TO_RIGHT_JUNCTION;
      else if (has_left_junction)
        commands[command_count++] = Command::DRIVE_TO_LEFT_JUNCTION;
      else
        // No junction available, this shouldn't happen in valid path
        commands[command_count++] = Command::DRIVE_TO_RIGHT_JUNCTION;
    }
  }

  // Add HALT at the end
  if (command_count < MAX_COMMANDS)
    commands[command_count++] = Command::HALT;

  return command_count;
}

void LineTrace::navigateToDestination(Map::Position dest)
{
  auto destination = map.getDestination();
  if (destination.x == dest.x && destination.y == dest.y)
    // Not updating destination if it's the same
    return;
  // Set destination
  map.setDestination(dest);

  // Search path from current position to destination
  Map::Position path[Map::MAX_PATH_LENGTH];

  auto start_pos = map.getCurrentPosition();
  auto last_command = commands[command_index];
  switch (last_command)
  {
  case LineTrace::Command::DRIVE_TO_LEFT_JUNCTION:
  case LineTrace::Command::DRIVE_TO_RIGHT_JUNCTION:
    switch (map.getCurrentDirection())
    {
    case Map::Direction::NORTH:
      start_pos.y++;
      break;
    case Map::Direction::EAST:
      start_pos.x++;
      break;
    case Map::Direction::SOUTH:
      start_pos.y--;
      break;
    case Map::Direction::WEST:
      start_pos.x--;
      break;
    }
    break;
  default:
    // No adjustment needed for other commands
    break;
  }

  auto start_dir = map.getCurrentDirection();
  switch (last_command)
  {
  case LineTrace::Command::TURN_LEFT:
    start_dir = (Map::Direction)(((uint8_t)start_dir + 3) % 4); // 좌회전 (-90°)
    break;
  case LineTrace::Command::TURN_RIGHT:
    start_dir = (Map::Direction)(((uint8_t)start_dir + 1) % 4); // 우회전 (+90°)
    break;
  default:
    // No adjustment needed for other commands
    break;
  }

  size_t path_length = map.searchPath(path, Map::Pose{start_pos, start_dir});

  // Convert path to commands
  Command commands[MAX_COMMANDS];
  size_t command_count = pathToCommands(path, path_length, commands);

  // Set commands to execute
  setCommands(commands, command_count);
}

void LineTrace::addObstacle(Map::Position pos)
{
  if (map.isObstacle(pos))
    return; // Obstacle already exists, no need to add
  // Add obstacle to map
  map.setObstacle(pos, true);

  // Recalculate route if we have a destination
  Map::Position current_dest = map.getDestination();
  Map::Position current_pos = map.getCurrentPosition();

  // Only recalculate if we're not at the destination
  if (current_dest.x != current_pos.x || current_dest.y != current_pos.y)
  {
    navigateToDestination(current_dest);
  }
}

void LineTrace::removeObstacle(Map::Position pos)
{
  if (!map.isObstacle(pos))
    return; // No obstacle exists, no need to remove
  // Remove obstacle from map
  map.setObstacle(pos, false);

  // Recalculate route if we have a destination
  Map::Position current_dest = map.getDestination();
  Map::Position current_pos = map.getCurrentPosition();

  // Only recalculate if we're not at the destination
  if (current_dest.x != current_pos.x || current_dest.y != current_pos.y)
  {
    navigateToDestination(current_dest);
  }
}

void LineTrace::execute()
{
  execute(drive_speed, turn_speed);
}
