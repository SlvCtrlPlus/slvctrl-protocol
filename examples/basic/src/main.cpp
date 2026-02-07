#include <Arduino.h>

#include <SlvCtrlProtocol.h>
#include <SlvCtrlArduinoSerialCommandsTransport.h>

// --------- your state ----------
static int32_t  speed    = 0;
static int32_t  maxSpeed = 100;
static int32_t  state    = 0;
static bool ready    = false;

// --------- getters/setters (C function pointers) ----------
// Note: your attribute classes use function pointers: T(*)(void*), not lambdas.
static int32_t getSpeed(void*) { return speed; }
static SlvCtrlParseError setSpeed(void*, int32_t v) { speed = v; return SlvCtrlParseError::Ok; }

static int32_t getMaxSpeed(void*) { return maxSpeed; }
static SlvCtrlParseError setMaxSpeed(void*, int32_t v) { maxSpeed = v; return SlvCtrlParseError::Ok; }

static int32_t getState(void*) { return state; }
static SlvCtrlParseError setState(void*, int32_t v) { state = v; return SlvCtrlParseError::Ok; }

static bool getReady(void*) { return ready; }

// --------- attributes ----------
static IntAttribute speedAttr("speed", &getSpeed, &setSpeed);
static RangeAttribute<int32_t> maxAttr("max", &getMaxSpeed, &setMaxSpeed, 0, 100);
static IntAttribute stateAttr("state", &getState, &setState);
static BoolAttribute readyAttr("ready", &getReady, nullptr);

static IAttribute* attrs[] = {
  &speedAttr,
  &maxAttr,
  &stateAttr,
  &readyAttr,
};

// --------- protocol + transport ----------
static SlvCtrlProtocol proto("nogasm", 10000, attrs);

// SerialCommands requires a command buffer:
static char cmdBuf[128];

// Transport wires SerialCommands -> proto command handlers
static SlvCtrlSerialCommandsTransport transport(
  Serial,
  proto,
  cmdBuf,
  sizeof(cmdBuf),
  "\n",   // command terminator
  " "     // arg delimiter
);

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("Ready:");
  Serial.println("  introduce");
  Serial.println("  attributes");
  Serial.println("  status");
  Serial.println("  get speed");
  Serial.println("  set speed 42");
}

void loop() {
  transport.readCommand();   // reads serial and dispatches commands
}
