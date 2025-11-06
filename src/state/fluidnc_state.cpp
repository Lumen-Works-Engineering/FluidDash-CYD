#include "fluidnc_state.h"

// Global instance
FluidNCState fluidncState;

FluidNCState::FluidNCState()
{
    init();
}

void FluidNCState::init()
{
    // Initialize machine state
    machineState = "OFFLINE";
    fluidncConnected = false;

    // Initialize positions
    posX = posY = posZ = posA = 0.0f;
    wposX = wposY = wposZ = wposA = 0.0f;
    wcoX = wcoY = wcoZ = wcoA = 0.0f;

    // Initialize motion parameters
    feedRate = 0;
    spindleRPM = 0;

    // Initialize overrides
    feedOverride = 100;
    rapidOverride = 100;
    spindleOverride = 100;

    // Initialize job status
    jobStartTime = 0;
    isJobRunning = false;

    // Initialize WebSocket & reporting
    autoReportingEnabled = false;
    reportingSetupTime = 0;
    lastStatusRequest = 0;
    debugWebSocket = false;
}

void FluidNCState::setMachineState(const String &state)
{
    machineState = state;
}

bool FluidNCState::isRunning() const
{
    return (machineState == "RUN" || machineState == "JOG");
}

bool FluidNCState::isAlarmed() const
{
    return (machineState == "ALARM");
}

bool FluidNCState::isIdle() const
{
    return (machineState == "IDLE");
}

void FluidNCState::setConnected(bool connected)
{
    fluidncConnected = connected;
    if (!connected)
    {
        machineState = "OFFLINE";
    }
}

void FluidNCState::updateMachinePosition(float x, float y, float z, float a)
{
    posX = x;
    posY = y;
    posZ = z;
    posA = a;
}

void FluidNCState::updateWorkPosition(float x, float y, float z, float a)
{
    wposX = x;
    wposY = y;
    wposZ = z;
    wposA = a;
}

void FluidNCState::updateWorkOffsets(float x, float y, float z, float a)
{
    wcoX = x;
    wcoY = y;
    wcoZ = z;
    wcoA = a;
}

void FluidNCState::updateMotion(int feed, int spindle)
{
    feedRate = feed;
    spindleRPM = spindle;
}

void FluidNCState::updateOverrides(int feed, int rapid, int spindle)
{
    feedOverride = feed;
    rapidOverride = rapid;
    spindleOverride = spindle;
}

void FluidNCState::startJob()
{
    isJobRunning = true;
    jobStartTime = millis();
}

void FluidNCState::stopJob()
{
    isJobRunning = false;
    jobStartTime = 0;
}

unsigned long FluidNCState::getJobRuntime() const
{
    if (!isJobRunning || jobStartTime == 0)
    {
        return 0;
    }
    return (millis() - jobStartTime) / 1000; // Return seconds
}
