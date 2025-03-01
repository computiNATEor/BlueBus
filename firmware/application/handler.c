/*
 * File: handler.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the logic to have the BC127 and IBus communicate
 */
#include "handler.h"
static HandlerContext_t Context;
static char *PROFILES[4] = {
    "A2DP",
    "AVRCP",
    "",
    "HFP"
};

/**
 * HandlerInit()
 *     Description:
 *         Initialize our context and register all event listeners and
 *         scheduled tasks.
 *     Params:
 *         BC127_t *bt - The BC127 Object
 *         IBus_t *ibus - The IBus Object
 *     Returns:
 *         void
 */
void HandlerInit(BC127_t *bt, IBus_t *ibus)
{
    Context.bt = bt;
    Context.ibus = ibus;
    Context.cdChangerLastPoll = TimerGetMillis();
    Context.cdChangerLastStatus = TimerGetMillis();
    Context.btDeviceConnRetries = 0;
    Context.btStartupIsRun = 0;
    Context.btConnectionStatus = HANDLER_BT_CONN_OFF;
    Context.btSelectedDevice = HANDLER_BT_SELECTED_DEVICE_NONE;
    Context.uiMode = ConfigGetUIMode();
    Context.seekMode = HANDLER_CDC_SEEK_MODE_NONE;
    Context.blinkerCount = 0;
    Context.blinkerStatus = HANDLER_BLINKER_OFF;
    Context.mflButtonStatus = HANDLER_MFL_STATUS_OFF;
    memset(&Context.ibusModuleStatus, 0, sizeof(HandlerModuleStatus_t));
    memset(&Context.bodyModuleStatus, 0, sizeof(HandlerBodyModuleStatus_t));
    Context.powerStatus = HANDLER_POWER_ON;
    Context.scanIntervals = 0;
    EventRegisterCallback(
        BC127Event_Boot,
        &HandlerBC127Boot,
        &Context
    );
    EventRegisterCallback(
        BC127Event_BootStatus,
        &HandlerBC127BootStatus,
        &Context
    );
    EventRegisterCallback(
        BC127Event_CallStatus,
        &HandlerBC127CallStatus,
        &Context
    );
    EventRegisterCallback(
        BC127Event_DeviceLinkConnected,
        &HandlerBC127DeviceLinkConnected,
        &Context
    );
    EventRegisterCallback(
        BC127Event_DeviceDisconnected,
        &HandlerBC127DeviceDisconnected,
        &Context
    );
    EventRegisterCallback(
        BC127Event_DeviceFound,
        &HandlerBC127DeviceFound,
        &Context
    );
    EventRegisterCallback(
        BC127Event_PlaybackStatusChange,
        &HandlerBC127PlaybackStatus,
        &Context
    );
    EventRegisterCallback(
        UIEvent_CloseConnection,
        &HandlerUICloseConnection,
        &Context
    );
    EventRegisterCallback(
        UIEvent_InitiateConnection,
        &HandlerUIInitiateConnection,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_CDStatusRequest,
        &HandlerIBusCDCStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_FirstMessageReceived,
        &HandlerIBusFirstMessageReceived,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_DoorsFlapsStatusResponse,
        &HandlerIBusGMDoorsFlapsStatusResponse,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_GTDIAIdentityResponse,
        &HandlerIBusGTDIAIdentityResponse,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_GTDIAOSIdentityResponse,
        &HandlerIBusGTDIAOSIdentityResponse,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_IKEIgnitionStatus,
        &HandlerIBusIKEIgnitionStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_IKESpeedRPMUpdate,
        &HandlerIBusIKESpeedRPMUpdate,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_IKEVehicleType,
        &HandlerIBusIKEVehicleType,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_LCMLightStatus,
        &HandlerIBusLCMLightStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_LCMDimmerStatus,
        &HandlerIBusLCMDimmerStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_LCMRedundantData,
        &HandlerIBusLCMRedundantData,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_MFLButton,
        &HandlerIBusMFLButton,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_MFLVolume,
        &HandlerIBusMFLVolume,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_ModuleStatusRequest,
        &HandlerIBusModuleStatusRequest,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_ModuleStatusResponse,
        &HandlerIBusModuleStatusResponse,
        &Context
    );
    TimerRegisterScheduledTask(
        &HandlerTimerCDCAnnounce,
        &Context,
        HANDLER_INT_CDC_ANOUNCE
    );
    TimerRegisterScheduledTask(
        &HandlerTimerCDCSendStatus,
        &Context,
        HANDLER_INT_CDC_STATUS
    );
    TimerRegisterScheduledTask(
        &HandlerTimerDeviceConnection,
        &Context,
        HANDLER_INT_DEVICE_CONN
    );
    TimerRegisterScheduledTask(
        &HandlerTimerLCMIOStatus,
        &Context,
        HANDLER_INT_LCM_IO_STATUS
    );
    TimerRegisterScheduledTask(
        &HandlerTimerOpenProfileErrors,
        &Context,
        HANDLER_INT_PROFILE_ERROR
    );
    TimerRegisterScheduledTask(
        &HandlerTimerPoweroff,
        &Context,
        HANDLER_INT_POWEROFF
    );
    TimerRegisterScheduledTask(
        &HandlerTimerScanDevices,
        &Context,
        HANDLER_INT_DEVICE_SCAN
    );

    BC127CommandStatus(Context.bt);
    if (Context.uiMode == IBus_UI_CD53 ||
        Context.uiMode == IBus_UI_BUSINESS_NAV
    ) {
        CD53Init(bt, ibus);
    } else if (Context.uiMode == IBus_UI_BMBT) {
        BMBTInit(bt, ibus);
    } else if (Context.uiMode == IBus_UI_MID) {
        MIDInit(bt, ibus);
    } else if (Context.uiMode == IBus_UI_MID_BMBT) {
        MIDInit(bt, ibus);
        BMBTInit(bt, ibus);
    }
    BC127CommandSetMicGain(Context.bt, ConfigGetSetting(CONFIG_SETTING_MIC_GAIN));
}

