#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Access mode
enum class SlvCtrlAccess : uint8_t { RO, WO, RW, INVALID };

enum class SlvCtrlParseError : uint8_t {
  Ok = 0,
  MissingValue,
  NotANumber,
  OutOfRange,
  ReadOnly,
  UnsupportedType,
  InvalidValue
};

inline const char* slvCtrlParseErrorToString(SlvCtrlParseError e) {
  switch (e) {
    case SlvCtrlParseError::Ok:              return "ok";
    case SlvCtrlParseError::MissingValue:    return "missing_value";
    case SlvCtrlParseError::NotANumber:      return "not_a_number";
    case SlvCtrlParseError::OutOfRange:      return "out_of_range";
    case SlvCtrlParseError::ReadOnly:        return "read_only";
    case SlvCtrlParseError::UnsupportedType: return "unsupported_type";
    case SlvCtrlParseError::InvalidValue:    return "invalid_value";
    default:                                 return "unknown";
  }
}

inline const char* slvCtrlAccessToString(SlvCtrlAccess a) {
  switch (a) {
    case SlvCtrlAccess::RO: return "ro";
    case SlvCtrlAccess::WO: return "wo";
    case SlvCtrlAccess::RW: return "rw";
    default: return "?";
  }
}

struct ISlvCtrlTransport {
  virtual ~ISlvCtrlTransport() = default;
  virtual void readCommand() = 0;
};

// ---- Output abstraction (no Stream) ----
struct ISlvCtrlOut {
  virtual ~ISlvCtrlOut() = default;
  virtual void print(const char* s) = 0;
  virtual void print(uint32_t v) = 0;
  virtual void print(int32_t v) = 0;
  virtual void print(float v, uint8_t decimals = 3) = 0;
  virtual void print(bool v) = 0;
  virtual void println(const char* s = "") = 0;
};

// ---- Command context abstraction (no SerialCommands) ----
struct ISlvCtrlCmdCtx {
  virtual ~ISlvCtrlCmdCtx() = default;
  virtual const char* next() = 0;     // next token (like SerialCommands::Next())
  virtual ISlvCtrlOut& out() = 0;     // where to write responses
};

// ---- Attribute base ----
struct IAttribute {
  virtual ~IAttribute() = default;
  virtual const char* name() const = 0;
  virtual SlvCtrlAccess access() const = 0;
  virtual SlvCtrlParseError setFromCString(const char* s) = 0;
  virtual void writeValue(ISlvCtrlOut& out) const = 0;
  virtual void describe(ISlvCtrlOut& out) const = 0;
};

template <typename T>
class BaseAttribute : public IAttribute {
  public:
    using Getter = T (*)(void* ctx);
    using Setter = SlvCtrlParseError (*)(void* ctx, T value);

    BaseAttribute(const char* name, Getter g, Setter s, void* ctx = nullptr) : name_(name), ctx_(ctx), getter_(g), setter_(s) {
      if (!getter_ && !setter_) __builtin_trap();
    }

    ~BaseAttribute() override = default;

    const char* name() const override { return name_; }

    SlvCtrlAccess access() const override { 
      if (getter_ && setter_) { return SlvCtrlAccess::RW; }
      if (getter_ && !setter_) { return SlvCtrlAccess::RO; }
      if (!getter_ && setter_) { return SlvCtrlAccess::WO; }
      return SlvCtrlAccess::INVALID;
    }

    T getValue() const {
      return getter_ ? getter_(ctx_) : T{};
    }

    SlvCtrlParseError setValue(T value) {
      if (setter_) return setter_(ctx_, value);
      return SlvCtrlParseError::ReadOnly;
    }

    void writeValue(ISlvCtrlOut& out) const override {
      out.print(this->getValue());
    }

  private:
    const char* name_;
    void* ctx_;
    Getter getter_;
    Setter setter_;
};

class IntAttribute : public BaseAttribute<int32_t> {
  public:
    IntAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
    : BaseAttribute<int32_t>(name, getter, setter, ctx) {}
    
    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      long value = strtol(s, &end, 10);
      if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
      if (value < (long)INT32_MIN || value > (long)INT32_MAX) return SlvCtrlParseError::OutOfRange;
      return setValue((int32_t)value);
    }

    void describe(ISlvCtrlOut& out) const override {
      out.print(slvCtrlAccessToString(access()));
      out.print("[int]");
    }
};

class FloatAttribute : public BaseAttribute<float> {
  public:
    FloatAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
        : BaseAttribute<float>(name, getter, setter, ctx) {}

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      float v = strtof(s, &end);
      if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
      return setValue(v);
    }

    void describe(ISlvCtrlOut& out) const override {
      out.print(slvCtrlAccessToString(access()));
      out.print("[float]");
    }
};

class BoolAttribute : public BaseAttribute<bool> {
  public:
    BoolAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
        : BaseAttribute<bool>(name, getter, setter, ctx) {}

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0) { return setValue(true); }
      if (strcmp(s, "0") == 0 || strcasecmp(s, "false") == 0) { return setValue(false); }
      return SlvCtrlParseError::InvalidValue;
    }

    void describe(ISlvCtrlOut& out) const override {
      out.print(slvCtrlAccessToString(access()));
      out.print("[bool]");
    }

    void writeValue(ISlvCtrlOut& out) const override {
      out.print(this->getValue() ? "true" : "false");
    }
};

template <typename T>
class RangeAttribute : public BaseAttribute<T> {
  using Base   = BaseAttribute<T>;
  using Getter = typename Base::Getter;
  using Setter = typename Base::Setter;

