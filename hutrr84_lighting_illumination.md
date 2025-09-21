# HUTRR84: Lighting And Illumination Spec Release: 1.12

**Submitted:** May 15, 2018  
**Requester:** Nathan Sherman (Microsoft)  
**Approved**

## Overview

This document establishes a new Usage Page for lighting control within the HID protocol. The current HID LED page (0x08) is insufficient for modern lighting control needs, particularly in gaming peripherals and PC components that require individual addressable lighting control.

This proposal adds content to Table 1: Usage Page Summary.

## Device Properties

Device properties are reported through a feature report that describes overall device characteristics.

- **LampCount** - The total number of individually addressable lamps that are part of the lamp array.
- **LampArrayKind** - Helps the host understand what lamp attributes can be expected.
- **UpdateLatencyInMicroseconds** - Time required for a lamp to update and stabilize.
- **SupportedLampPurposes** - Indicates which lamp purposes are supported.

## Lamp Attributes

Each lamp in the array has individual properties that describe its characteristics:

- **PositionXInMicrometers**
- **PositionYInMicrometers** 
- **PositionZInMicrometers**
- **LampPurposes**
- **UpdateLatencyInMicroseconds**
- **RedLevelCount**
- **GreenLevelCount**
- **BlueLevelCount**
- **IntensityLevelCount**
- **IsProgrammable**
- **InputBinding**

## Control Reports

### LampAttributesRequestReport
Requests information about a specific lamp identified by LampId.

### LampAttributesResponseReport
Returns the attributes of a specific lamp.

### LampMultiUpdateReport
Updates multiple lamps with individual RGBI values.

Fields:
- LampCount
- LampUpdateFlags
- LampId[N]
- RedUpdateChannel[N]
- GreenUpdateChannel[N]
- BlueUpdateChannel[N]
- IntensityUpdateChannel[N]

### LampRangeUpdateReport
Updates a range of lamps with the same RGBI values.

Fields:
- LampUpdateFlags
- LampIdStart
- LampIdEnd
- RedUpdateChannel
- GreenUpdateChannel
- BlueUpdateChannel
- IntensityUpdateChannel

### LampArrayControlReport
Controls overall array behavior.

Fields:
- AutonomousMode

## HID Report Descriptor Example