static void HandlerSwitchUI(HandlerContext_t *context, unsigned char newUi)
{
    // Unregister the previous UI
    if (context->uiMode == IBus_UI_CD53 ||
        context->uiMode == IBus_UI_BUSINESS_NAV
    ) {
        CD53Destroy();
    } else if (context->uiMode == IBus_UI_BMBT) {
        BMBTDestroy();
    } else if (context->uiMode == IBus_UI_MID) {
        MIDDestroy();
    } else if (context->uiMode == IBus_UI_MID_BMBT) {
        MIDDestroy();
        BMBTDestroy();
    }
    if (newUi == IBus_UI_CD53 || newUi == IBus_UI_BUSINESS_NAV) {
        CD53Init(context->bt, context->ibus);
    } else if (newUi == IBus_UI_BMBT) {
        BMBTInit(context->bt, context->ibus);
    } else if (newUi == IBus_UI_MID) {
        MIDInit(context->bt, context->ibus);
    } else if (newUi == IBus_UI_MID_BMBT) {
        MIDInit(context->bt, context->ibus);
        BMBTInit(context->bt, context->ibus);
    }
    ConfigSetUIMode(newUi);
    context->uiMode = newUi;
}

/**
 * HandlerBC127Boot()
 *     Description:
 *         If the BC127 restarts, reset our internal state
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127Boot(void *ctx, unsigned char *tmp)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    BC127ClearPairedDevices(context->bt);
    BC127CommandStatus(context->bt);
}

/**
 * HandlerBC127BootStatus()
 *     Description:
 *         If the BC127 Radios are off, meaning we rebooted and got the status
 *         back, then alter the module status to match the ignition status
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127BootStatus(void *ctx, unsigned char *tmp)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    context->btConnectionStatus = HANDLER_BT_CONN_OFF;
    BC127CommandList(context->bt);
    if (context->ibus->ignitionStatus == IBUS_IGNITION_OFF) {
        // Set the BT module not connectable or discoverable and disconnect all devices
        BC127CommandBtState(context->bt, BC127_STATE_OFF, BC127_STATE_OFF);
        BC127CommandClose(context->bt, BC127_CLOSE_ALL);
    } else {
        // Set the connectable and discoverable states to what they were
        BC127CommandBtState(context->bt, BC127_STATE_ON, context->bt->discoverable);
    }
}

void HandlerBC127CallStatus(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    // If we were playing before the call, try to resume playback
    if (context->bt->callStatus == BC127_CALL_INACTIVE &&
        context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING
    ) {
        BC127CommandPlay(context->bt);
    }
    if (ConfigGetSetting(CONFIG_SETTING_TCU_MODE) == CONFIG_SETTING_OFF ||
        context->ibus->cdChangerFunction == IBUS_CDC_FUNC_NOT_PLAYING
    ) {
        if ((context->bt->callStatus == BC127_CALL_INCOMING ||
            context->bt->callStatus == BC127_CALL_OUTGOING) &&
            context->bt->scoStatus == BC127_CALL_SCO_OPEN
        ) {
            // Enable the amp and mute the radio
            PAM_SHDN = 1;
            TEL_MUTE = 1;
        }
        // Close the call immediately, without waiting for SCO to close
        if (context->bt->callStatus == BC127_CALL_INACTIVE) {
            // Disable the amp and unmute the radio
            PAM_SHDN = 0;
            TimerDelayMicroseconds(250);
            TEL_MUTE = 0;
        }
        // Tell the vehicle what the call status is
        HandlerIBusBroadcastTELStatus(context);
    }
}

/**
 * HandlerBC127DeviceLinkConnected()
 *     Description:
 *         If a device link is opened, disable connectability once all profiles
 *         are opened. Otherwise if the ignition is off, disconnect all devices
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *data - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127DeviceLinkConnected(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->ibus->ignitionStatus > IBUS_IGNITION_OFF) {
        // Once A2DP and AVRCP are connected, we can disable connectability
        // If HFP is enabled, do not disable connectability until the
        // profile opens
        if (context->bt->activeDevice.avrcpLinkId != 0 &&
            context->bt->activeDevice.a2dpLinkId != 0
        ) {
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_OFF ||
                context->bt->activeDevice.hfpLinkId != 0
            ) {
                LogDebug(LOG_SOURCE_SYSTEM, "Handler: Disable connectability");
                BC127CommandBtState(
                    context->bt,
                    BC127_STATE_OFF,
                    context->bt->discoverable
                );
                if (ConfigGetSetting(CONFIG_SETTING_AUTOPLAY) == CONFIG_SETTING_ON &&
                    context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PLAYING
                ) {
                    BC127CommandPlay(context->bt);
                }
            } else if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON &&
                       context->bt->activeDevice.hfpLinkId == 0
            ) {
                char *macId = (char *) context->bt->activeDevice.macId;
                BC127CommandProfileOpen(context->bt, macId, "HFP");
            }
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                IBusCommandTELSetLED(context->ibus, IBUS_TEL_LED_STATUS_GREEN);
            }
        }
    } else {
        BC127CommandClose(context->bt, BC127_CLOSE_ALL);
    }
}

/**
 * HandlerBC127DeviceDisconnected()
 *     Description:
 *         If a device disconnects and our ignition is on,
 *         make the module connectable again.
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127DeviceDisconnected(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    // Reset the metadata so we don't display the wrong data
    BC127ClearMetadata(context->bt);
    BC127ClearPairingErrors(context->bt);
    if (context->ibus->ignitionStatus > IBUS_IGNITION_OFF) {
        BC127CommandBtState(
            context->bt,
            BC127_STATE_ON,
            context->bt->discoverable
        );
        if (context->btConnectionStatus == HANDLER_BT_CONN_CHANGE) {
            BC127PairedDevice_t *dev = &context->bt->pairedDevices[
                context->btSelectedDevice
            ];
            BC127CommandProfileOpen(context->bt, dev->macId, "A2DP");
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                BC127CommandProfileOpen(context->bt, dev->macId, "HFP");
            }
            context->btSelectedDevice = HANDLER_BT_SELECTED_DEVICE_NONE;
        } else {
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                IBusCommandTELSetLED(context->ibus, IBUS_TEL_LED_STATUS_RED);
            }
            BC127CommandList(context->bt);
        }
    }
    context->btConnectionStatus = HANDLER_BT_CONN_OFF;
}

/**
 * HandlerBC127DeviceFound()
 *     Description:
 *         If a device is found and we are not connected, connect to it
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127DeviceFound(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->bt->activeDevice.deviceId == 0 &&
        context->btConnectionStatus == HANDLER_BT_CONN_OFF &&
        context->ibus->ignitionStatus > IBUS_IGNITION_OFF
    ) {
        char *macId = (char *) data;
        LogDebug(LOG_SOURCE_SYSTEM, "Handler: No Device -- Attempt connection");
        BC127CommandProfileOpen(context->bt, macId, "A2DP");
        if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
            BC127CommandProfileOpen(context->bt, macId, "HFP");
        }
        context->btConnectionStatus = HANDLER_BT_CONN_ON;
    } else {
        LogDebug(
            LOG_SOURCE_SYSTEM,
            "Handler: Not connecting to new device %d %d %d",
            context->bt->activeDevice.deviceId,
            context->btConnectionStatus,
            context->ibus->ignitionStatus
        );
    }
}

/**
 * HandlerBC127PlaybackStatus()
 *     Description:
 *         If the application is starting, request the BC127 AVRCP Metadata
 *         if it is playing. If the CD Change status is not set to "playing"
 *         then we pause playback.
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerBC127PlaybackStatus(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    // If this is the first status update
    if (context->btStartupIsRun == 0) {
        if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
            // Request Metadata
            BC127CommandGetMetadata(context->bt);
        }
        context->btStartupIsRun = 1;
    }
    if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING &&
        context->ibus->cdChangerFunction == IBUS_CDC_FUNC_NOT_PLAYING
    ) {
        // We're playing but not in Bluetooth mode - stop playback
        BC127CommandPause(context->bt);
    }
}

/**
 * HandlerUICloseConnection()
 *     Description:
 *         Close the active connection and dissociate ourselves from it
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerUICloseConnection(void *ctx, unsigned char *data)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    // Reset the metadata so we don't display the wrong data
    BC127ClearMetadata(context->bt);
    // Clear the actively paired device
    BC127ClearActiveDevice(context->bt);
    // Enable connectivity
    BC127CommandBtState(context->bt, BC127_STATE_ON, context->bt->discoverable);
    BC127CommandClose(context->bt, BC127_CLOSE_ALL);

}

/**
 * HandlerUIInitiateConnection()
 *     Description:
 *         Handle the connection when a new device is selected in the UI
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerUIInitiateConnection(void *ctx, unsigned char *deviceId)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->bt->activeDevice.deviceId != 0) {
        BC127CommandClose(context->bt, BC127_CLOSE_ALL);
    }
    context->btSelectedDevice = (int8_t) *deviceId;
    context->btConnectionStatus = HANDLER_BT_CONN_CHANGE;
}

/**
 * HandlerIBusCDCStatus()
 *     Description:
 *         Track the current CD Changer status based on what the radio
 *         instructs us to do. We respond with exactly what the radio instructs
 *         even if we haven't done it yet. Otherwise, the radio will continue
 *         to accost us to do what it wants
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusCDCStatus(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    unsigned char curStatus = IBUS_CDC_STAT_STOP;
    unsigned char curFunction = IBUS_CDC_FUNC_NOT_PLAYING;
    unsigned char requestedCommand = pkt[4];
    if (requestedCommand == IBUS_CDC_CMD_GET_STATUS) {
        curFunction = context->ibus->cdChangerFunction;
        if (curFunction == IBUS_CDC_FUNC_PLAYING) {
            curStatus = IBUS_CDC_STAT_PLAYING;
        } else if (curFunction == IBUS_CDC_FUNC_PAUSE) {
            curStatus = IBUS_CDC_STAT_PAUSE;
        }
    } else if (requestedCommand == IBUS_CDC_CMD_STOP_PLAYING) {
        if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
            BC127CommandPause(context->bt);
        }
        curStatus = IBUS_CDC_STAT_STOP;
        curFunction = IBUS_CDC_FUNC_NOT_PLAYING;
        // Return to non-S/PDIF input once told to stop playback, if enabled
        if (ConfigGetSetting(CONFIG_SETTING_USE_SPDIF_INPUT) == CONFIG_SETTING_ON) {
            IBusCommandDSPSetMode(context->ibus, IBUS_DSP_MODE_INPUT_RADIO);
        }
    } else if (requestedCommand == IBUS_CDC_CMD_CHANGE_TRACK) {
        curFunction = context->ibus->cdChangerFunction;
        curStatus = IBUS_CDC_STAT_PLAYING;
        // Do not go backwards/forwards if the UI is CD53 because
        // those actions can be used to use the UI
        if (context->uiMode != IBus_UI_CD53) {
            if (pkt[5] == 0x00) {
                BC127CommandForward(context->bt);
            } else {
                BC127CommandBackward(context->bt);
            }
        }
    } else if (requestedCommand == IBUS_CDC_CMD_SEEK) {
        if (pkt[5] == 0x00) {
            context->seekMode = HANDLER_CDC_SEEK_MODE_REV;
            BC127CommandBackwardSeekPress(context->bt);
        } else {
            context->seekMode = HANDLER_CDC_SEEK_MODE_FWD;
            BC127CommandForwardSeekPress(context->bt);
        }
        curStatus = IBUS_CDC_STAT_FAST_REV;
    } else if (requestedCommand == IBUS_CDC_CMD_SCAN) {
        curStatus = 0x00;
        // The 5th octet in the packet tells the CDC if we should
        // enable or disable the given mode
        if (pkt[5] == 0x01) {
            curFunction = IBUS_CDC_FUNC_SCAN_MODE;
        } else {
            curFunction = IBUS_CDC_FUNC_PLAYING;
        }
    } else if (requestedCommand == IBUS_CDC_CMD_RANDOM_MODE) {
        curStatus = 0x00;
        // The 5th octet in the packet tells the CDC if we should
        // enable or disable the given mode
        if (pkt[5] == 0x01) {
            curFunction = IBUS_CDC_FUNC_RANDOM_MODE;
        } else {
            curFunction = IBUS_CDC_FUNC_PLAYING;
        }
    } else {
        if (requestedCommand == IBUS_CDC_CMD_PAUSE_PLAYING) {
            curStatus = IBUS_CDC_STAT_PAUSE;
            curFunction = IBUS_CDC_FUNC_PAUSE;
        } else if (requestedCommand == IBUS_CDC_CMD_START_PLAYING) {
            curStatus = IBUS_CDC_STAT_PLAYING;
            curFunction = IBUS_CDC_FUNC_PLAYING;
            if (context->seekMode == HANDLER_CDC_SEEK_MODE_FWD) {
                BC127CommandForwardSeekRelease(context->bt);
                context->seekMode = HANDLER_CDC_SEEK_MODE_NONE;
            } else if (context->seekMode == HANDLER_CDC_SEEK_MODE_REV) {
                BC127CommandBackwardSeekRelease(context->bt);
                context->seekMode = HANDLER_CDC_SEEK_MODE_NONE;
            }
            // Set the Input to S/PDIF once told to start playback, if enabled
            if (ConfigGetSetting(CONFIG_SETTING_USE_SPDIF_INPUT) == CONFIG_SETTING_ON) {
                IBusCommandDSPSetMode(context->ibus, IBUS_DSP_MODE_INPUT_SPDIF);
            }
        } else {
            curStatus = requestedCommand;
        }
    }
    unsigned char discCount = IBUS_CDC_DISC_COUNT_6;
    if (context->uiMode == IBus_UI_BMBT) {
        discCount = IBUS_CDC_DISC_COUNT_1;
    }
    IBusCommandCDCStatus(context->ibus, curStatus, curFunction, discCount);
    context->cdChangerLastPoll = TimerGetMillis();
    context->cdChangerLastStatus = TimerGetMillis();
}

/**
 * HandlerIBusFirstMessageReceived()
 *     Description:
 *         Request module status after the first IBus message is received.
 *         DO NOT change the order in which these modules are polled.
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusFirstMessageReceived(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    IBusCommandGetModuleStatus(
        context->ibus,
        IBUS_DEVICE_RAD,
        IBUS_DEVICE_IKE
    );
    IBusCommandGetModuleStatus(
        context->ibus,
        IBUS_DEVICE_RAD,
        IBUS_DEVICE_GT
    );
    IBusCommandGetModuleStatus(
        context->ibus,
        IBUS_DEVICE_RAD,
        IBUS_DEVICE_MID
    );
    IBusCommandGetModuleStatus(
        context->ibus,
        IBUS_DEVICE_CDC,
        IBUS_DEVICE_RAD
    );
    IBusCommandGetModuleStatus(
        context->ibus,
        IBUS_DEVICE_IKE,
        IBUS_DEVICE_LCM
    );
    if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
        IBusCommandSetModuleStatus(
            context->ibus,
            IBUS_DEVICE_TEL,
            IBUS_DEVICE_LOC,
            0x01
        );
    }
    IBusCommandIKEGetIgnitionStatus(context->ibus);
}

/**
 * HandlerIBusGMDoorsFlapStatusResponse()
 *     Description:
 *         Track which doors have been opened while the ignition was on
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *type - The navigation type
 *     Returns:
 *         void
 */