  static_assert(
    std::is_same<T, int32_t>::value || std::is_same<T, float>::value,
    "RangeAttribute<T>: T must be int32_t or float"
  );

  public:
    RangeAttribute(const char* name, Getter getter, Setter setter, T min, T max, void* ctx = nullptr)
        : BaseAttribute<T>(name, getter, setter, ctx), min_(min), max_(max) {}

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      if constexpr (std::is_floating_point_v<T>) {
        float v = strtof(s, &end);
        if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
        if (v < min_ || v > max_) return SlvCtrlParseError::OutOfRange;
        return this->setValue(v);
      }
      
      if constexpr (std::is_integral_v<T>) {
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
        if (v < (long)INT32_MIN || v > (long)INT32_MAX) return SlvCtrlParseError::OutOfRange;
        T iv = (T)v; // T is int32_t here
        if (iv < min_ || iv > max_) return SlvCtrlParseError::OutOfRange;
        return this->setValue(iv);
      }

      __builtin_trap();
    }

    void describe(ISlvCtrlOut& out) const override {
      out.print(slvCtrlAccessToString(this->access()));
      out.print("[");
      if constexpr (std::is_same_v<T, int32_t>) {
        out.print("int:");
      }
      else if constexpr (std::is_same_v<T, float>) {
        out.print("float:");
      }
      else {
        out.print("?:");
      }
      out.print(min_);
      out.print("-");
      out.print(max_);
      out.print("]");
    }

  private:
    T min_;
    T max_;
};

class StrAttribute : public BaseAttribute<const char*> {
  public:
    StrAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
      : BaseAttribute<const char*>(name, getter, setter, ctx) {}

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      return setValue(s);
    }

    void describe(ISlvCtrlOut& out) const override {
      out.print(slvCtrlAccessToString(access()));
      out.print("[str]");
    }

    void writeValue(ISlvCtrlOut& out) const override {
      const char* s = this->getValue();
      out.print(s ? s : "");
    }
};

// ---- Protocol ----
//
// Commands:
//   introduce
//   attributes
//   status
//   get <name>
//   set <name> <value>
//
class SlvCtrlProtocol {
  public:
    static constexpr uint32_t protocolVersion = 10000;

    template <size_t N>
    SlvCtrlProtocol(const char* deviceType, uint32_t fwVersion, IAttribute* (&attrs)[N])
      : deviceType_(deviceType ? deviceType : "unknown"),
        fwVersion_(fwVersion),
        attrs_(&attrs[0]),
        attrCount_(N) {}

    void cmdIntroduce(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      o.print(deviceType_);
      o.print(";");
      o.print(fwVersion_);
      o.print(";");
      o.print(protocolVersion);
      o.println();
    }

    void cmdAttributes(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      o.print("attributes");
      if (attrCount_) o.print(";");

      bool first = true;
      for (size_t i = 0; i < attrCount_; ++i) {
        IAttribute* a = attrs_[i];
        if (!a) continue;

        if (!first) o.print(",");
        first = false;

        o.print(a->name());
        o.print(":");
        a->describe(o);
      }
      o.println();
    }

    void cmdStatus(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      o.print("status");
      if (attrCount_) o.print(";");

      bool first = true;
      for (size_t i = 0; i < attrCount_; ++i) {
        IAttribute* a = attrs_[i];
        if (!a || !canRead(a->access())) continue;

        if (!first) o.print(",");
        first = false;

        o.print(a->name());
        o.print(":");
        a->writeValue(o);
      }
      o.println();
    }

    void cmdGet(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      const char* name = ctx.next();
      if (!name) { o.println("ERR usage: get <name>"); return; }

      IAttribute* a = findAttr(name);
      if (!a) { o.println("ERR unknown attribute"); return; }
      if (!canRead(a->access())) { o.println("ERR write-only"); return; }

      o.print(a->name());
      o.print(";");
      a->writeValue(o);
      o.println(";status:success");
    }

    void cmdSet(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      const char* name  = ctx.next();
      const char* value = ctx.next();
      if (!name || !value) {
        o.println("ERR usage: set <name> <value>");
        return;
      }

      IAttribute* a = findAttr(name);
      if (!a) { o.println("ERR unknown attribute"); return; }
      if (!canWrite(a->access())) { o.println("ERR read-only"); return; }

      SlvCtrlParseError err = a->setFromCString(value);

      o.print(name);
      o.print(";");
      o.print(value);
      o.print(";");
      o.println(slvCtrlParseErrorToString(err));
    }

    void cmdUnrecognized(ISlvCtrlCmdCtx& ctx, const char* cmd) {
      auto& o = ctx.out();
      o.print("ERR unrecognized [");
      o.print(cmd ? cmd : "");
      o.println("]");
    }

  private:
    static bool canRead(SlvCtrlAccess a)  { return a == SlvCtrlAccess::RO || a == SlvCtrlAccess::RW; }
    static bool canWrite(SlvCtrlAccess a) { return a == SlvCtrlAccess::WO || a == SlvCtrlAccess::RW; }

    IAttribute* findAttr(const char* name) const {
      if (!name || !attrs_) return nullptr;
      for (size_t i = 0; i < attrCount_; ++i) {
        IAttribute* a = attrs_[i];
        if (a && strcmp(a->name(), name) == 0) return a;
      }
      return nullptr;
    }

  private:
    const char* deviceType_;
    uint32_t fwVersion_;
    IAttribute* const* attrs_;
    size_t attrCount_;
};
