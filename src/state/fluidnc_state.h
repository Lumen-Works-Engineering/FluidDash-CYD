#ifndef FLUIDNC_STATE_H
#define FLUIDNC_STATE_H

#include <Arduino.h>

/**
 * @brief FluidNC CNC controller state management class
 *
 * Encapsulates all FluidNC-related runtime state including:
 * - Machine state (IDLE, RUN, ALARM, etc.)
 * - Position data (machine and work coordinates)
 * - Motion parameters (feed rate, spindle speed)
 * - Override values
 * - Job status
 */
class FluidNCState
{
public:
    // ========== Machine State ==========
    String machineState;   // Machine state: "IDLE", "RUN", "ALARM", "OFFLINE", etc.
    bool fluidncConnected; // FluidNC connection status

    // ========== Position Data (Machine Coordinates) ==========
    float posX; // X-axis machine position (MPos) in mm
    float posY; // Y-axis machine position (MPos) in mm
    float posZ; // Z-axis machine position (MPos) in mm
    float posA; // A-axis machine position (MPos) in mm/deg

    // ========== Position Data (Work Coordinates) ==========
    float wposX; // X-axis work position (WPos) in mm
    float wposY; // Y-axis work position (WPos) in mm
    float wposZ; // Z-axis work position (WPos) in mm
    float wposA; // A-axis work position (WPos) in mm/deg

    // ========== Work Coordinate Offsets ==========
    float wcoX; // X-axis work offset (WCO) in mm
    float wcoY; // Y-axis work offset (WCO) in mm
    float wcoZ; // Z-axis work offset (WCO) in mm
    float wcoA; // A-axis work offset (WCO) in mm/deg

    // ========== Motion Parameters ==========
    int feedRate;   // Current feed rate (mm/min)
    int spindleRPM; // Spindle speed (RPM)

    // ========== Override Values ==========
    int feedOverride;    // Feed override percentage (default: 100%)
    int rapidOverride;   // Rapid override percentage (default: 100%)
    int spindleOverride; // Spindle override percentage (default: 100%)

    // ========== Job Status ==========
    unsigned long jobStartTime; // Job start timestamp (millis)
    bool isJobRunning;          // Job running flag

    // ========== WebSocket & Reporting ==========
    bool autoReportingEnabled;        // Auto-reporting state flag
    unsigned long reportingSetupTime; // Reporting setup timestamp
    unsigned long lastStatusRequest;  // Last status request timestamp
    bool debugWebSocket;              // WebSocket debug flag

    // ========== Constructor ==========
    FluidNCState();

    // ========== Methods ==========

    /**
     * @brief Initialize FluidNC state with default values
     */
    void init();

    /**
     * @brief Set machine state
     * @param state New machine state string
     */
    void setMachineState(const String &state);

    /**
     * @brief Check if machine is in a running state
     * @return true if machine is running (RUN or JOG)
     */
    bool isRunning() const;

    /**
     * @brief Check if machine is in alarm state
     * @return true if machine is in ALARM state
     */
    bool isAlarmed() const;

    /**
     * @brief Check if machine is idle
     * @return true if machine is IDLE
     */
    bool isIdle() const;

    /**
     * @brief Set connection status
     * @param connected Connection status
     */
    void setConnected(bool connected);

    /**
     * @brief Update machine position (MPos)
     * @param x X position
     * @param y Y position
     * @param z Z position
     * @param a A position (optional)
     */
    void updateMachinePosition(float x, float y, float z, float a = 0.0f);

    /**
     * @brief Update work position (WPos)
     * @param x X position
     * @param y Y position
     * @param z Z position
     * @param a A position (optional)
     */
    void updateWorkPosition(float x, float y, float z, float a = 0.0f);

    /**
     * @brief Update work coordinate offsets (WCO)
     * @param x X offset
     * @param y Y offset
     * @param z Z offset
     * @param a A offset (optional)
     */
    void updateWorkOffsets(float x, float y, float z, float a = 0.0f);

    /**
     * @brief Update motion parameters
     * @param feed Feed rate (mm/min)
     * @param spindle Spindle speed (RPM)
     */
    void updateMotion(int feed, int spindle);

    /**
     * @brief Update override values
     * @param feed Feed override (%)
     * @param rapid Rapid override (%)
     * @param spindle Spindle override (%)
     */
    void updateOverrides(int feed, int rapid, int spindle);

    /**
     * @brief Start job tracking
     */
    void startJob();

    /**
     * @brief Stop job tracking
     */
    void stopJob();

    /**
     * @brief Get job runtime in seconds
     * @return Job runtime in seconds (0 if not running)
     */
    unsigned long getJobRuntime() const;
};

// Global FluidNC state instance
extern FluidNCState fluidncState;

#endif // FLUIDNC_STATE_H
