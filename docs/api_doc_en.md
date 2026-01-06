# Data Acquisition System Frontend API Documentation

English | [简体中文](api_doc.md)

## Overview

This document describes all frontend API interfaces for the high-performance data acquisition and processing system. The system provides REST API for control and management, and WebSocket interface for real-time data streaming.

### Service Addresses

- **HTTP API**: `http://127.0.0.1:8080`
- **WebSocket**: `ws://127.0.0.1:8081`

### Common Response Format

All API responses follow a unified format:

```json
{
  "success": boolean,
  "data": any | null,
  "error": string | null,
  "timestamp": number
}
```

## System Control API

### 1. Get System Status

**Endpoint**: `GET /api/control/status`

**Description**: Get complete system status including device connection, data processing, trigger mode, etc.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": {
    "data_collection_active": false,
    "device_connected": true,
    "connected_clients": 2,
    "packets_processed": 15420,
    "uptime_seconds": 3600,
    "memory_usage_mb": 45.2,
    "connection_type": "socket",
    "current_mode": "trigger",
    "trigger_support": true,
    "trigger_status": {
      "cached_bursts": 3,
      "current_burst_active": false,
      "last_trigger_timestamp": 1704067200,
      "total_triggers_received": 25
    }
  },
  "error": null,
  "timestamp": 1704067200000
}
```

**Field Descriptions**:
- `data_collection_active`: Whether data collection is active
- `device_connected`: Device connection status
- `connected_clients`: Number of WebSocket connected clients
- `packets_processed`: Total processed packets
- `uptime_seconds`: System uptime in seconds
- `memory_usage_mb`: Memory usage in MB
- `connection_type`: Connection type ("serial" or "socket")
- `current_mode`: Current working mode ("continuous" or "trigger")
- `trigger_status`: Detailed trigger mode status

### 2. Start Data Collection

**Endpoint**: `POST /api/control/start`

**Description**: Start data collection process.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Data collection started",
  "error": null,
  "timestamp": 1704067200000
}
```

### 3. Stop Data Collection

**Endpoint**: `POST /api/control/stop`

**Description**: Stop data collection process.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Data collection stopped",
  "error": null,
  "timestamp": 1704067200000
}
```

### 4. Device Ping Test

**Endpoint**: `POST /api/control/ping`

**Description**: Send ping command to test device connectivity.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Ping command sent to device",
  "error": null,
  "timestamp": 1704067200000
}
```

### 5. Get Device Information

**Endpoint**: `POST /api/control/device_info`

**Description**: Request detailed device information.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Device info request sent",
  "error": null,
  "timestamp": 1704067200000
}
```

## Mode Control API

### 6. Set Continuous Mode

**Endpoint**: `POST /api/control/continuous_mode`

**Description**: Switch device to continuous acquisition mode.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Continuous mode command sent",
  "error": null,
  "timestamp": 1704067200000
}
```

### 7. Set Trigger Mode

**Endpoint**: `POST /api/control/trigger_mode`

**Description**: Switch device to trigger acquisition mode.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Trigger mode command sent",
  "error": null,
  "timestamp": 1704067200000
}
```

### 8. Request Trigger Data

**Endpoint**: `POST /api/control/request_trigger_data`

**Description**: Manually request device to send trigger buffer data (only valid in trigger mode).

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": "Buffered data request sent",
  "error": null,
  "timestamp": 1704067200000
}
```

**Error Response**:
```json
{
  "success": false,
  "data": null,
  "error": "Device not in trigger mode",
  "timestamp": 1704067200000
}
```

### 9. Configure Data Stream

**Endpoint**: `POST /api/control/configure`

**Description**: Configure device sampling parameters including channels, sample rate, and data format.

**Request Body**:
```json
{
  "channels": [
    {
      "channel_id": 0,
      "sample_rate": 10000,
      "format": 1
    },
    {
      "channel_id": 1,
      "sample_rate": 10000,
      "format": 1
    }
  ]
}
```

**Parameter Descriptions**:
- `channel_id`: Channel ID (0-15)
- `sample_rate`: Sample rate in Hz
- `format`: Data format (1=int16, 2=int32, 4=float32)