void HandlerIBusGMDoorsFlapsStatusResponse(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->bodyModuleStatus.lowSideDoors == 0) {
        unsigned char doorStatus = pkt[4] & 0x0F;
        if (doorStatus > 0x01) {
            context->bodyModuleStatus.lowSideDoors = 1;
        }
    }
    unsigned char lockStatus = pkt[4] & 0xF0;
    if (CHECK_BIT(lockStatus, 4) != 0) {
        LogInfo(LOG_SOURCE_SYSTEM, "Handler: Centrol Locks unlocked");
        context->bodyModuleStatus.doorsLocked = 0;
    } else if (CHECK_BIT(lockStatus, 5) != 0) {
        LogInfo(LOG_SOURCE_SYSTEM, "Handler: Centrol Locks locked");
        context->bodyModuleStatus.doorsLocked = 1;
    }
}

/**
 * HandlerIBusGTDIAIdentityResponse()
 *     Description:
 *         Identify the navigation module hardware and software versions
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *type - The navigation type
 *     Returns:
 *         void
 */
void HandlerIBusGTDIAIdentityResponse(void *ctx, unsigned char *type)
{
    unsigned char navType = *type;
    if (ConfigGetNavType() != navType) {
        ConfigSetNavType(navType);
    }
}

/**
 * HandlerIBusGTDIAOSIdentityResponse()
 *     Description:
 *         Identify the navigation module type from its OS
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - Data packet
 *     Returns:
 *         void
 */
