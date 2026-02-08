# SlvCtrlProtocol

This is a Platform.IO library that implements the [SlvCtrl protocol](https://github.com/SlvCtrlPlus/slvctrlplus-doc/tree/main/protocol).

## Commands

- `status` - list all attributes with current value (if readable)
- `get <name>` - read one attribute
- `set <name> <value>` - set one attribute (if writable)
- `introduce` - device identity line: `{devicetype},{fwversion},{protocolversion}`
- `attributes` - list attributes: `attributes;name:rw[type[:meta]],...`

## Install (PlatformIO)

In your project `platformio.ini`:

```ini
lib_deps =
  https://github.com/SlvCtrlPlus/slvctrl-protocol.git#v0.1.0
```

## Usage

See `examples/` folder in this repository.

Currently the following attribute types are supported:

* IntAttribute
* FloatAttribute
* RangeAttribute<int|float>
* StrAttribute
* BoolAttribute

## Portability note

The public header avoids including `Arduino.h` to keep it lightweight.
Implementation files include `Arduino.h` + `SerialCommands.h` (because they are required to actually run on Arduino-compatible cores).
