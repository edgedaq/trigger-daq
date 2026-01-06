# Communication Protocol Documentation

## 1. Frame Format

The
 communication protocol employs a structured frame to guarantee 
reliability and facilitate easy parsing. The frame structure is detailed
 as follows:

| Field         | Size               | Description                                                                                                                         |
| ------------- | ------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| **FrameHead** | 2 Bytes            | Frame header, with a fixed value (e.g., `0xAA 0x55`) for straightforward identification.                                            |
| **Length**    | 2 Bytes            | Total frame length, spanning from **CommandID** to **CheckSum**, excluding FrameHead and FrameTail. Stored in little-endian format. |
| **CommandID** | 1 Byte             | Command identifier, utilized to distinguish various commands or data types.                                                         |
| **Seq**       | 1 Byte             | Sequence number for pairing requests and responses or for frame counting.                                                           |
| **Payload**   | Variable (N Bytes) | Data payload specific to the command.                                                                                               |
| **CheckSum**  | 2 Bytes            | CRC16 checksum computed from **CommandID** to the end of the **Payload**.                                                           |
| **FrameTail** | 2 Bytes            | Frame tail, possessing a fixed value (e.g., `0x55 0xAA`) for frame boundary validation.                                             |

## 2. Frame Layout

| Field     | Size               | Value                                                                   |
| --------- | ------------------ | ----------------------------------------------------------------------- |
| FrameHead | 2 Bytes            | `0xAA 0x55`                                                             |
| Length    | 2 Bytes            | Total frame length (from CommandID to CheckSum) in little-endian format |
| CommandID | 1 Byte             | Command identifier                                                      |
| Seq       | 1 Byte             | Sequence number                                                         |
| Payload   | Variable (N Bytes) | Command-specific data                                                   |
| CheckSum  | 2 Bytes            | CRC16 value calculated from CommandID to the end of Payload             |
| FrameTail | 2 Bytes            | `0x55 0xAA`                                                             |

**Frame Structure Visualization**:  
`FrameHead (2B) | Length (2B) | CommandID (1B) | Seq (1B) | Payload (N B) | CheckSum (2B) | FrameTail (2B)`  
Example: `0xAA 0x55 0xXX 0xXX 0x?? 0x?? ... 0x?? 0x?? 0x55 0xAA`

## 3. Command Definitions

CommandID is used to distinguish different operation types. The following allocation rules are adopted:

- **PC -> Device**: 0x00 - 0x7F (requests or control commands)
- **Device -> PC**: 0x80 - 0xFF (responses or active reports)
- **Response Command ID**: Typically, the highest bit of the corresponding request command ID is set to 1 (i.e., Request_ID | 0x80).

### (1) Status and Parameter Commands

| CommandID | Direction    | Meaning                    | Remarks                                                                      |
| --------- | ------------ | -------------------------- | ---------------------------------------------------------------------------- |
| 0x10      | PC -> Device | Request Device Status      | Payload can be empty. Used to query the current state of the device.         |
| 0x90      | Device -> PC | Device Status Response     | Payload contains device status information (e.g., running, idle, error).     |
| 0x11      | PC -> Device | Set Device Parameters      | Payload contains the parameter identifiers and their values.                 |
| 0x91      | Device -> PC | Parameter Set Response     | Payload indicates success or failure. On failure, can include an error code. |
| 0x12      | PC -> Device | Request Device Parameters  | Payload specifies which parameters to read. Can be empty to request all.     |
| 0x92      | Device -> PC | Device Parameters Response | Payload contains the requested parameter values.                             |

### (2) Control Commands

| CommandID | Direction    | Meaning                 | Remarks                                                               |
| --------- | ------------ | ----------------------- | --------------------------------------------------------------------- |
| 0x20      | PC -> Device | Start Data Transmission | Commands the device to begin sending data streams (e.g., ADC data).   |
| 0xA0      | Device -> PC | Start Command Response  | Acknowledges the start command, indicating success or failure.        |
| 0x21      | PC -> Device | Stop Data Transmission  | Commands the device to halt data streams.                             |
| 0xA1      | Device -> PC | Stop Command Response   | Acknowledges the stop command.                                        |
| 0x22      | PC -> Device | Device Reset            | Requests a software reset of the device.                              |
| 0xA2      | Device -> PC | Reset Command Response  | Acknowledges the reset command. Usually sent before the reset occurs. |
| 0x2F      | PC -> Device | Ping                    | Used to check if the communication link is active.                    |
| 0xAF      | Device -> PC | Pong (Ping Response)    | The response to a Ping command, confirming the link is active.        |