void HandlerIBusGTDIAOSIdentityResponse(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    char navigationOS[8] = {};
    uint8_t i;
    for (i = 0; i < 7; i++) {
        navigationOS[i] = pkt[i + 4];
    }
    // The string should come null terminated, but we should not trust that
    navigationOS[7] = '\0';
    if (UtilsStricmp(navigationOS, "BMWC01S") == 0) {
        if (context->ibusModuleStatus.MID == 0) {
            if (ConfigGetUIMode() != IBus_UI_BMBT) {
                LogInfo(LOG_SOURCE_SYSTEM, "Detected BMBT UI");
                HandlerSwitchUI(context, IBus_UI_BMBT);
            }
        } else {
            if (ConfigGetUIMode() != IBus_UI_MID_BMBT) {
                LogInfo(LOG_SOURCE_SYSTEM, "Detected MID / BMBT UI");
                HandlerSwitchUI(context, IBus_UI_MID_BMBT);
            }
        }
    } else if (UtilsStricmp(navigationOS, "BMWM01S") == 0) {
        if (ConfigGetUIMode() != IBus_UI_BUSINESS_NAV) {
            LogInfo(LOG_SOURCE_SYSTEM, "Detected Business Nav UI");
            HandlerSwitchUI(context, IBus_UI_BUSINESS_NAV);
        }
    } else {
        LogError("Unable to identify GT OS: %s", navigationOS);
    }
}

