#pragma once

#include <Arduino.h>
#include <SerialCommands.h>

#include <SlvCtrlProtocol.h>

// Output adapter: ISlvCtrlOut -> Arduino Stream
class SlvCtrlArduinoOut final : public ISlvCtrlOut {
  public:
    explicit SlvCtrlArduinoOut(Stream& s) : s_(s) {}

    void send(const char* s) override {
        s_.print(s ? s : "");
    }

    void send(char v) override {
        s_.print(v);
    }

    void send(int32_t v) override {
        // Arduino Print uses long for signed integers
        s_.print((long)v);
    }

    void send(uint32_t v) override {
        // Arduino Print uses unsigned long for unsigned integers
        s_.print((unsigned long)v);
    }

    void send(float v, uint8_t decimals = 3) override {
        s_.print(v, decimals);
    }

    void send(bool v) override {
        s_.print(v);
    }

  private:
    Stream& s_;
};

// Command ctx adapter: ISlvCtrlCmdCtx -> SerialCommands
class SlvCtrlArduinoCmdCtx final : public ISlvCtrlCmdCtx {
public:
  SlvCtrlArduinoCmdCtx(SerialCommands& sender, ISlvCtrlOut& out)
    : sender_(sender), out_(out) {}

  const char* next() override { return sender_.Next(); }
  ISlvCtrlOut& out() override { return out_; }

private:
  SerialCommands& sender_;
  ISlvCtrlOut& out_;
};

class SlvCtrlSerialCommandsTransport final : public ISlvCtrlTransport {
public:
  SlvCtrlSerialCommandsTransport(Stream& serial,
                                 SlvCtrlProtocol& proto,
                                 char* cmdBuffer,
                                 size_t cmdBufferSize,
                                 const char* cmdTerm = "\n",
                                 const char* argDelim = " ")
    : serial_(serial),
      proto_(proto),
      out_(serial_),
      serialCommands_(&serial_, cmdBuffer, cmdBufferSize, cmdTerm, argDelim),
      cmdStatus_("status", &SlvCtrlSerialCommandsTransport::onStatus),
      cmdIntroduce_("introduce", &SlvCtrlSerialCommandsTransport::onIntroduce),
      cmdAttributes_("attributes", &SlvCtrlSerialCommandsTransport::onAttributes),
      cmdGet_("get", &SlvCtrlSerialCommandsTransport::onGet),
      cmdSet_("set", &SlvCtrlSerialCommandsTransport::onSet) {

    active_ = this;

    serialCommands_.SetDefaultHandler(&SlvCtrlSerialCommandsTransport::onUnrecognized);
    serialCommands_.AddCommand(&cmdStatus_);
    serialCommands_.AddCommand(&cmdIntroduce_);
    serialCommands_.AddCommand(&cmdAttributes_);
    serialCommands_.AddCommand(&cmdGet_);
    serialCommands_.AddCommand(&cmdSet_);
  }

  ~SlvCtrlSerialCommandsTransport() override {
    if (active_ == this) active_ = nullptr;
  }

  void readCommand() override { serialCommands_.ReadSerial(); }

private:
  static void onStatus(SerialCommands* s)     { if (active_ && s) active_->handleStatus(*s); }
  static void onIntroduce(SerialCommands* s)  { if (active_ && s) active_->handleIntroduce(*s); }
  static void onAttributes(SerialCommands* s) { if (active_ && s) active_->handleAttributes(*s); }
  static void onGet(SerialCommands* s)        { if (active_ && s) active_->handleGet(*s); }
  static void onSet(SerialCommands* s)        { if (active_ && s) active_->handleSet(*s); }
  static void onUnrecognized(SerialCommands* s, const char* cmd) {
    if (active_ && s) active_->handleUnrecognized(*s, cmd);
  }

  void handleStatus(SerialCommands& s) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdStatus(ctx);
  }
  void handleIntroduce(SerialCommands& s) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdIntroduce(ctx);
  }
  void handleAttributes(SerialCommands& s) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdAttributes(ctx);
  }
  void handleGet(SerialCommands& s) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdGet(ctx);
  }
  void handleSet(SerialCommands& s) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdSet(ctx);
  }
  void handleUnrecognized(SerialCommands& s, const char* cmd) {
    SlvCtrlArduinoCmdCtx ctx(s, out_);
    proto_.cmdUnrecognized(ctx, cmd);
  }

private:
  Stream& serial_;
  SlvCtrlProtocol& proto_;
  SlvCtrlArduinoOut out_;

  SerialCommands serialCommands_;
  SerialCommand cmdStatus_;
  SerialCommand cmdIntroduce_;
  SerialCommand cmdAttributes_;
  SerialCommand cmdGet_;
  SerialCommand cmdSet_;

  static SlvCtrlSerialCommandsTransport* active_;
};