### (3) Data Transmission Commands

| CommandID | Direction    | Meaning           | Remarks                                                                                                                                                                            |
| --------- | ------------ | ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0x40      | Device -> PC | ADC Data Packet   | Device<br> proactively sends ADC data. The payload will contain data from one or <br>more channels. This command is for streaming and does not require a <br>response from the PC. |
| 0x41      | Device -> PC | Other Sensor Data | Can be used for other types of data packets, like IMU, temperature, etc.                                                                                                           |

### (4) Logging and Debugging Commands

| CommandID | Direction    | Meaning     | Remarks                                                                                                                                                     |
| --------- | ------------ | ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0xE0      | Device -> PC | Log Message | Device<br> sends a log string for debugging or status updates. The payload can <br>contain the log level (e.g., INFO, WARN, ERROR) and the message content. |

## 4. Summary

1. **Protocol Frame Format**:
   - By means of fixed frame head/tail, length field, and checksum, a 
     frame of data can be reliably identified within a continuous byte 
     stream.
2. **CommandID of Instructions/Commands**:
   - Used to distinguish different requests, responses, or data types.
   - The Seq field is employed for matching requests-responses or for frame counting, preventing out-of-order or lost packets.
3. **CRC16**:
   - Although USB CDC or serial ports can ensure the reliability of the 
     physical layer, adding CRC16 at the application layer further guarantees
     the integrity of the frame structure and can detect logical errors 
     (such as buffer overflows, alignment errors, etc.).
   - Here is the function for calculating CRC16 in C language:

c

运行

```c
#include <stdio.h>
#include <stdint.h>

// Function to calculate CRC16
uint16_t CRC16_Calc(const uint8_t* data, uint16_t length, uint16_t initVal)
{
    uint16_t crc = initVal;  // The initial value can usually be set to 0xFFFF, 0x0000, etc.

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;  // Common polynomial
            }
            else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

4. **C Language Example**:
   - The functions buildFrame() and parseFrame() are provided, which can 
     be directly tested on a PC and also transplanted to a dual-end of MCU + 
     PC.
5. **Expansion**:
   - In actual projects, more command words can be defined.
   - The Payload part can add structures or binary data as needed.
   - The CRC algorithm can be replaced with the version existing in the project.
   - If larger data volumes need to be supported, segmented transmission 
     or multi-frame splicing can be implemented at the application layer.

# Implementation of I/O Buffer with Multi-Frame Support

To
 enable continuous sending and receiving of multiple frames, a circular 
queue or FIFO is required to store the original input data and the 
frames to be transmitted. The following presents an example 
implementation, including:

- **Rx Buffer**: A cache for the original input byte 
  stream, supporting the placement of new bytes via the function 
  feedRxBuffer() and the parsing of one or more frames within it using 
  tryParseFramesFromRx().
- **Tx Buffer**: Queues the constructed frames in sequence and retrieves and transmits them in a sending thread or interrupt.

### Summary:

1. The capacity of the circular queue can be increased according to 
   requirements. The RX_BUFFER_SIZE and TX_BUFFER_SIZE need to be set based
   on the actual communication rate and business data volume.
2. If the receiver cannot keep up with the sender, it may lead to a 
   circular queue overflow. Appropriate actions such as packet loss, 
   blocking wait, or other treatments can be performed when detecting 
   insufficient space in feedRxBuffer() / enqueueTxFrame().
3. In the example, when placing multiple frames into the TxBuffer, the 
   method of frameLen + frameData is used for separation; alternatively, 
   the "frame head-frame tail" method can be adopted to store the entire 
   frame in the TxBuffer. The key is to distinguish the boundary of each 
   frame during dequeue.
4. For actual projects, serial port sending and receiving usually occur
   in interrupts or DMA callbacks. Once new data is available, it is fed 
   into the RxBuffer(). The function tryParseFramesFromRx() is called 
   regularly in the main loop or task, and multiple frames may be parsed 
   each time.
5. Similarly, the sending end can also be in an independent task or 
   timer, and each frame is sent to the hardware peripheral based on 
   dequeueTxFrame().
6. Through this approach, multiple frames of data can be stably sent 
   and received, and the single-frame logic can be handled at the 
   application layer using buildFrame() / parseFrame(), with a clear 
   hierarchy and easy extensibility.