/**
 * HandlerIBusIKEIgnitionStatus()
 *     Description:
 *         Track the Ignition state and update the BC127 accordingly. We set
 *         the BT device "off" when the key is set to position 0 and on
 *         as soon as it goes to a position >= 1.
 *         Request the LCM status when the car is turned to or past position 1
 *         Unlock the vehicle once the key is turned to position 1
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusIKEIgnitionStatus(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    unsigned char ignitionStatus = pkt[0];
    // If the ignition status has changed
    if (ignitionStatus != context->ibus->ignitionStatus) {
        // If the first bit is set, the key is in position 1 at least, otherwise
        // the ignition is off
        if (ignitionStatus == IBUS_IGNITION_OFF) {
            // Set the BT module not connectable/discoverable. Disconnect all devices
            BC127CommandBtState(context->bt, BC127_STATE_OFF, BC127_STATE_OFF);
            BC127CommandClose(context->bt, BC127_CLOSE_ALL);
            BC127ClearPairedDevices(context->bt);
            // Unlock the vehicle
            if (ConfigGetSetting(CONFIG_SETTING_COMFORT_LOCKS_ADDRESS) ==
                CONFIG_SETTING_ON
            ) {
                if (context->ibus->vehicleType == IBUS_VEHICLE_TYPE_E38_E39_E53) {
                    IBusCommandGMDoorCenterLockButton(context->ibus);
                } else {
                    if (context->bodyModuleStatus.lowSideDoors == 1) {
                        IBusCommandGMDoorUnlockAll(context->ibus);
                    } else {
                        IBusCommandGMDoorUnlockHigh(context->ibus);
                    }
                }
            }
            context->bodyModuleStatus.lowSideDoors = 0;
        // If the ignition WAS off, but now it's not, then run these actions.
        // I realize the second condition is frivolous, but it helps with
        // readability.
        } else if (context->ibus->ignitionStatus == IBUS_IGNITION_OFF &&
                   ignitionStatus != IBUS_IGNITION_OFF
        ) {
            LogDebug(LOG_SOURCE_SYSTEM, "Handler: Ignition On");
            // Play a tone to wake up the WM8804 / PCM5122
            BC127CommandTone(Context.bt, "V 0 N C6 L 4");
            // Anounce the CDC to the network
            HandlerIBusBroadcastCDCStatus(context);
            // Reset the metadata so we don't display the wrong data
            BC127ClearMetadata(context->bt);
            // Set the BT module connectable
            BC127CommandBtState(context->bt, BC127_STATE_ON, BC127_STATE_OFF);
            // Request BC127 state
            BC127CommandStatus(context->bt);
            BC127CommandList(context->bt);
            // Enable the TEL LEDs
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                if (context->bt->activeDevice.avrcpLinkId == 0 &&
                    context->bt->activeDevice.a2dpLinkId == 0
                ) {
                    IBusCommandTELSetLED(context->ibus, IBUS_TEL_LED_STATUS_RED);
                } else {
                    IBusCommandTELSetLED(context->ibus, IBUS_TEL_LED_STATUS_GREEN);
                }
            }
            // Ask the LCM for the redundant data
            LogDebug(LOG_SOURCE_SYSTEM, "Handler: Request LCM Redundant Data");
            IBusCommandLCMGetRedundantData(context->ibus);
        }
    } else {
        if (ignitionStatus > IBUS_IGNITION_OFF) {
            HandlerIBusBroadcastCDCStatus(context);
            if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
                HandlerIBusBroadcastTELStatus(context);
                if (context->bt->activeDevice.avrcpLinkId != 0 &&
                    context->bt->activeDevice.a2dpLinkId != 0
                ) {
                    IBusCommandTELSetLED(
                        context->ibus,
                        IBUS_TEL_LED_STATUS_GREEN
                    );
                } else {
                    IBusCommandTELSetLED(
                        context->ibus,
                        IBUS_TEL_LED_STATUS_RED
                    );
                }
            }
        }
    }
    if (context->ibusModuleStatus.IKE == 0) {
        HandlerIBusBroadcastTELStatus(context);
        context->ibusModuleStatus.IKE = 1;
    }
}

/**
 * HandlerIBusIKESpeedRPMUpdate()
 *     Description:
 *         Act upon updates from the IKE about the vehicle speed / RPM
 *         * Lock the vehicle at 20mph
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusIKESpeedRPMUpdate(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (ConfigGetSetting(CONFIG_SETTING_COMFORT_LOCKS_ADDRESS) ==
        CONFIG_SETTING_ON &&
        context->bodyModuleStatus.doorsLocked == 0x00
        && pkt[4] >= 0x20
    ) {
        if (context->ibus->vehicleType == IBUS_VEHICLE_TYPE_E38_E39_E53) {
            IBusCommandGMDoorCenterLockButton(context->ibus);
        } else {
            IBusCommandGMDoorLockHigh(context->ibus);
        }
    }
}

/**
 * HandlerIBusIKEVehicleType()
 *     Description:
 *         Set the vehicle type
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - The IBus Packet
 *     Returns:
 *         void
 */
void HandlerIBusIKEVehicleType(void *ctx, unsigned char *pkt)
{
    unsigned char rawVehicleType = (pkt[4] >> 4) & 0xF;
    unsigned char detectedVehicleType = IBusGetVehicleType(pkt);
    if (detectedVehicleType == 0xFF) {
        LogError("Handler: Unknown Vehicle Detected");
    }
    if (detectedVehicleType != ConfigGetVehicleType() &&
        detectedVehicleType != 0xFF
    ) {
        ConfigSetVehicleType(detectedVehicleType);
        if (rawVehicleType == 0x0A || rawVehicleType == 0x0F) {
            ConfigSetIKEType(IBUS_IKE_TYPE_LOW);
            LogDebug(LOG_SOURCE_SYSTEM, "Detected New Vehicle Type: E46/Z4");
        } else if (rawVehicleType == 0x02) {
            ConfigSetIKEType(IBUS_IKE_TYPE_LOW);
            LogDebug(
                LOG_SOURCE_SYSTEM,
                "Detected New Vehicle Type: E38/E39/E53 - Low OBC"
            );
        } else if (rawVehicleType == 0x00) {
            ConfigSetIKEType(IBUS_IKE_TYPE_HIGH);
            LogDebug(
                LOG_SOURCE_SYSTEM,
                "Detected New Vehicle Type: E38/E39/E53 - High OBC"
            );
        }
    }
}

