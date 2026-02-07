#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <climits>

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

inline size_t slvClampSnprintfResult(char* out, size_t outLen, int n) {
  if (!out || outLen == 0) return 0;
  if (n < 0) { out[0] = '\0'; return 0; }
  // snprintf returns "would have written" count; clamp if truncated
  if ((size_t)n >= outLen) return outLen - 1;
  return (size_t)n;
}

struct ISlvCtrlTransport {
  virtual ~ISlvCtrlTransport() = default;
  virtual void readCommand() = 0;
};

// ---- Output abstraction (no Stream) ----
struct ISlvCtrlOut {
  virtual ~ISlvCtrlOut() = default;
  virtual void print(const char* s) = 0;
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
  virtual const char* typeName() const = 0;
  virtual SlvCtrlAccess access() const = 0;
  virtual SlvCtrlParseError setFromCString(const char* s) = 0;
  virtual size_t formatValue(char* out, size_t outLen) const = 0;
  virtual size_t describeTo(char* out, size_t outLen) const = 0;
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

    const char* typeName() const override = 0;

    const char* name() const override { return name_; }

    SlvCtrlAccess access() const override { 
      if (getter_ && setter_) { return SlvCtrlAccess::RW; }
      if (getter_ && !setter_) { return SlvCtrlAccess::RO; }
      if (!getter_ && setter_) { return SlvCtrlAccess::WO; }
      return SlvCtrlAccess::INVALID;
    }

    SlvCtrlParseError setFromCString(const char* s) override = 0;

    T getValue() const {
      return getter_ ? getter_(ctx_) : T{};
    }

    SlvCtrlParseError setValue(T value) {
      if (setter_) return setter_(ctx_, value);
      return SlvCtrlParseError::ReadOnly;
    }

    size_t describeTo(char* out, size_t outLen) const override {
      int n = snprintf(out, outLen, "%s[%s]", slvCtrlAccessToString(access()), typeName());
      return slvClampSnprintfResult(out, outLen, n);
    }

  private:
    const char* name_;
    void* ctx_;
    Getter getter_;
    Setter setter_;
};

class IntAttribute : public BaseAttribute<int> {
  public:
    IntAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
    : BaseAttribute<int>(name, getter, setter, ctx) {}

    const char* typeName() const override { return "int"; }
    
    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      long value = strtol(s, &end, 10);
      if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
      if (value < INT_MIN || value > INT_MAX) return SlvCtrlParseError::OutOfRange;
      return setValue((int)value);
    }

    size_t formatValue(char* out, size_t outLen) const override {
      int n = snprintf(out, outLen, "%d", this->getValue());
      return slvClampSnprintfResult(out, outLen, n);
    }
};

class FloatAttribute : public BaseAttribute<float> {
  public:
    FloatAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
        : BaseAttribute<float>(name, getter, setter, ctx) {}

    const char* typeName() const override { return "float"; }

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      float v = strtof(s, &end);
      if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
      return setValue(v);
    }

    size_t formatValue(char* out, size_t outLen) const override {
      int n = snprintf(out, outLen, "%.3f", (double)this->getValue());
      return slvClampSnprintfResult(out, outLen, n);
    }
};

class BoolAttribute : public BaseAttribute<bool> {
  public:
    BoolAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
        : BaseAttribute<bool>(name, getter, setter, ctx) {}

    const char* typeName() const override { return "bool"; }

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0) { return setValue(true); }
      if (strcmp(s, "0") == 0 || strcasecmp(s, "false") == 0) { return setValue(false); }
      return SlvCtrlParseError::InvalidValue;
    }

    size_t formatValue(char* out, size_t outLen) const override {
      const char* s = this->getValue() ? "true" : "false";
      int n = snprintf(out, outLen, "%s", s);
      return slvClampSnprintfResult(out, outLen, n);
    }
};

template <typename T>
class RangeAttribute : public BaseAttribute<T> {
  using Base   = BaseAttribute<T>;
  using Getter = typename Base::Getter;
  using Setter = typename Base::Setter;

  static_assert(
    std::is_same<T, int>::value || std::is_same<T, float>::value,
    "RangeAttribute<T>: T must be int or float"
  );