**Response Example**:
```json
{
  "success": true,
  "data": "Stream configuration sent",
  "error": null,
  "timestamp": 1704067200000
}
```

## Trigger Data Management API

### 10. Get Trigger Burst List

**Endpoint**: `GET /api/trigger/list`

**Description**: Get summary information of all currently cached trigger bursts.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": [
    {
      "burst_id": "trigger_1704067200_1704067205000",
      "trigger_timestamp": 1704067200,
      "trigger_channel": 0,
      "total_samples": 1500,
      "duration_ms": 75.5,
      "created_at": 1704067205000,
      "quality": "Good",
      "can_save": true
    }
  ],
  "error": null,
  "timestamp": 1704067200000
}
```

**Field Descriptions**:
- `burst_id`: Unique burst identifier
- `trigger_timestamp`: Trigger timestamp
- `trigger_channel`: Trigger channel ID
- `total_samples`: Total sample count
- `duration_ms`: Data duration in milliseconds
- `created_at`: Burst creation time
- `quality`: Data quality ("Good"/"Warning"/"Error")
- `can_save`: Whether burst can be saved

### 11. Preview Trigger Burst

**Endpoint**: `GET /api/trigger/preview/{burst_id}`

**Description**: Get detailed information and complete data for a specific trigger burst.

**Path Parameters**:
- `burst_id`: Burst ID

**Response Example**:
```json
{
  "success": true,
  "data": {
    "burst_id": "trigger_1704067200_1704067205000",
    "trigger_timestamp": 1704067200,
    "trigger_channel": 0,
    "pre_samples": 1000,
    "post_samples": 1000,
    "data_packets": [
      {
        "timestamp": 1704067200100,
        "sequence": 12345,
        "channel_count": 2,
        "sample_rate": 10000.0,
        "data": [1.23, 1.24, 1.25],
        "metadata": {
          "packet_count": 1,
          "processing_time_us": 500,
          "data_quality": {
            "status": "Good"
          },
          "channel_info": [
            {
              "channel_id": 0,
              "sample_count": 100,
              "min_value": 1.0,
              "max_value": 3.0,
              "avg_value": 2.0
            }
          ]
        },
        "data_type": {
          "source": "Trigger",
          "trigger_info": {
            "trigger_timestamp": 1704067200,
            "is_complete": true,
            "sequence_in_burst": 1
          }
        }
      }
    ],
    "is_complete": true,
    "total_samples": 1500,
    "created_at": 1704067205000,
    "quality_summary": {
      "overall_quality": {
        "status": "Good"
      },
      "channel_stats": [
        {
          "channel_id": 0,
          "sample_count": 1500,
          "min_value": 0.5,
          "max_value": 2.8,
          "avg_value": 1.65,
          "rms_value": 1.8
        }
      ],
      "value_range": [0.5, 2.8],
      "anomaly_count": 0
    }
  },
  "error": null,
  "timestamp": 1704067200000
}
```

### 12. Save Trigger Burst

**Endpoint**: `POST /api/trigger/save/{burst_id}`

**Description**: Save specified trigger burst data to file system.

**Path Parameters**:
- `burst_id`: Burst ID

**Request Body**:
```json
{
  "dir": "experiments/vibration_test",
  "filename": "impact_measurement_001",
  "format": "csv",
  "description": "50Hz vibration impact test data"
}
```

**Parameter Descriptions**:
- `dir` (optional): Subdirectory path relative to data directory
- `filename` (optional): Custom filename (without extension)
- `format` (required): Export format ("json"/"csv"/"binary")
- `description` (optional): File description

**Response Example**:
```json
{
  "success": true,
  "data": {
    "saved_path": "experiments/vibration_test/impact_measurement_001.csv",
    "format": "csv",
    "size_bytes": 125600,
    "burst_info": {
      "burst_id": "trigger_1704067200_1704067205000",
      "trigger_timestamp": 1704067200,
      "trigger_channel": 0,
      "total_samples": 1500,
      "duration_ms": 75.5,
      "created_at": 1704067205000,
      "quality": "Good",
      "can_save": true
    }
  },
  "error": null,
  "timestamp": 1704067200000
}
```

**Error Response Example**:
```json
{
  "success": false,
  "data": null,
  "error": "Invalid format. Supported: [\"json\", \"csv\", \"binary\"]",
  "timestamp": 1704067200000
}
```

### 13. Delete Trigger Burst

**Endpoint**: `DELETE /api/trigger/delete/{burst_id}`

**Description**: Delete specified trigger burst from cache.

**Path Parameters**:
- `burst_id`: Burst ID

**Response Example**:
```json
{
  "success": true,
  "data": "Trigger burst deleted",
  "error": null,
  "timestamp": 1704067200000
}
```

## File Management API

### 14. List Files

**Endpoint**: `GET /api/files`

**Description**: List files in data directory.

**Query Parameters**:
- `dir` (optional): Subdirectory path

**Examples**:
- `GET /api/files` - List root directory files
- `GET /api/files?dir=experiments` - List experiments subdirectory files

**Response Example**:
```json
{
  "success": true,
  "data": [
    {
      "filename": "wave_20240101_120000.bin",
      "size_bytes": 204800,
      "created_at": 1704067200000,
      "file_type": "binary"
    },
    {
      "filename": "experiments/test_data.csv",
      "size_bytes": 51200,
      "created_at": 1704067100000,
      "file_type": "raw_frames"
    }
  ],
  "error": null,
  "timestamp": 1704067200000
}
```

**Field Descriptions**:
- `filename`: Relative path filename
- `size_bytes`: File size in bytes
- `created_at`: Creation timestamp
- `file_type`: File type ("binary"/"json"/"raw_frames"/"unknown")

### 15. Download File

**Endpoint**: `GET /api/files/{filename}`

**Description**: Download specified file. Supports subdirectory paths.

**Path Parameters**:
- `filename`: File path (supports subdirectories, e.g., "experiments/data.bin")

**Important Notes**:
- **URL Encoding**: When filename contains special characters (slashes, spaces, Chinese characters, etc.), URL encoding is required
- **Subdirectory Paths**: Supports subdirectory format, e.g., `subfolder/file.bin` needs to be encoded as `subfolder%2Ffile.bin`

**Request Examples**:
```bash
# Root directory file
GET /api/files/data.bin