/**
 * HandlerIBusLightStatus()
 *     Description:
 *         Track the Light Status messages in case the user has configured
 *         Three/Five One-Touch Blinkers.
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusLCMLightStatus(void *ctx, unsigned char *pkt)
{
    uint8_t blinkCount = ConfigGetSetting(CONFIG_SETTING_COMFORT_BLINKERS);
    if (blinkCount > 0x01 && blinkCount != 0xFF) {
        HandlerContext_t *context = (HandlerContext_t *) ctx;
        unsigned char lightStatus = pkt[4];
        if (context->blinkerStatus == HANDLER_BLINKER_OFF) {
            context->blinkerCount = 2;
            if (CHECK_BIT(lightStatus, IBUS_LCM_DRV_SIG_BIT) != 0 &&
                CHECK_BIT(lightStatus, IBUS_LCM_PSG_SIG_BIT) == 0
            ) {
                context->blinkerStatus = HANDLER_BLINKER_DRV;
                IBusCommandLCMEnableBlinker(context->ibus, IBUS_LCM_BLINKER_DRV);
            } else if (CHECK_BIT(lightStatus, IBUS_LCM_PSG_SIG_BIT) != 0 &&
                CHECK_BIT(lightStatus, IBUS_LCM_DRV_SIG_BIT) == 0
            ) {
                context->blinkerStatus = HANDLER_BLINKER_PSG;
                IBusCommandLCMEnableBlinker(context->ibus, IBUS_LCM_BLINKER_PSG);
            }
        } else if (context->blinkerStatus == HANDLER_BLINKER_DRV) {
            if (CHECK_BIT(lightStatus, IBUS_LCM_PSG_SIG_BIT) != 0 ||
                context->blinkerCount == blinkCount
            ) {
                // Reset ourselves once the signal is off so we do not
                // reactivate and signal in increments of `blinkCount`
                if (CHECK_BIT(lightStatus, IBUS_LCM_DRV_SIG_BIT) == 0) {
                    context->blinkerStatus = HANDLER_BLINKER_OFF;
                }
                IBusCommandDIATerminateDiag(context->ibus, IBUS_DEVICE_LCM);
            } else {
                context->blinkerCount++;
            }
        } else if (context->blinkerStatus == HANDLER_BLINKER_PSG) {
            if (CHECK_BIT(lightStatus, IBUS_LCM_DRV_SIG_BIT) != 0 ||
                context->blinkerCount == blinkCount
            ) {
                // Reset ourselves once the signal is off so we do not
                // reactivate and signal in increments of `blinkCount`
                if (CHECK_BIT(lightStatus, IBUS_LCM_PSG_SIG_BIT) == 0) {
                    context->blinkerStatus = HANDLER_BLINKER_OFF;
                }
                IBusCommandDIATerminateDiag(context->ibus, IBUS_DEVICE_LCM);
            } else {
                context->blinkerCount++;
            }
        }
    }
}

/**
 * HandlerIBusLCMDimmerStatus()
 *     Description:
 *         Track the Dimmer Status messages so we can correctly set the
 *         dimming state when messing with the lighting
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusLCMDimmerStatus(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    IBusCommandDIAGetIOStatus(context->ibus, IBUS_DEVICE_LCM);
}

/**
 * HandlerIBusLCMRedundantData()
 *     Description:
 *         Check the VIN to see if we're in a new vehicle
 *         Raw: D0 10 80 54 50 4E 66 05 80 06 10 42 38 07 00 06 05 81
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *tmp - Any event data
 *     Returns:
 *         void
 */
void HandlerIBusLCMRedundantData(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    // Check VIN
    unsigned char vehicleId[] = {
        pkt[4],
        pkt[5],
        pkt[6],
        pkt[7],
        (pkt[8] >> 4) & 0xF,
    };
    unsigned char currentVehicleId[5] = {};
    ConfigGetVehicleIdentity(currentVehicleId);
    char vinTwo[] = {vehicleId[0], vehicleId[1], '\0'};
    char currentVinTwo[] = {currentVehicleId[0], currentVehicleId[1], '\0'};
    LogRaw(
        "Got VIN: %s%d%d%d%d%d\r\nExisting VIN: %s%d%d%d%d%d\r\n",
        vinTwo,
        (vehicleId[2] >> 4) & 0xF,
        vehicleId[2] & 0xF,
        (vehicleId[3] >> 4) & 0xF,
        vehicleId[3] & 0xF,
        vehicleId[4],
        currentVinTwo,
        (currentVehicleId[2] >> 4) & 0xF,
        currentVehicleId[2] & 0xF,
        (currentVehicleId[3] >> 4) & 0xF,
        currentVehicleId[3] & 0xF,
        currentVehicleId[4]
    );
    if (memcmp(&vehicleId, &currentVehicleId, 5) != 0) {
        LogDebug(LOG_SOURCE_SYSTEM, "Detected VIN Change");
        // Save the new VIN
        ConfigSetVehicleIdentity(vehicleId);
        // Request the vehicle type
        IBusCommandIKEGetVehicleType(context->ibus);
        // Fallback for vehicle UI Identification
        if (context->ibusModuleStatus.RAD != 0 &&
            context->ibusModuleStatus.MID == 0 &&
            context->ibusModuleStatus.GT == 0
        ) {
            LogInfo(LOG_SOURCE_SYSTEM, "Detected CD53 UI");
            HandlerSwitchUI(context, IBus_UI_CD53);
        }
    }
}

/**
 * HandlerIBusMFLButton()
 *     Description:
 *         Act upon MFL button presses when in CD Changer mode (when BT is active)
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - The packet
 *     Returns:
 *         void
 */
void HandlerIBusMFLButton(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    unsigned char mflButton = pkt[4];
    if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
        if (mflButton == IBusMFLButtonVoicePress) {
            context->mflButtonStatus = HANDLER_MFL_STATUS_OFF;
        }
        if (mflButton == IBusMFLButtonVoiceRelease &&
            context->mflButtonStatus == HANDLER_MFL_STATUS_OFF
        ) {
            if (context->bt->callStatus == BC127_CALL_ACTIVE) {
                BC127CommandCallEnd(context->bt);
            } else if (context->bt->callStatus == BC127_CALL_INCOMING) {
                BC127CommandCallAnswer(context->bt);
            } else if (context->bt->callStatus == BC127_CALL_OUTGOING) {
                BC127CommandCallEnd(context->bt);
            } else if(context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PLAYING) {
                if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
                    BC127CommandPause(context->bt);
                } else {
                    BC127CommandPlay(context->bt);
                }
            }
        } else if (mflButton == IBusMFLButtonVoiceHold) {
            context->mflButtonStatus = HANDLER_MFL_STATUS_SPEAK_HOLD;
            BC127CommandToggleVR(context->bt);
        }
    } else {
        if (mflButton == IBusMFLButtonVoiceRelease) {
            if(context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PLAYING) {
                if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
                    BC127CommandPause(context->bt);
                } else {
                    BC127CommandPlay(context->bt);
                }
            }
        }
    }
}

