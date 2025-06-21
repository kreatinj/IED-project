#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <asyncHTTPrequest.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include <stdio.h>
#include <WiFi.h>

enum class Direction
{
  NORTH,
  EAST,
  SOUTH,
  WEST,
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

String directionToString(Direction dir)
{
  switch (dir)
  {
  case Direction::NORTH:
    return "N";
  case Direction::EAST:
    return "E";
  case Direction::SOUTH:
    return "S";
  case Direction::WEST:
    return "W";
  }
  return "UNKNOWN"; // Default case if direction is not recognized
}

static constexpr size_t SIZE = 4;                    // Size of the grid (4x4)
static constexpr size_t MAX_OBSTACLES = SIZE * SIZE; // Maximum number of obstacles

Position destination = Position{1, 1};                      // Destination coordinates
Pose current_pose = Pose{Position{1, 1}, Direction::NORTH}; // Current location and direction
size_t obstacleCount = 0;                                   // Number of obstacles detected
Position obstacles[MAX_OBSTACLES];                          // Array to hold obstacles

HardwareSerial serial(2);

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

static AsyncWebServer server(80);
static AsyncCorsMiddleware cors;
asyncHTTPrequest request;

String stringifyPosition(Position position)
{
  JsonDocument doc;
  doc["x"] = position.x;
  doc["y"] = position.y;
  String str;
  serializeJson(doc, str);
  return str;
}
String stringifyPose(Pose pose)
{
  JsonDocument doc;
  doc["x"] = pose.position.x;
  doc["y"] = pose.position.y;
  doc["direction"] = directionToString(pose.direction);
  String str;
  serializeJson(doc, str);
  return str;
}
String stringifyObstacles(Position *obstacles, size_t count)
{
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (size_t i = 0; i < count; i++)
  {
    doc["obstacles"][i]["x"] = obstacles[i].x;
    doc["obstacles"][i]["y"] = obstacles[i].y;
  }
  String str;
  serializeJson(doc, str);
  return str;
}

static const String CENTRALIZED_CENTER_URL = "http://192.168.0.84:9000";
static const String CENTRALIZED_CENTER_OBSTACLES_URL = CENTRALIZED_CENTER_URL + "/api/obstacles";

unsigned long lastObstacleUpdate = 0;
static constexpr unsigned long OBSTACLE_UPDATE_INTERVAL = 100;
bool requestInProgress = false;

bool disabled_send_destination = false;
bool disabled_send_obstacles = false;

void setup()
{
  Serial.begin(115200);
  serial.begin(38400);

  Serial.println("Starting WiFi connection...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected successfully");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  cors.setOrigin("*");
  cors.setMethods("*");
  cors.setHeaders("Content-Type");
  cors.setAllowCredentials(false);
  server.addMiddleware(&cors);

  server.on("/api/destination", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    if (index + len == total) {
      JsonDocument doc;
      auto error = deserializeJson(doc, (const char*)data, len);

      if (!error && doc["x"].is<uint8_t>() && doc["y"].is<uint8_t>()) {
        destination.x = doc["x"];
        destination.y = doc["y"];
        disabled_send_destination = false; // Enable sending destination again
        request->send(200, "text/plain", "Destination set successfully");
      } else {
        request->send(400, "text/plain", "Invalid JSON or missing parameters");
      }
    } });

  server.on("/api/destination", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              auto content = stringifyPosition(destination);
              request->send(200, "application/json", content); });

  server.on("/api/location", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              auto content = stringifyPose(current_pose);
              request->send(200, "application/json", content); });

  server.on("/api/obstacles", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              auto content = stringifyObstacles(obstacles, obstacleCount);
              request->send(200, "application/json", content); });
  server.begin();
}

