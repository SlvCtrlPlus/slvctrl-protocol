# SlvCtrlProtocol

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/slvctrlplus/library/slvctrl-protocol.svg)](https://registry.platformio.org/libraries/slvctrlplus/slvctrl-protocol)

This is a PlatformIO library that implements the [SlvCtrl protocol](https://github.com/SlvCtrlPlus/slvctrlplus-doc/tree/main/protocol).

## Commands

- `status` - list all attributes with current value (if readable)
- `get <name>` - read one attribute
- `set <name> <value>` - set one attribute (if writable)
- `introduce` - device identity line: `type:{devicetype},fw:{fwversion},protocol:{protocolversion}`
- `attributes` - list attributes: `attributes;name:rw[type[:meta]],...`

## Install (PlatformIO)

In your project's `platformio.ini`:

```ini
lib_deps =
  slvctrlplus/slvctrl-protocol@^0.1.3
```

## Usage

See `examples/` folder in this repository.

Currently the following attribute types are supported:

* IntAttribute
* FloatAttribute
* RangeAttribute<int32_t|float>
* ListAttribute<int32_t|const char*>
* StrAttribute
* BoolAttribute

## Portability note

The public header avoids including `Arduino.h` to keep it lightweight.
Implementation files include `Arduino.h` + `SerialCommands.h` (because they are required to actually run on Arduino-compatible cores).