```
0x05, 0x59,        // USAGE_PAGE (Lighting And Illumination)
0x09, 0x01,        // USAGE (LampArray)
0xa1, 0x01,        // COLLECTION (Application)
0x85, 0x01,        //   REPORT_ID (1)
0x09, 0x02,        //   USAGE (LampArrayAttributesReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x03,        //     USAGE (LampCount)
0x09, 0x04,        //     USAGE (LampArrayKind)
0x09, 0x05,        //     USAGE (UpdateLatencyInMicroseconds)
0x09, 0x06,        //     USAGE (SupportedLampPurposes)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x27, 0xff, 0xff, 0x00, 0x00,  // LOGICAL_MAXIMUM (65535)
0x75, 0x10,        //     REPORT_SIZE (16)
0x95, 0x04,        //     REPORT_COUNT (4)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0x85, 0x02,        //   REPORT_ID (2)
0x09, 0x20,        //   USAGE (LampAttributesRequestReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x21,        //     USAGE (LampId)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x27, 0xff, 0xff, 0x00, 0x00,  // LOGICAL_MAXIMUM (65535)
0x75, 0x10,        //     REPORT_SIZE (16)
0x95, 0x01,        //     REPORT_COUNT (1)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0x85, 0x03,        //   REPORT_ID (3)
0x09, 0x30,        //   USAGE (LampAttributesResponseReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x21,        //     USAGE (LampId)
0x09, 0x31,        //     USAGE (PositionXInMicrometers)
0x09, 0x32,        //     USAGE (PositionYInMicrometers)
0x09, 0x33,        //     USAGE (PositionZInMicrometers)
0x09, 0x34,        //     USAGE (LampPurposes)
0x09, 0x05,        //     USAGE (UpdateLatencyInMicroseconds)
0x09, 0x35,        //     USAGE (RedLevelCount)
0x09, 0x36,        //     USAGE (GreenLevelCount)
0x09, 0x37,        //     USAGE (BlueLevelCount)
0x09, 0x38,        //     USAGE (IntensityLevelCount)
0x09, 0x39,        //     USAGE (IsProgrammable)
0x09, 0x3a,        //     USAGE (InputBinding)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x27, 0xff, 0xff, 0x00, 0x00,  // LOGICAL_MAXIMUM (65535)
0x75, 0x10,        //     REPORT_SIZE (16)
0x95, 0x0a,        //     REPORT_COUNT (10)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0x85, 0x04,        //   REPORT_ID (4)
0x09, 0x50,        //   USAGE (LampMultiUpdateReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x55,        //     USAGE (LampUpdateFlags)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x25, 0x08,        //     LOGICAL_MAXIMUM (8)
0x75, 0x08,        //     REPORT_SIZE (8)
0x95, 0x01,        //     REPORT_COUNT (1)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0x09, 0x21,        //     USAGE (LampId)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x27, 0xff, 0xff, 0x00, 0x00,  // LOGICAL_MAXIMUM (65535)
0x75, 0x10,        //     REPORT_SIZE (16)
0x95, 0x10,        //     REPORT_COUNT (16)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0x09, 0x51,        //     USAGE (RedUpdateChannel)
0x09, 0x52,        //     USAGE (GreenUpdateChannel)
0x09, 0x53,        //     USAGE (BlueUpdateChannel)
0x09, 0x54,        //     USAGE (IntensityUpdateChannel)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x26, 0xff, 0x00,  //     LOGICAL_MAXIMUM (255)
0x75, 0x08,        //     REPORT_SIZE (8)
0x95, 0x20,        //     REPORT_COUNT (32)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0x85, 0x05,        //   REPORT_ID (5)
0x09, 0x60,        //   USAGE (LampRangeUpdateReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x55,        //     USAGE (LampUpdateFlags)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x25, 0x08,        //     LOGICAL_MAXIMUM (8)
0x75, 0x08,        //     REPORT_SIZE (8)
0x95, 0x01,        //     REPORT_COUNT (1)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0x09, 0x61,        //     USAGE (LampIdStart)
0x09, 0x62,        //     USAGE (LampIdEnd)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x27, 0xff, 0xff, 0x00, 0x00,  // LOGICAL_MAXIMUM (65535)
0x75, 0x10,        //     REPORT_SIZE (16)
0x95, 0x02,        //     REPORT_COUNT (2)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0x09, 0x51,        //     USAGE (RedUpdateChannel)
0x09, 0x52,        //     USAGE (GreenUpdateChannel)
0x09, 0x53,        //     USAGE (BlueUpdateChannel)
0x09, 0x54,        //     USAGE (IntensityUpdateChannel)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x26, 0xff, 0x00,  //     LOGICAL_MAXIMUM (255)
0x75, 0x08,        //     REPORT_SIZE (8)
0x95, 0x04,        //     REPORT_COUNT (4)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0x85, 0x06,        //   REPORT_ID (6)
0x09, 0x70,        //   USAGE (LampArrayControlReport)
0xa1, 0x02,        //   COLLECTION (Logical)
0x09, 0x71,        //     USAGE (AutonomousMode)
0x15, 0x00,        //     LOGICAL_MINIMUM (0)
0x25, 0x01,        //     LOGICAL_MAXIMUM (1)
0x75, 0x08,        //     REPORT_SIZE (8)
0x95, 0x01,        //     REPORT_COUNT (1)
0xb1, 0x02,        //     FEATURE (Data,Var,Abs)
0xc0,              //   END_COLLECTION
0xc0               // END_COLLECTION
```