/**
 * HandlerIBusMFLButton()
 *     Description:
 *         Act upon MFL Volume commands to control call volume
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - The packet
 *     Returns:
 *         void
 */
void HandlerIBusMFLVolume(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->bt->callStatus != BC127_CALL_INACTIVE) {
        if (pkt[4] == IBusMFLVolUp) {
            BC127CommandVolume(context->bt, 13, "UP");
        } else if (pkt[4] == IBusMFLVolDown) {
            BC127CommandVolume(context->bt, 13, "DOWN");
        }
    }
}

/**
 * HandlerIBusModuleStatusRequest()
 *     Description:
 *         Respond to module status requests for those modules which
 *         we are emulating
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - The IBus packet
 *     Returns:
 *         void
 */
void HandlerIBusModuleStatusRequest(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (pkt[IBUS_PKT_DST] == IBUS_DEVICE_CDC) {
        IBusCommandSetModuleStatus(
            context->ibus,
            IBUS_DEVICE_CDC,
            pkt[IBUS_PKT_SRC],
            0x00
        );
        context->cdChangerLastPoll = TimerGetMillis();
    } else if (pkt[IBUS_PKT_DST] == IBUS_DEVICE_TEL &&
               ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON
    ) {
        IBusCommandSetModuleStatus(
            context->ibus,
            IBUS_DEVICE_TEL,
            pkt[IBUS_PKT_SRC],
            0x01
        );
    }
}

/**
 * HandlerIBusModuleStatusResponse()
 *     Description:
 *         Track module status as we get them & track UI changes
 *     Params:
 *         void *ctx - The context provided at registration
 *         unsigned char *pkt - The packet
 *     Returns:
 *         void
 */
void HandlerIBusModuleStatusResponse(void *ctx, unsigned char *pkt)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    unsigned char module = pkt[IBUS_PKT_SRC];
    if (module == IBUS_DEVICE_DSP &&
        context->ibusModuleStatus.DSP == 0
    ) {
        context->ibusModuleStatus.DSP = 1;
        LogInfo(LOG_SOURCE_SYSTEM, "DSP Detected");
    } else if (module == IBUS_DEVICE_GT &&
        context->ibusModuleStatus.GT == 0
    ) {
        context->ibusModuleStatus.GT = 1;
        LogInfo(LOG_SOURCE_SYSTEM, "GT Detected");
        unsigned char uiMode = ConfigGetUIMode();
        if (uiMode != IBus_UI_BMBT &&
            uiMode != IBus_UI_MID_BMBT &&
            uiMode != IBus_UI_BUSINESS_NAV
        ) {
            // Request the Navigation Identity
            IBusCommandDIAGetIdentity(context->ibus, IBUS_DEVICE_GT);
            IBusCommandDIAGetOSIdentity(context->ibus, IBUS_DEVICE_GT);
        }
    } else if (module == IBUS_DEVICE_LCM &&
        context->ibusModuleStatus.LCM == 0
    ) {
        LogInfo(LOG_SOURCE_SYSTEM, "LCM Detected");
        context->ibusModuleStatus.LCM = 1;
    } else if (module == IBUS_DEVICE_MID &&
        context->ibusModuleStatus.MID == 0
    ) {
        context->ibusModuleStatus.MID = 1;
        LogInfo(LOG_SOURCE_SYSTEM, "MID Detected");
        unsigned char uiMode = ConfigGetUIMode();
        if (uiMode != IBus_UI_MID &&
            uiMode != IBus_UI_MID_BMBT
        ) {
            if (context->ibusModuleStatus.GT == 1) {
                LogInfo(LOG_SOURCE_SYSTEM, "Detected MID / BMBT UI");
                HandlerSwitchUI(context, IBus_UI_MID_BMBT);
            } else {
                LogInfo(LOG_SOURCE_SYSTEM, "Detected MID UI");
                HandlerSwitchUI(context, IBus_UI_MID);
            }
        }
    } else if (module == IBUS_DEVICE_BMBT &&
        context->ibusModuleStatus.BMBT == 0
    ) {
        context->ibusModuleStatus.BMBT = 1;
        LogInfo(LOG_SOURCE_SYSTEM, "BMBT Detected");
    } else if (module == IBUS_DEVICE_RAD &&
        context->ibusModuleStatus.RAD == 0
    ) {
        context->ibusModuleStatus.RAD = 1;
        LogInfo(LOG_SOURCE_SYSTEM, "RAD Detected");
    }
}

/**
 * HandlerIBusBroadcastCDCStatus()
 *     Description:
 *         Wrapper to send the CDC Status
 *     Params:
 *         HandlerContext_t *context - The handler context
 *     Returns:
 *         void
 */
void HandlerIBusBroadcastCDCStatus(HandlerContext_t *context)
{
    unsigned char curStatus = IBUS_CDC_STAT_STOP;
    if (context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PAUSE) {
        curStatus = IBUS_CDC_FUNC_PAUSE;
    } else if (context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PLAYING) {
        curStatus = IBUS_CDC_STAT_PLAYING;
    }
    unsigned char discCount = IBUS_CDC_DISC_COUNT_6;
    if (context->uiMode == IBus_UI_BMBT) {
        discCount = IBUS_CDC_DISC_COUNT_1;
    }
    IBusCommandCDCStatus(
        context->ibus,
        curStatus,
        context->ibus->cdChangerFunction,
        discCount
    );
    context->cdChangerLastStatus = TimerGetMillis();
}

/**
 * HandlerIBusBroadcastTELStatus()
 *     Description:
 *         Send the TEL status to the vehicle
 *     Params:
 *         HandlerContext_t *context - The module context
 *     Returns:
 *         void
 */