# Subdirectory file (requires URL encoding)
GET /api/files/experiments%2Ftest_data.csv
# Original path: experiments/test_data.csv
```

**JavaScript Example**:
```javascript
// Correct encoding method
const filename = "test_output/data.csv";
const url = `/api/files/${encodeURIComponent(filename)}`;
fetch(url).then(response => response.blob());
```

**Python Example**:
```python
import urllib.parse

filename = "test_output/data.csv"
encoded_filename = urllib.parse.quote(filename, safe='')
url = f"/api/files/{encoded_filename}"
```

**Response**:
- **Success**: Returns binary file content with appropriate Content-Type and Content-Disposition headers
- **Failure**: Returns 404 status code

**Response Headers Example**:
```
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="data.bin"
```

### 16. Save Data File

**Endpoint**: `POST /api/files/save`

**Description**: Save base64-encoded data to file system.

**Request Body**:
```json
{
  "dir": "measurements/2024-01-01",
  "filename": "test_data.bin",
  "base64": "AAABAAACAAADAAAEAAAF..."
}
```

**Parameter Descriptions**:
- `dir` (optional): Relative subdirectory path
- `filename` (optional): Filename (auto-generated if not provided)
- `base64` (required): Base64-encoded file content

**Response Example**:
```json
{
  "success": true,
  "data": "measurements/2024-01-01/test_data.bin",
  "error": null,
  "timestamp": 1704067200000
}
```

## System Information API

### 17. Health Check

**Endpoint**: `GET /health`

**Description**: Check system health status.

**Request**: No parameters required

**Response Example**:
```json
{
  "success": true,
  "data": {
    "status": "healthy",
    "service": "data-gateway",
    "version": "2.0",
    "trigger_support": true,
    "timestamp": "2024-01-01T12:00:00Z"
  },
  "error": null,
  "timestamp": 1704067200000
}
```

### 18. API Information

**Endpoint**: `GET /`

**Description**: Get API basic information and available endpoints list.

**Response Example**:
```json
{
  "name": "Data Gateway API",
  "version": "2.0",
  "description": "High-performance data acquisition and processing system with enhanced trigger support",
  "features": {
    "continuous_mode": true,
    "trigger_mode": true,
    "websocket_streaming": true,
    "file_management": true,
    "real_time_processing": true,
    "trigger_data_management": true,
    "custom_file_saving": true
  },
  "endpoints": {
    "health": "/health",
    "status": "/api/control/status",
    "start": "/api/control/start",
    "stop": "/api/control/stop",
    "ping": "/api/control/ping",
    "device_info": "/api/control/device_info",
    "modes": {
      "continuous": "/api/control/continuous_mode",
      "trigger": "/api/control/trigger_mode"
    },
    "trigger": {
      "request_data": "/api/control/request_trigger_data",
      "list_bursts": "/api/trigger/list",
      "preview_burst": "/api/trigger/preview/{burst_id}",
      "save_burst": "/api/trigger/save/{burst_id}",
      "delete_burst": "/api/trigger/delete/{burst_id}"
    },
    "configuration": "/api/control/configure",
    "files": {
      "list": "/api/files?dir=<optional>",
      "download": "/api/files/{filename}",
      "save": "/api/files/save"
    },
    "websocket": "ws://<host>:<port>"
  },
  "documentation": "https://github.com/edgedaq/trigger-daq"
}
```

## WebSocket Real-time Interface

### Connection Address

`ws://127.0.0.1:8081`