  public:
    RangeAttribute(const char* name, Getter getter, Setter setter, T min, T max, void* ctx = nullptr)
        : BaseAttribute<T>(name, getter, setter, ctx), min_(min), max_(max) {

      if constexpr (std::is_floating_point_v<T>) {
        int n = snprintf(type_, sizeof(type_), "%.3f-%.3f", (double)min_, (double)max_);
        (void)slvClampSnprintfResult(type_, sizeof(type_), n);
      } else if constexpr (std::is_integral_v<T>) {
        int n = snprintf(type_, sizeof(type_), "%d-%d", (int)min_, (int)max_);
        (void)slvClampSnprintfResult(type_, sizeof(type_), n);
      }
    }

    const char* typeName() const override { 
      return type_;
    }

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      char* end = nullptr;
      if constexpr (std::is_floating_point_v<T>) {
        float v = strtof(s, &end);
        if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
        if (v < min_ || v > max_) return SlvCtrlParseError::OutOfRange;
        return this->setValue((T)v);
      }
      
      if constexpr (std::is_integral_v<T>) {
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0') return SlvCtrlParseError::NotANumber;
        if (v < (long)min_ || v > (long)max_) return SlvCtrlParseError::OutOfRange;
        return this->setValue((T)v);
      }

      __builtin_trap();
    }

    size_t formatValue(char* out, size_t outLen) const override {
      if constexpr (std::is_floating_point_v<T>) {
        int n = snprintf(out, outLen, "%.3f", (double)this->getValue());
        return slvClampSnprintfResult(out, outLen, n);
      } else {
        int n = snprintf(out, outLen, "%d", (int)this->getValue());
        return slvClampSnprintfResult(out, outLen, n);
      }
    }

  private:
    T min_;
    T max_;
    char type_[32];
};

class StrAttribute : public BaseAttribute<const char*> {
  public:
    StrAttribute(const char* name, Getter getter, Setter setter, void* ctx = nullptr)
      : BaseAttribute<const char*>(name, getter, setter, ctx) {}

    const char* typeName() const override { return "str"; }

    SlvCtrlParseError setFromCString(const char* s) override {
      if (!s) return SlvCtrlParseError::MissingValue;
      return setValue(s);
    }

    size_t formatValue(char* out, size_t outLen) const override {
      const char* s = this->getValue();
      int n = snprintf(out, outLen, "%s", s ? s : "");
      return slvClampSnprintfResult(out, outLen, n);
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
    static constexpr uint32_t kProtocolVersion = 10000;

    template <size_t N>
    SlvCtrlProtocol(const char* deviceType,
                    uint32_t fwVersion,
                    IAttribute* (&attrs)[N])
      : deviceType_(deviceType ? deviceType : "unknown"),
        fwVersion_(fwVersion),
        attrs_(&attrs[0]),
        attrCount_(N) {}

    void cmdIntroduce(ISlvCtrlCmdCtx& ctx) {
      char buf[96];
      int n = snprintf(buf, sizeof(buf), "%s;%lu;%lu",
                      deviceType_,
                      (unsigned long)fwVersion_,
                      (unsigned long)kProtocolVersion);
      (void)slvClampSnprintfResult(buf, sizeof(buf), n);
      ctx.out().println(buf);
    }

    void cmdAttributes(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      o.print("attributes");
      if (attrCount_) o.print(";");

      char desc[64];

      for (size_t i = 0; i < attrCount_; ++i) {
        if (i) o.print(",");
        IAttribute* a = attrs_[i];
        if (!a) continue;

        a->describeTo(desc, sizeof(desc));

        o.print(a->name());
        o.print(":");
        o.print(desc);
      }
      o.println();
    }

    void cmdStatus(ISlvCtrlCmdCtx& ctx) {
      auto& o = ctx.out();
      o.print("status");
      if (attrCount_) o.print(";");

      for (size_t i = 0; i < attrCount_; ++i) {
        if (i) o.print(",");
        IAttribute* a = attrs_[i];
        if (!a) continue;

        o.print(a->name());
        o.print(":");

        if (canRead(a->access())) {
          char v[64];
          a->formatValue(v, sizeof(v));
          o.print(v);
        }
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

      char v[64];
      a->formatValue(v, sizeof(v));

      o.print(a->name());
      o.print(";");
      o.print(v);
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

      char buf[192];
      int n = snprintf(buf, sizeof(buf), "%s;%s;%s",
                      name, value, slvCtrlParseErrorToString(err));
      (void)slvClampSnprintfResult(buf, sizeof(buf), n);
      o.println(buf);
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