void HandlerIBusBroadcastTELStatus(HandlerContext_t *context)
{
    if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
        if (context->bt->callStatus == BC127_CALL_INACTIVE) {
            IBusCommandTELStatus(
                context->ibus,
                IBUS_TEL_STATUS_ACTIVE_POWER_HANDSFREE
            );
        } else {
            IBusCommandTELStatus(
                context->ibus,
                IBUS_TEL_STATUS_ACTIVE_POWER_CALL_HANDSFREE
            );
        }
    }
}

/**
 * HandlerTimerCDCAnnounce()
 *     Description:
 *         This periodic task tracks how long it has been since the radio
 *         sent us (the CDC) a "ping". We should re-announce ourselves if that
 *         value reaches the timeout specified and the ignition is on.
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerCDCAnnounce(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    uint32_t now = TimerGetMillis();
    if ((now - context->cdChangerLastPoll) >= HANDLER_CDC_ANOUNCE_TIMEOUT &&
        context->ibus->ignitionStatus > IBUS_IGNITION_OFF
    ) {
        IBusCommandSetModuleStatus(
            context->ibus,
            IBUS_DEVICE_CDC,
            IBUS_DEVICE_LOC,
            0x00
        );
        context->cdChangerLastPoll = now;
    }
}

/**
 * HandlerTimerCDCSendStatus()
 *     Description:
 *         This periodic task will proactively send the CDC status to the BM5x
 *         radio if we don't see a status poll within the last 20000 milliseconds.
 *         The CDC poll happens every 19945 milliseconds
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerCDCSendStatus(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    uint32_t now = TimerGetMillis();
    if ((now - context->cdChangerLastStatus) >= HANDLER_CDC_STATUS_TIMEOUT &&
        context->ibus->ignitionStatus > IBUS_IGNITION_OFF &&
        (context->uiMode == IBus_UI_BMBT || context->uiMode == IBus_UI_MID_BMBT)
    ) {
        HandlerIBusBroadcastCDCStatus(context);
        LogDebug(LOG_SOURCE_SYSTEM, "Handler: Send CDC status preemptively");
    }
}

/**
 * HandlerTimerDeviceConnection()
 *     Description:
 *         Monitor the BT connection and ensure it stays connected
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerDeviceConnection(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (strlen(context->bt->activeDevice.macId) > 0 &&
        context->bt->activeDevice.a2dpLinkId == 0
    ) {
        if (context->btDeviceConnRetries <= HANDLER_DEVICE_MAX_RECONN) {
            LogDebug(
                LOG_SOURCE_SYSTEM,
                "Handler: A2DP link closed -- Attempting to connect"
            );
            BC127CommandProfileOpen(
                context->bt,
                context->bt->activeDevice.macId,
                "A2DP"
            );
            context->btDeviceConnRetries += 1;
        } else {
            LogError("Handler: Giving up on BT connection");
            context->btDeviceConnRetries = 0;
            // Enable connectivity
            BC127CommandBtState(context->bt, BC127_STATE_ON, context->bt->discoverable);
            BC127ClearPairedDevices(context->bt);
            BC127CommandClose(context->bt, BC127_CLOSE_ALL);
        }
    } else if (context->btDeviceConnRetries > 0) {
        context->btDeviceConnRetries = 0;
    }
}

/**
 * HandlerTimerLCMIOStatus()
 *     Description:
 *         Request the LCM I/O Status when the key is in position 2 or above
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerLCMIOStatus(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (context->ibusModuleStatus.LCM != 0 &&
        context->ibus->ignitionStatus != IBUS_IGNITION_OFF
    ) {
        // Ask the LCM for the I/O Status of all lamps
        IBusCommandDIAGetIOStatus(context->ibus, IBUS_DEVICE_LCM);
    }
}

/**
 * HandlerTimerOpenProfileErrors()
 *     Description:
 *         If there are any profile open errors, request the profile
 *         be opened again
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerOpenProfileErrors(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (strlen(context->bt->activeDevice.macId) > 0) {
        uint8_t idx;
        for (idx = 0; idx < BC127_PROFILE_COUNT; idx++) {
            if (context->bt->pairingErrors[idx] == 1) {
                LogDebug(LOG_SOURCE_SYSTEM, "Handler: Attempting to resolve pairing error");
                BC127CommandProfileOpen(
                    context->bt,
                    context->bt->activeDevice.macId,
                    PROFILES[idx]
                );
                context->bt->pairingErrors[idx] = 0;
            }
        }
    }
}

/**
 * HandlerTimerPoweroff()
 *     Description:
 *         Track the time since the last IBus message and see if we need to
 *         power off.
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerPoweroff(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (ConfigGetPoweroffTimeoutDisabled() == CONFIG_SETTING_ENABLED) {
        uint32_t lastRx = TimerGetMillis() - context->ibus->rxLastStamp;
        if (lastRx >= HANDLER_POWER_TIMEOUT_MILLIS) {
            if (context->powerStatus == HANDLER_POWER_ON) {
                // Destroy the UART module for IBus
                UARTDestroy(IBUS_UART_MODULE);
                TimerDelayMicroseconds(500);
                LogInfo(LOG_SOURCE_SYSTEM, "System Power Down!");
                context->powerStatus = HANDLER_POWER_OFF;
                // Disable the TH3122
                IBUS_EN = 0;
            } else {
                // Re-enable the TH3122 EN line so we can try pulling it,
                // and the regulator low again
                IBUS_EN = 1;
                context->powerStatus = HANDLER_POWER_ON;
            }
        }
    }
}

/**
 * HandlerTimerScanDevices()
 *     Description:
 *         Rescan for devices on the PDL periodically. Scan every 5 seconds if
 *         there is no connected device, otherwise every 60 seconds
 *     Params:
 *         void *ctx - The context provided at registration
 *     Returns:
 *         void
 */
void HandlerTimerScanDevices(void *ctx)
{
    HandlerContext_t *context = (HandlerContext_t *) ctx;
    if (((context->bt->activeDevice.deviceId == 0 &&
        context->btConnectionStatus == HANDLER_BT_CONN_OFF) ||
        context->scanIntervals == 12) &&
        context->ibus->ignitionStatus > IBUS_IGNITION_OFF
    ) {
        context->scanIntervals = 0;
        BC127ClearInactivePairedDevices(context->bt);
        BC127CommandList(context->bt);
    } else {
        context->scanIntervals += 1;
    }
}