### Connection Flow

1. Establish WebSocket connection
2. Receive welcome message
3. Send subscription message (optional)
4. Receive real-time data

### Subscription Control

**Send Subscription Message**:
```json
{
  "type": "subscribe",
  "channels": ["all"]
}
```

**Available Channels**:
- `"all"`: Subscribe to all events
- `"data"`: Subscribe to data stream
- `"trigger_events"`: Subscribe to trigger event notifications
- `"trigger_bursts"`: Subscribe to trigger burst completion notifications
- `"continuous_only"`: Subscribe to continuous mode data only
- `"trigger_only"`: Subscribe to trigger mode data only

**Subscription Confirmation Response**:
```json
{
  "type": "subscription_updated",
  "client_id": "uuid-client-id",
  "subscriptions": {
    "data_stream": true,
    "trigger_events": true,
    "trigger_bursts": true,
    "continuous_only": false,
    "trigger_only": false
  },
  "timestamp": 1704067200000
}
```

### WebSocket Message Types

#### Welcome Message
```json
{
  "type": "welcome",
  "client_id": "uuid-client-id",
  "timestamp": 1704067200000,
  "server_capabilities": {
    "data_streaming": true,
    "trigger_events": true,
    "trigger_burst_complete": true,
    "subscription_control": true
  }
}
```

#### Real-time Data Stream
```json
{
  "type": "data",
  "timestamp": 1704067200000,
  "sequence": 12345,
  "channel_count": 2,
  "sample_rate": 10000.0,
  "data": [1.23, 1.24, 1.25, 1.26, 1.27],
  "metadata": {
    "packet_count": 12345,
    "processing_time_us": 500,
    "data_quality": {
      "status": "Good"
    },
    "channel_info": [
      {
        "channel_id": 0,
        "sample_count": 100,
        "min_value": 1.0,
        "max_value": 3.0,
        "avg_value": 2.0
      }
    ]
  },
  "data_type": {
    "source": "Continuous",
    "trigger_info": null
  }
}
```

#### Trigger Event Notification
```json
{
  "type": "trigger_event",
  "timestamp": 1704067200,
  "channel": 0,
  "pre_samples": 1000,
  "post_samples": 1000,
  "event_time": 1704067205000
}
```