void onObstaclesResponse(void *optParm, asyncHTTPrequest *request, int readyState)
{
  if (readyState == 4)
  {
    requestInProgress = false;
    if (request->responseHTTPcode() == 200)
    {
      auto payload = request->responseText();
      JsonDocument doc;
      auto error = deserializeJson(doc, payload);

      if (!error)
      {
        JsonArray obstaclesArray = doc["obstacles"];

        for (JsonObject obstacle : obstaclesArray)
        {
          bool exists = false;
          for (size_t i = 0; i < obstacleCount; i++)
          {
            if (obstacles[i].x == obstacle["x"] && obstacles[i].y == obstacle["y"])
            {
              exists = true;
              break;
            }
          }
          if (!exists)
          {
            auto stringified_obstacle = stringifyPosition(Position{obstacle["x"], obstacle["y"]});
            serial.print("INSERT_OBS: ");
            serial.println(stringified_obstacle);
            disabled_send_obstacles = false; // Enable sending obstacles again
          }
        }

        for (size_t i = 0; i < obstacleCount; i++)
        {
          bool exists = false;
          for (JsonObject obstacle : obstaclesArray)
          {
            if (obstacle["x"] == obstacles[i].x && obstacle["y"] == obstacles[i].y)
            {
              exists = true;
              break;
            }
          }
          if (!exists)
          {
            auto stringified_obstacle = stringifyPosition(obstacles[i]);
            serial.print("REMOVE_OBS: ");
            serial.println(stringified_obstacle);
            disabled_send_obstacles = false; // Enable sending obstacles again
          }
        }

        obstacleCount = 0;
        for (JsonObject obstacle : obstaclesArray)
        {
          if (obstacleCount < MAX_OBSTACLES)
          {
            obstacles[obstacleCount].x = obstacle["x"];
            obstacles[obstacleCount].y = obstacle["y"];
            obstacleCount++;
          }
        }
      }
    }
  }
}

void updateObstacles()
{
  if (!requestInProgress)
  {
    requestInProgress = true;
    request.onReadyStateChange(onObstaclesResponse);
    request.open("GET", CENTRALIZED_CENTER_OBSTACLES_URL.c_str());
    request.send();
    lastObstacleUpdate = millis();
  }
}

static constexpr size_t DATA_SIZE = 256;
char buffer[DATA_SIZE];
size_t buffer_index = 0;

const String LOC_COMMAND = "LOC: ";
void handleLocationCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(LOC_COMMAND.length()));
  current_pose.position.x = doc["x"];
  current_pose.position.y = doc["y"];
  current_pose.direction = (Direction)doc["direction"].as<int>();
}

const String DEST_COMMAND = "DEST: ";
void handleDestinationCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(DEST_COMMAND.length()));
  if (destination.x == doc["x"] && destination.y == doc["y"])
    disabled_send_destination = true;
}

const String OBS_COMMAND = "OBS: ";
void handleObstaclesCommand(const String &command)
{
  JsonDocument doc;
  deserializeJson(doc, command.substring(OBS_COMMAND.length()));
  auto array = doc.as<JsonArray>();
  if (array.size() != obstacleCount)
  {
    disabled_send_obstacles = false; // Disable sending obstacles if the count doesn't match
    return;
  }
  else
  {
    for (size_t i = 0; i < obstacleCount; i++)
    {
      if (array[i]["x"] != obstacles[i].x || array[i]["y"] != obstacles[i].y)
      {
        disabled_send_obstacles = false; // Disable sending obstacles if any position doesn't match
        return;
      }
    }
    disabled_send_obstacles = true; // Disable sending obstacles if all positions match
    return;
  }
}

static constexpr unsigned long DESTINATION_SEND_INTERVAL = 50;
unsigned long lastDestinationSend = 0;
void sendDestination()
{
  auto stringified_destination = stringifyPosition(destination);
  serial.print(DEST_COMMAND);
  serial.println(stringified_destination);
  lastDestinationSend = millis();
}

static constexpr unsigned long OBSTACLE_SEND_INTERVAL = 50;
unsigned long lastObstacleSend = 0;
void sendObstacles()
{
  auto stringified_obstacles = stringifyObstacles(obstacles, obstacleCount);
  serial.print("OBS: ");
  serial.println(stringified_obstacles);
  lastObstacleSend = millis();
}

void loop()
{
  if (!disabled_send_destination && millis() - lastDestinationSend >= DESTINATION_SEND_INTERVAL)
    sendDestination();
  if (!disabled_send_obstacles && millis() - lastObstacleSend >= OBSTACLE_SEND_INTERVAL)
    sendObstacles();

  if (millis() - lastObstacleUpdate >= OBSTACLE_UPDATE_INTERVAL)
    updateObstacles();

  while (serial.available() && buffer_index < DATA_SIZE - 1)
  {
    char c = serial.read();
    if (c == '\n' || c == '\r') // 줄바꿈 문자 처리
    {
      buffer[buffer_index] = '\0';   // 문자열 종료
      auto command = String(buffer); // 버퍼 내용을 문자열로 변환
      if (command.startsWith(LOC_COMMAND))
        handleLocationCommand(command);
      else if (command.startsWith(DEST_COMMAND))
        handleDestinationCommand(command);
      else if (command.startsWith(OBS_COMMAND))
        handleObstaclesCommand(command);
      buffer_index = 0; // 버퍼 초기화
    }
    else
      buffer[buffer_index++] = c; // 버퍼에 문자 추가
  }
}