#### Trigger Burst Complete Notification
```json
{
  "type": "trigger_burst_complete",
  "burst_id": "trigger_1704067200_1704067205000",
  "trigger_timestamp": 1704067200,
  "trigger_channel": 0,
  "total_samples": 1500,
  "total_packets": 8,
  "duration_ms": 75.5,
  "quality": "Good",
  "can_save": true,
  "created_at": 1704067205000,
  "preview_samples": [1.23, 1.24, 1.25, 1.26, 1.27],
  "channel_stats": [
    {
      "channel_id": 0,
      "sample_count": 1500,
      "min_value": 0.5,
      "max_value": 2.8,
      "avg_value": 1.65,
      "rms_value": 1.8
    }
  ],
  "voltage_range": [0.1, 3.2],
  "event_time": 1704067210000
}
```

#### Ping/Pong Heartbeat

**Send Ping**:
```json
{
  "type": "ping"
}
```

**Receive Pong**:
```json
{
  "type": "pong",
  "timestamp": 1704067200000
}
```

## Error Handling

### HTTP Status Codes

- `200 OK`: Request successful
- `400 Bad Request`: Request parameter error
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server internal error

### Error Response Format

```json
{
  "success": false,
  "data": null,
  "error": "Specific error message",
  "timestamp": 1704067200000
}
```

### Common Errors

1. **Device not connected**: "Device not connected"
2. **Invalid format**: "Invalid format. Supported: [\"json\", \"csv\", \"binary\"]"
3. **Burst not found**: "Trigger burst not found"
4. **Mode error**: "Device not in trigger mode"
5. **Parameter error**: "Invalid parameter"

## Development Recommendations

### Frontend Development Best Practices

1. **Status Polling**: Regularly call `/api/control/status` to get system status
2. **WebSocket Reconnection**: Implement automatic reconnection for network interruptions
3. **Error Handling**: Unified handling of API error responses
4. **Data Visualization**: Use WebSocket data stream for real-time charting
5. **Burst Management**: Implement trigger burst list, preview, and save interface
6. **File Management**: Provide file upload and download functionality

### Performance Optimization

1. **Data Buffering**: Use buffering mechanism when WebSocket data volume is large
2. **Selective Subscription**: Subscribe to specific data types as needed
3. **File Pagination**: Support pagination display for file lists
4. **Memory Management**: Timely cleanup of unnecessary data and DOM elements

### Security Considerations

1. **Input Validation**: Validate all user inputs
2. **File Paths**: Prevent path traversal attacks
3. **Data Size**: Limit upload file size
4. **Connection Limits**: Control WebSocket connection count

## Example Code

### JavaScript WebSocket Client

```javascript
const ws = new WebSocket('ws://127.0.0.1:8081');

ws.onopen = function() {
    console.log('WebSocket connection established');

    // Subscribe to all events
    ws.send(JSON.stringify({
        type: 'subscribe',
        channels: ['all']
    }));
};

ws.onmessage = function(event) {
    const message = JSON.parse(event.data);

    switch(message.type) {
        case 'welcome':
            console.log('Received welcome message:', message.client_id);
            break;

        case 'data':
            // Handle real-time data
            updateChart(message.data);
            break;

        case 'trigger_event':
            console.log('Trigger event:', message);
            break;

        case 'trigger_burst_complete':
            console.log('Trigger burst complete:', message.burst_id);
            refreshTriggerList();
            break;
    }
};
```

### JavaScript API Call Examples

```javascript
// Get system status
async function getSystemStatus() {
    const response = await fetch('/api/control/status');
    const result = await response.json();
    return result;
}

// Start data collection
async function startCollection() {
    const response = await fetch('/api/control/start', {
        method: 'POST'
    });
    const result = await response.json();
    return result;
}

// Save trigger burst
async function saveTriggerBurst(burstId, options) {
    const response = await fetch(`/api/trigger/save/${burstId}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(options)
    });
    const result = await response.json();
    return result;
}
```

---

For more information, please refer to:
- [Protocol Documentation](protocol_doc_en.md)
- [Architecture Documentation](ARCHITECTURE_en.md)
- [FAQ](FAQ_en.md)

**Documentation Version**: 1.0
**Last Updated**: 2025-01-07
