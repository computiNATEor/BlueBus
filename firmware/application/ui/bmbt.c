/*
 * File: bmbt.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the BoardMonitor UI Mode handler
 */
#include "bmbt.h"
static BMBTContext_t Context;
uint8_t menuSettings[] = {
    BMBT_MENU_IDX_SETTINGS_AUDIO,
    BMBT_MENU_IDX_SETTINGS_CALLING,
    BMBT_MENU_IDX_SETTINGS_COMFORT,
    BMBT_MENU_IDX_SETTINGS_UI
};
char *menuSettingsLabels[] = {
    "Audio",
    "Calling",
    "Comfort",
    "UI"
};

void BMBTInit(BC127_t *bt, IBus_t *ibus)
{
    Context.bt = bt;
    Context.ibus = ibus;
    Context.menu = BMBT_MENU_NONE;
    Context.status.playerMode = BMBT_MODE_INACTIVE;
    Context.status.displayMode = BMBT_DISPLAY_OFF;
    Context.status.navState = BMBT_NAV_STATE_ON;
    Context.status.navIndexType = IBUS_CMD_GT_WRITE_INDEX_TMC;
    Context.status.radType = IBUS_RADIO_TYPE_BM53;
    Context.writtenIndices = 3;
    Context.timerHeaderIntervals = BMBT_MENU_HEADER_TIMER_OFF;
    Context.timerMenuIntervals = BMBT_MENU_HEADER_TIMER_OFF;
    Context.mainDisplay = UtilsDisplayValueInit("Bluetooth", BMBT_DISPLAY_OFF);
    EventRegisterCallback(
        BC127Event_DeviceConnected,
        &BMBTBC127DeviceConnected,
        &Context
    );
    EventRegisterCallback(
        BC127Event_DeviceDisconnected,
        &BMBTBC127DeviceDisconnected,
        &Context
    );
    EventRegisterCallback(
        BC127Event_MetadataChange,
        &BMBTBC127Metadata,
        &Context
    );
    EventRegisterCallback(
        BC127Event_Boot,
        &BMBTBC127Ready,
        &Context
    );
    EventRegisterCallback(
        BC127Event_PlaybackStatusChange,
        &BMBTBC127PlaybackStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_BMBTButton,
        &BMBTIBusBMBTButtonPress,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_CDStatusRequest,
        &BMBTIBusCDChangerStatus,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_GTChangeUIRequest,
        &BMBTIBusGTChangeUIRequest,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_GTMenuSelect,
        &BMBTIBusMenuSelect,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_RADDisplayMenu,
        &BMBTRADDisplayMenu,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_RADUpdateMainArea,
        &BMBTRADUpdateMainArea,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_ValueUpdate,
        &BMBTIBusValueUpdate,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_ScreenModeSet,
        &BMBTScreenModeSet,
        &Context
    );
    EventRegisterCallback(
        IBusEvent_ScreenModeUpdate,
        &BMBTScreenModeUpdate,
        &Context
    );
    Context.headerWriteTaskId = TimerRegisterScheduledTask(
        &BMBTTimerHeaderWrite,
        &Context,
        BMBT_HEADER_TIMER_WRITE_INT
    );
    Context.menuWriteTaskId = TimerRegisterScheduledTask(
        &BMBTTimerMenuWrite,
        &Context,
        BMBT_MENU_TIMER_WRITE_INT
    );
    Context.displayUpdateTaskId = TimerRegisterScheduledTask(
        &BMBTTimerScrollDisplay,
        &Context,
        BMBT_SCROLL_TEXT_TIMER
    );
}

/**
 * BMBTDestroy()
 *     Description:
 *         Unregister all event handlers, scheduled tasks and clear the context
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void BMBTDestroy()
{
    EventUnregisterCallback(
        BC127Event_DeviceConnected,
        &BMBTBC127DeviceConnected
    );
    EventUnregisterCallback(
        BC127Event_DeviceDisconnected,
        &BMBTBC127DeviceDisconnected
    );
    EventUnregisterCallback(
        BC127Event_MetadataChange,
        &BMBTBC127Metadata
    );
    EventUnregisterCallback(
        BC127Event_Boot,
        &BMBTBC127Ready
    );
    EventUnregisterCallback(
        BC127Event_PlaybackStatusChange,
        &BMBTBC127PlaybackStatus
    );
    EventUnregisterCallback(
        IBusEvent_BMBTButton,
        &BMBTIBusBMBTButtonPress
    );
    EventUnregisterCallback(
        IBusEvent_CDStatusRequest,
        &BMBTIBusCDChangerStatus
    );
    EventUnregisterCallback(
        IBusEvent_GTChangeUIRequest,
        &BMBTIBusGTChangeUIRequest
    );
    EventUnregisterCallback(
        IBusEvent_GTMenuSelect,
        &BMBTIBusMenuSelect
    );
    EventUnregisterCallback(
        IBusEvent_RADDisplayMenu,
        &BMBTRADDisplayMenu
    );
    EventUnregisterCallback(
        IBusEvent_RADUpdateMainArea,
        &BMBTRADUpdateMainArea
    );
    EventUnregisterCallback(
        IBusEvent_ValueUpdate,
        &BMBTIBusValueUpdate
    );
    EventUnregisterCallback(
        IBusEvent_ScreenModeSet,
        &BMBTScreenModeSet
    );
    EventUnregisterCallback(
        IBusEvent_ScreenModeUpdate,
        &BMBTScreenModeUpdate
    );
    TimerUnregisterScheduledTask(&BMBTTimerHeaderWrite);
    TimerUnregisterScheduledTask(&BMBTTimerMenuWrite);
    TimerUnregisterScheduledTask(&BMBTTimerScrollDisplay);
    memset(&Context, 0, sizeof(BMBTContext_t));
}

/**
 * BMBTMenuRefresh()
 *     Description:
 *         Trigger the scheduled task to rewrite the main area. If the text
 *         fits on the screen, reset the index so it is written again
 *     Params:
 *         BMBTContext_t *context - The BMBT context
 *     Returns:
 *         void
 */
static void BMBTMainAreaRefresh(BMBTContext_t *context)
{
    if (context->mainDisplay.length <= 9) {
        context->mainDisplay.index = 0;
    }
    TimerTriggerScheduledTask(context->displayUpdateTaskId);
}

/**
 * BMBTMenuRefresh()
 *     Description:
 *         Wrapper to send a menu refresh call to the GT that handles the
 *         old and new style UIs as well as the static screen support
 *     Params:
 *         BMBTContext_t *context - The BMBT context
 *     Returns:
 *         void
 */
static void BMBTMenuRefresh(BMBTContext_t *context)
{
    if ((context->menu != BMBT_MENU_DASHBOARD &&
        context->menu != BMBT_MENU_DASHBOARD_FRESH) ||
        context->ibus->gtVersion != IBUS_GT_MKIV_STATIC
    ) {
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    } else {
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_STATIC);
    }
}

static void BMBTSetMainDisplayText(
    BMBTContext_t *context,
    const char *str,
    int8_t timeout,
    uint8_t autoUpdate
) {
    strncpy(context->mainDisplay.text, str, UTILS_DISPLAY_TEXT_SIZE - 1);
    context->mainDisplay.length = strlen(context->mainDisplay.text);
    context->mainDisplay.index = 0;
    if (autoUpdate == 1) {
        TimerTriggerScheduledTask(context->displayUpdateTaskId);
    }
    context->mainDisplay.timeout = timeout;
}

/**
 * BMBTTriggerWriteHeader()
 *     Description:
 *         Trigger the counter that fires off our header field writing
 *         timer. If the counter has already been triggered, do nothing.
 *     Params:
 *         BMBTContext_t *context - The context
 *     Returns:
 *         void
 */
static void BMBTTriggerWriteHeader(BMBTContext_t *context)
{
    if (context->timerHeaderIntervals == BMBT_MENU_HEADER_TIMER_OFF) {
        TimerResetScheduledTask(context->headerWriteTaskId);
        context->timerHeaderIntervals = 0;
    }
}

/**
 * BMBTTriggerWriteMenu()
 *     Description:
 *         Trigger the counter that fires off our menu writing
 *         timer. If the counter has already been triggered, do nothing.
 *     Params:
 *         BMBTContext_t *context - The context
 *     Returns:
 *         void
 */
static void BMBTTriggerWriteMenu(BMBTContext_t *context)
{
    // If we can refresh the last menu back onto the screen,
    // do so immediately. Otherwise, trigger the menu write timer
    if (context->menu == BMBT_MENU_NONE ||
        context->menu == BMBT_MENU_DASHBOARD_FRESH ||
        context->ibus->gtVersion < IBUS_GT_MKIII_NEW_UI ||
        context->status.radType == IBUS_RADIO_TYPE_C43
    ) {
        if (context->timerMenuIntervals == BMBT_MENU_HEADER_TIMER_OFF) {
            TimerResetScheduledTask(context->menuWriteTaskId);
            context->timerMenuIntervals = 0;
        }
    } else {
        BMBTMenuRefresh(context);
    }
}

/**
 * BMBTHeaderWriteDeviceName()
 *     Description:
 *         Wrapper to extend the length of the device field to 20 characters
 *         with space padding if we are writing to the old style UI.
 *     Params:
 *         BMBTContext_t *context - The context
 *         char *text - The text to write
 *     Returns:
 *         void
 */
static void BMBTHeaderWriteDeviceName(BMBTContext_t *context, char *text)
{
    if (context->ibus->gtVersion < IBUS_GT_MKIII_NEW_UI) {
        char cleanName[21];
        strncpy(cleanName, text, 20);
        uint8_t nameLength = strlen(cleanName);
        while (nameLength < 20) {
            cleanName[nameLength++] = 0x20;
        }
        IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_DEV_NAME, cleanName);
    } else {
        char cleanName[12];
        strncpy(cleanName, text, 11);
        IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_DEV_NAME, cleanName);
    }
}

/**
 * BMBTGTWriteIndex()
 *     Description:
 *         Wrapper to automatically push the nav type into the IBus Library
 *         Command so that we can save verbosity in these calls
 *     Params:
 *         BMBTContext_t *context - The context
 *         uint8_t index - The index to write to
 *         char *text - The text to write
 *     Returns:
 *         void
 */
static void BMBTGTWriteIndex(BMBTContext_t *context, uint8_t index, char *text)
{
    context->status.navIndexType = IBUS_CMD_GT_WRITE_INDEX_TMC;
    IBusCommandGTWriteIndexTMC(context->ibus, index, text);
}

/**
 * BMBTGTWriteTitle()
 *     Description:
 *         Wrapper to automatically account for the nav type when
 *         writing the title area
 *     Params:
 *         BMBTContext_t *context - The context
 *         char *text - The text to write
 *     Returns:
 *         void
 */
static void BMBTGTWriteTitle(BMBTContext_t *context, char *text)
{
    if (context->ibus->gtVersion < IBUS_GT_MKIII_NEW_UI) {
        IBusCommandGTWriteTitleArea(context->ibus, text);
    } else {
        IBusCommandGTWriteTitleIndex(context->ibus, text);
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
    }
}

static void BMBTHeaderWrite(BMBTContext_t *context)
{
    if (ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) == CONFIG_SETTING_OFF ||
        context->bt->playbackStatus == BC127_AVRCP_STATUS_PAUSED
    ) {
        BMBTGTWriteTitle(context, "Bluetooth");
    } else {
        BMBTMainAreaRefresh(context);
    }
    if (context->bt->activeDevice.deviceId != 0) {
        char name[33];
        UtilsRemoveNonAscii(name, context->bt->activeDevice.deviceName);
        BMBTHeaderWriteDeviceName(context, name);
    } else {
        BMBTHeaderWriteDeviceName(context, "No Device");
    }
    if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PAUSED) {
        IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_PB_STAT, "||");
    } else {
        IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_PB_STAT, "> ");
    }
    if (ConfigGetVehicleType() == IBUS_VEHICLE_TYPE_E38_E39_E53) {
        char oilTemp[4];
        snprintf(oilTemp, 4, "%d", context->ibus->oilTemperature);
        oilTemp[3] = '\0';
        //IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_GAIN, oilTemp);
    }
    IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_BT, "BT  ");
    IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
}

static void BMBTMenuMain(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Main Menu");
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_DASHBOARD, "Dashboard");
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_DEVICE_SELECTION, "Select Device");
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_SETTINGS, "Settings");
    uint8_t index = 3;
    while (index < context->writtenIndices) {
        BMBTGTWriteIndex(context, index, " ");
        index++;
    }
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, " ");
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = 3;
    context->menu = BMBT_MENU_MAIN;
}

static void BMBTMenuDashboardUpdate(BMBTContext_t *context, char *f1, char *f2, char *f3)
{
    if (strlen(f1) == 0) {
        strncpy(f1, " ", 1);
    }
    if (strlen(f2) == 0) {
        strncpy(f2, " ", 1);
    }
    if (strlen(f3) == 0) {
        strncpy(f3, " ", 1);
    }
    if (context->ibus->gtVersion == IBUS_GT_MKIV_STATIC) {
        IBusCommandGTWriteIndexStatic(context->ibus, 1, f1);
        IBusCommandGTWriteIndexStatic(context->ibus, 2, f2);
        IBusCommandGTWriteIndexStatic(context->ibus, 3, f3);
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_STATIC);
    } else {
        IBusCommandGTWriteIndex(context->ibus, 0, f1);
        IBusCommandGTWriteIndex(context->ibus, 1, f2);
        IBusCommandGTWriteIndex(context->ibus, 2, f3);
        context->status.navIndexType = IBUS_CMD_GT_WRITE_INDEX;
        uint8_t index = 3;
        while (index < context->writtenIndices) {
            BMBTGTWriteIndex(context, index, " ");
            index++;
        }
        context->writtenIndices = 3;
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    }
}

static void BMBTMenuDashboard(BMBTContext_t *context)
{
    char title[BC127_METADATA_FIELD_SIZE];
    char artist[BC127_METADATA_FIELD_SIZE];
    char album[BC127_METADATA_FIELD_SIZE];
    memset(title, 0, BC127_METADATA_FIELD_SIZE);
    memset(artist, 0, BC127_METADATA_FIELD_SIZE);
    memset(album, 0, BC127_METADATA_FIELD_SIZE);
    UtilsRemoveNonAscii(title, context->bt->title);
    UtilsRemoveNonAscii(artist, context->bt->artist);
    UtilsRemoveNonAscii(album, context->bt->album);
    if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PAUSED) {
        if (strlen(title) == 0) {
            strncpy(title, "- Not Playing -", 16);
            strncpy(artist, " ", 2);
            strncpy(album, " ", 2);
        }
    } else {
        if (strlen(title) == 0) {
            strncpy(title, "Unknown Title", 14);
        }
        if (strlen(artist) == 0) {
            strncpy(artist, "Unknown Artist", 15);
        }
        if (strlen(album) == 0) {
            strncpy(album, "Unknown Album", 14);
        }
    }
    BMBTMenuDashboardUpdate(context, title, artist, album);
    context->menu = BMBT_MENU_DASHBOARD;
}

static void BMBTMenuDeviceSelection(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Device Selection");
    uint8_t idx;
    uint8_t screenIdx = 2;
    if (context->bt->discoverable == BC127_STATE_ON) {
        BMBTGTWriteIndex(context, BMBT_MENU_IDX_PAIRING_MODE, "Pairing: On");
    } else {
        BMBTGTWriteIndex(context, BMBT_MENU_IDX_PAIRING_MODE, "Pairing: Off");
    }
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_CLEAR_PAIRING, "Clear Pairings");
    BC127PairedDevice_t *dev = 0;
    for (idx = 0; idx < context->bt->pairedDevicesCount; idx++) {
        dev = &context->bt->pairedDevices[idx];
        if (dev != 0) {
            char name[33];
            UtilsRemoveNonAscii(name, dev->deviceName);
            char cleanText[12];
            strncpy(cleanText, name, 11);
            cleanText[11] = '\0';
            // Add a space and asterisks to the end of the device name
            // if it's the currently selected device
            if (strcmp(dev->macId, context->bt->activeDevice.macId) == 0) {
                uint8_t startIdx = strlen(cleanText);
                if (startIdx > 9) {
                    startIdx = 9;
                }
                cleanText[startIdx++] = 0x20;
                cleanText[startIdx++] = 0x2A;
            }
            BMBTGTWriteIndex(context, screenIdx, cleanText);
            screenIdx++;
        }
    }
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    uint8_t index = screenIdx;
    while (index < context->writtenIndices) {
        BMBTGTWriteIndex(context, index, " ");
        index++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = screenIdx;
    context->menu = BMBT_MENU_DEVICE_SELECTION;
}

static void BMBTMenuSettings(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Settings");
    uint8_t idx;
    for (idx = 0; idx < sizeof(menuSettings); idx++) {
        BMBTGTWriteIndex(
            context,
            menuSettings[idx],
            menuSettingsLabels[idx]
        );
    }
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    while (idx < context->writtenIndices) {
        BMBTGTWriteIndex(context, idx, " ");
        idx++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = idx;
    context->menu = BMBT_MENU_SETTINGS;
}

static void BMBTMenuSettingsAudio(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Settings -> Audio");
    unsigned char currentVolume = ConfigGetSetting(CONFIG_SETTING_DAC_VOL);
    char volText[15];
    if (currentVolume > 0x30) {
        unsigned char gain = (currentVolume - 0x30) / 2;
        snprintf(volText, 14, "Volume: -%ddB", gain);
    } else if (currentVolume == 0) {
        snprintf(volText, 14, "Volume: +24dB");
    } else if (currentVolume == 0x30) {
        snprintf(volText, 14, "Volume: 0dB");
    } else {
        unsigned char gain = (0x30 - currentVolume) / 2;
        snprintf(volText, 14, "Volume: +%ddB", gain);
    }
    volText[14] = '\0';
    BMBTGTWriteIndex(
        context,
        BMBT_MENU_IDX_SETTINGS_AUDIO_DAC_GAIN,
        volText
    );
    if (ConfigGetSetting(CONFIG_SETTING_USE_SPDIF_INPUT) == CONFIG_SETTING_ON) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_AUDIO_DSP_INPUT,
            "DSP: Digital"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_AUDIO_DSP_INPUT,
            "DSP: Analog"
        );
    }
    if (ConfigGetSetting(CONFIG_SETTING_AUTOPLAY) == CONFIG_SETTING_OFF) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_AUDIO_AUTOPLAY,
            "Autoplay: Off"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_AUDIO_AUTOPLAY,
            "Autoplay: On"
        );
    }
    uint8_t idx = 3;
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    while (idx < context->writtenIndices) {
        BMBTGTWriteIndex(context, idx, " ");
        idx++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = idx;
    context->menu = BMBT_MENU_SETTINGS_AUDIO;
}

static void BMBTMenuSettingsComfort(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Settings -> Comfort");
    unsigned char vehicleType = ConfigGetVehicleType();
    if (vehicleType == IBUS_VEHICLE_TYPE_E38_E39_E53) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_COMFORT_VEHICLE_TYPE,
            "Car: E38/E39/E53"
        );
    } else if (vehicleType == IBUS_VEHICLE_TYPE_E46_Z4) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_COMFORT_VEHICLE_TYPE,
            "Car: E46/Z4"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_COMFORT_VEHICLE_TYPE,
            "Car: Unset"
        );
    }
    unsigned char blinkCount = ConfigGetSetting(CONFIG_SETTING_COMFORT_BLINKERS);
    if (blinkCount == 0) {
        blinkCount = 1;
    }
    char blinkerText[13];
    snprintf(blinkerText, 12, "Blinkers: %d", blinkCount);
    blinkerText[12] = '\0';
    BMBTGTWriteIndex(
        context,
        BMBT_MENU_IDX_SETTINGS_COMFORT_BLINKERS,
        blinkerText
    );
    if (ConfigGetSetting(CONFIG_SETTING_COMFORT_LOCKS) == CONFIG_SETTING_ON) {
        BMBTGTWriteIndex(
            context, BMBT_MENU_IDX_SETTINGS_COMFORT_LOCKS,
            "Auto Locks: On"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_COMFORT_LOCKS,
            "Auto Locks: Off"
        );
    }
    uint8_t idx = 3;
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    while (idx < context->writtenIndices) {
        BMBTGTWriteIndex(context, idx, " ");
        idx++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = idx;
    context->menu = BMBT_MENU_SETTINGS_COMFORT;
}

static void BMBTMenuSettingsCalling(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Settings -> Calling");
    if (ConfigGetSetting(CONFIG_SETTING_HFP) == 0x00) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_HFP,
            "Handsfree: On"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_HFP,
            "Handsfree: On"
        );
    }
    if (ConfigGetSetting(CONFIG_SETTING_TCU_MODE) == CONFIG_SETTING_OFF) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_TCU_MODE,
            "TCU: Always"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_TCU_MODE,
            "TCU: Radio/AUX"
        );
    }
    unsigned char micGain = ConfigGetSetting(CONFIG_SETTING_MIC_GAIN);
    if (micGain > 21) {
        micGain = 0;
    }
    char micGainText[16];
    snprintf(micGainText, 15, "Mic Gain: %idB", (int8_t)BC127CVCGainTable[micGain]);
    micGainText[15] = '\0';
    BMBTGTWriteIndex(
        context,
        BMBT_MENU_IDX_SETTINGS_CALLING_MIC_GAIN,
        micGainText
    );
    if (ConfigGetSetting(CONFIG_SETTING_MIC_BIAS_ADDRESS) == CONFIG_SETTING_OFF) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_MIC_BIAS,
            "Mic Bias: Off"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_MIC_BIAS,
            "Mic Bias: On"
        );
    }
    uint8_t idx = 5;
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    while (idx < context->writtenIndices) {
        BMBTGTWriteIndex(context, idx, " ");
        idx++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = idx;
    context->menu = BMBT_MENU_SETTINGS_CALLING;
}

static void BMBTMenuSettingsUI(BMBTContext_t *context)
{
    IBusCommandGTWriteIndexTitle(context->ibus, "Settings -> UI");
    unsigned char metadataMode = ConfigGetSetting(CONFIG_SETTING_METADATA_MODE);
    if (metadataMode == BMBT_METADATA_MODE_PARTY) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_UI_METADATA_MODE,
            "Metadata: Party"
        );
    } else if (metadataMode == BMBT_METADATA_MODE_CHUNK) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_UI_METADATA_MODE,
            "Metadata: Chunk"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_UI_METADATA_MODE,
            "Metadata: Off"
        );
    }
    if (ConfigGetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU) == CONFIG_SETTING_OFF) {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
            "Menu: Main"
        );
    } else {
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
            "Menu: Dashboard"
        );
    }
    //if (ConfigGetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU) == CONFIG_SETTING_OFF) {
    //    BMBTGTWriteIndex(
    //        context,
    //        BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
    //        "Temps: Off"
    //    );
    //} else {
    //    BMBTGTWriteIndex(
    //        context,
    //        BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
    //        "Temps: Coolant"
    //    );
    //    BMBTGTWriteIndex(
    //        context,
    //        BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
    //        "Temps: Outdoor"
    //    );
    //    BMBTGTWriteIndex(
    //        context,
    //        BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU,
    //        "Temps: Both"
    //    );
    //}
    uint8_t idx = 2;
    BMBTGTWriteIndex(context, BMBT_MENU_IDX_BACK, "Back");
    while (idx < context->writtenIndices) {
        BMBTGTWriteIndex(context, idx, " ");
        idx++;
    }
    IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    context->writtenIndices = idx;
    context->menu = BMBT_MENU_SETTINGS_UI;
}

static void BMBTSettingsUpdateAudio(BMBTContext_t *context, uint8_t selectedIdx)
{
    if (selectedIdx == BMBT_MENU_IDX_SETTINGS_AUDIO_DAC_GAIN) {
        unsigned char currentVolume = ConfigGetSetting(CONFIG_SETTING_DAC_VOL);
        currentVolume = currentVolume + 2;
        if (currentVolume > 96) {
            currentVolume = 0;
        }
        ConfigSetSetting(CONFIG_SETTING_DAC_VOL, currentVolume);
        char volText[15];
        if (currentVolume > 0x30) {
            unsigned char gain = (currentVolume - 0x30) / 2;
            snprintf(volText, 14, "Volume: -%ddB", gain);
        } else if (currentVolume == 0) {
            snprintf(volText, 14, "Volume: +24dB");
        } else if (currentVolume == 0x30) {
            snprintf(volText, 14, "Volume: 0dB");
        } else {
            unsigned char gain = (0x30 - currentVolume) / 2;
            snprintf(volText, 14, "Volume: +%ddB", gain);
        }
        volText[14] = '\0';
        BMBTGTWriteIndex(context, selectedIdx, volText);
        PCM51XXSetVolume(currentVolume);
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_AUDIO_DSP_INPUT) {
        if (ConfigGetSetting(CONFIG_SETTING_USE_SPDIF_INPUT) == CONFIG_SETTING_ON) {
            ConfigSetSetting(CONFIG_SETTING_USE_SPDIF_INPUT, CONFIG_SETTING_OFF);
            IBusCommandDSPSetMode(context->ibus, IBUS_DSP_MODE_INPUT_RADIO);
            BMBTGTWriteIndex(context, selectedIdx, "DSP: Analog");
        } else {
            ConfigSetSetting(CONFIG_SETTING_USE_SPDIF_INPUT, CONFIG_SETTING_ON);
            IBusCommandDSPSetMode(context->ibus, IBUS_DSP_MODE_INPUT_SPDIF);
            BMBTGTWriteIndex(context, selectedIdx, "DSP: Digital");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_AUDIO_AUTOPLAY) {
        if (ConfigGetSetting(CONFIG_SETTING_AUTOPLAY) == CONFIG_SETTING_OFF) {
            ConfigSetSetting(CONFIG_SETTING_AUTOPLAY, CONFIG_SETTING_ON);
            BMBTGTWriteIndex(context, selectedIdx, "Autoplay: On");
        } else {
            ConfigSetSetting(CONFIG_SETTING_AUTOPLAY, CONFIG_SETTING_OFF);
            BMBTGTWriteIndex(context, selectedIdx, "Autoplay: Off");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
        BMBTMenuSettings(context);
    }
    if (selectedIdx != BMBT_MENU_IDX_BACK) {
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    }
}

static void BMBTSettingsUpdateComfort(BMBTContext_t *context, uint8_t selectedIdx)
{
    if (selectedIdx == BMBT_MENU_IDX_SETTINGS_COMFORT_VEHICLE_TYPE) {
        unsigned char value = ConfigGetVehicleType();
        if (value == 0 || value == 0xFF || value == IBUS_VEHICLE_TYPE_E46_Z4) {
            ConfigSetVehicleType(IBUS_VEHICLE_TYPE_E38_E39_E53);
            BMBTGTWriteIndex(context, selectedIdx, "Car: E38/E39/E53");
        } else {
            ConfigSetVehicleType(IBUS_VEHICLE_TYPE_E46_Z4);
            BMBTGTWriteIndex(context, selectedIdx, "Car: E46/Z4");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_COMFORT_BLINKERS) {
        unsigned char value = ConfigGetSetting(CONFIG_SETTING_COMFORT_BLINKERS);
        if (value == 0) {
            value = 1;
        } else if (value == 8) {
            value = 0;
        }
        value = value + 1;
        ConfigSetSetting(CONFIG_SETTING_COMFORT_BLINKERS, value);
        char blinkerText[13];
        snprintf(blinkerText, 12, "Blinkers: %d", value);
        blinkerText[12] = '\0';
        BMBTGTWriteIndex(context, selectedIdx, blinkerText);
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_COMFORT_LOCKS) {
        unsigned char value = ConfigGetSetting(CONFIG_SETTING_COMFORT_LOCKS);
        if (value == CONFIG_SETTING_OFF) {
            ConfigSetSetting(CONFIG_SETTING_COMFORT_LOCKS, CONFIG_SETTING_ON);
            BMBTGTWriteIndex(context, selectedIdx, "Auto Locks: On");
        } else {
            ConfigSetSetting(CONFIG_SETTING_COMFORT_LOCKS, CONFIG_SETTING_OFF);
            BMBTGTWriteIndex(context, selectedIdx, "Auto Locks: Off");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
        BMBTMenuSettings(context);
    }
    if (selectedIdx != BMBT_MENU_IDX_BACK) {
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    }
}

static void BMBTSettingsUpdateCalling(BMBTContext_t *context, uint8_t selectedIdx)
{
    if (selectedIdx == BMBT_MENU_IDX_SETTINGS_CALLING_HFP) {
        unsigned char value = ConfigGetSetting(CONFIG_SETTING_HFP);
        if (value == 0x00) {
            ConfigSetSetting(CONFIG_SETTING_HFP, 0x01);
            BMBTGTWriteIndex(context, selectedIdx, "Handsfree: On");
            BC127CommandSetProfiles(context->bt, 1, 1, 0, 1);
        } else {
            BC127CommandSetProfiles(context->bt, 1, 1, 0, 0);
            BMBTGTWriteIndex(context, selectedIdx, "Handsfree: Off");
            ConfigSetSetting(CONFIG_SETTING_HFP, 0x00);
        }
        BC127CommandReset(context->bt);
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_CALLING_TCU_MODE) {
        if (ConfigGetSetting(CONFIG_SETTING_TCU_MODE) == CONFIG_SETTING_OFF) {
            ConfigSetSetting(CONFIG_SETTING_TCU_MODE, CONFIG_SETTING_ON);
            BMBTGTWriteIndex(context, selectedIdx, "TCU: Radio/AUX");
        } else {
            ConfigSetSetting(CONFIG_SETTING_TCU_MODE, CONFIG_SETTING_OFF);
            BMBTGTWriteIndex(context, selectedIdx, "TCU: Always");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_CALLING_MIC_GAIN) {
        unsigned char micGain = ConfigGetSetting(CONFIG_SETTING_MIC_GAIN);
        micGain = micGain + 1;
        if (micGain > 21) {
            micGain = 0;
        }
        ConfigSetSetting(CONFIG_SETTING_MIC_GAIN, micGain);
        BC127CommandSetMicGain(context->bt, micGain);
        char micGainText[16];
        snprintf(micGainText, 15, "Mic Gain: %idB", (int8_t)BC127CVCGainTable[micGain]);
        micGainText[15] = '\0';
        BMBTGTWriteIndex(
            context,
            BMBT_MENU_IDX_SETTINGS_CALLING_MIC_GAIN,
            micGainText
        );
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_CALLING_MIC_BIAS) {
        if (ConfigGetSetting(CONFIG_SETTING_MIC_BIAS_ADDRESS) == CONFIG_SETTING_OFF) {
            BC127CommandSetAudioAnalog(context->bt, "11", "15", "1", "OFF");
            ConfigSetSetting(CONFIG_SETTING_MIC_BIAS_ADDRESS, CONFIG_SETTING_ON);
            BMBTGTWriteIndex(context, selectedIdx, "Mic Bias: On");
        } else {
            BC127CommandSetAudioAnalog(context->bt, "11", "15", "0", "OFF");
            ConfigSetSetting(CONFIG_SETTING_MIC_BIAS_ADDRESS, CONFIG_SETTING_OFF);
            BMBTGTWriteIndex(context, selectedIdx, "Mic Bias: Off");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
        BMBTMenuSettings(context);
    }
    if (selectedIdx != BMBT_MENU_IDX_BACK) {
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    }
}

static void BMBTSettingsUpdateUI(BMBTContext_t *context, uint8_t selectedIdx)
{
    if (selectedIdx == BMBT_MENU_IDX_SETTINGS_UI_METADATA_MODE) {
        unsigned char value = ConfigGetSetting(
            CONFIG_SETTING_METADATA_MODE
        );
        if (value == 0x00) {
            value = BMBT_METADATA_MODE_PARTY;
            BMBTGTWriteIndex(context, selectedIdx, "Metadata: Party");
        } else if (value == 0x01) {
            value = BMBT_METADATA_MODE_CHUNK;
            BMBTGTWriteIndex(context, selectedIdx, "Metadata: Chunk");
        } else {
            value = BMBT_METADATA_MODE_OFF;
            BMBTGTWriteIndex(context, selectedIdx, "Metadata: Off");
        }
        ConfigSetSetting(CONFIG_SETTING_METADATA_MODE, value);
        if (value != BMBT_METADATA_MODE_OFF &&
            strlen(context->bt->title) > 0 &&
            context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING
        ) {
            char text[UTILS_DISPLAY_TEXT_SIZE];
            snprintf(
                text,
                UTILS_DISPLAY_TEXT_SIZE,
                "%s - %s - %s",
                context->bt->title,
                context->bt->artist,
                context->bt->album
            );
            char cleanText[UTILS_DISPLAY_TEXT_SIZE];
            UtilsRemoveNonAscii(cleanText, text);
            BMBTSetMainDisplayText(context, cleanText, 0, 0);
        } else if (value == BMBT_METADATA_MODE_OFF) {
            IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
            BMBTGTWriteTitle(context, "Bluetooth");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU) {
        if (ConfigGetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU) == 0x00) {
            ConfigSetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU, 0x01);
            BMBTGTWriteIndex(context, selectedIdx, "Menu: Dashboard");
        } else {
            ConfigSetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU, 0x00);
            BMBTGTWriteIndex(context, selectedIdx, "Menu: Main");
        }
    } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
        BMBTMenuSettings(context);
    }
    if (selectedIdx != BMBT_MENU_IDX_BACK) {
        IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
    }
}

/**
 * BMBTBC127DeviceConnected()
 *     Description:
 *         Handle screen updates when a device connects
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *tmp - The data from the event
 *     Returns:
 *         void
 */
void BMBTBC127DeviceConnected(void *ctx, unsigned char *data)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.displayMode == BMBT_DISPLAY_ON) {
        char name[33];
        UtilsRemoveNonAscii(name, context->bt->activeDevice.deviceName);
        BMBTHeaderWriteDeviceName(context, name);
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
        if (context->menu == BMBT_MENU_DEVICE_SELECTION) {
            BMBTMenuDeviceSelection(context);
        }
    }
}

/**
 * BMBTBC127DeviceDisconnected()
 *     Description:
 *         Handle screen updates when a device disconnects
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *tmp - The data from the event
 *     Returns:
 *         void
 */
void BMBTBC127DeviceDisconnected(void *ctx, unsigned char *data)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.displayMode == BMBT_DISPLAY_ON) {
        BMBTHeaderWriteDeviceName(context, "No Device");
        IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_PB_STAT, "||");
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
        if (context->menu == BMBT_MENU_DEVICE_SELECTION) {
            BMBTMenuDeviceSelection(context);
        }
    }
}

/**
 * BMBTBC127Metadata()
 *     Description:
 *         Handle metadata updates from the BC127
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *tmp - The data from the event
 *     Returns:
 *         void
 */
void BMBTBC127Metadata(void *ctx, unsigned char *data)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.playerMode == BMBT_MODE_ACTIVE &&
        context->status.displayMode == BMBT_DISPLAY_ON
    ) {
        if (ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) != CONFIG_SETTING_OFF) {
            char text[UTILS_DISPLAY_TEXT_SIZE];
            snprintf(
                text,
                UTILS_DISPLAY_TEXT_SIZE,
                "%s - %s - %s",
                context->bt->title,
                context->bt->artist,
                context->bt->album
            );
            char cleanText[UTILS_DISPLAY_TEXT_SIZE];
            UtilsRemoveNonAscii(cleanText, text);
            BMBTSetMainDisplayText(context, cleanText, 0, 1);
        }
        if (context->menu == BMBT_MENU_DASHBOARD ||
            context->menu == BMBT_MENU_DASHBOARD_FRESH
        ) {
            BMBTMenuDashboard(context);
        }
    }
}

/**
 * BMBTBC127PlaybackStatus()
 *     Description:
 *         Handle the BC127 playback state changes
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *tmp - The data from the event
 *     Returns:
 *         void
 */
void BMBTBC127PlaybackStatus(void *ctx, unsigned char *tmp)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.displayMode == BMBT_DISPLAY_ON) {
        if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PAUSED) {
            BMBTSetMainDisplayText(context, "Bluetooth", 0, 1);
            IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_PB_STAT, "||");
        } else {
            BMBTMainAreaRefresh(context);
            IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_PB_STAT, "> ");
        }
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
    }
}

/**
 * BMBTBC127Ready()
 *     Description:
 *         Handle the BC127 rebooting gracefully
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *tmp - The data from the event
 *     Returns:
 *         void
 */
void BMBTBC127Ready(void *ctx, unsigned char *tmp)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    BMBTHeaderWriteDeviceName(context, "No Device");
    if (context->status.displayMode == BMBT_DISPLAY_ON) {
        IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
    }
}

/**
 * BMBTIBusBMBTButtonPress()
 *     Description:
 *         Handle button presses on the BoardMonitor
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *pkt - A pointer to the data packet
 *     Returns:
 *         void
 */
void BMBTIBusBMBTButtonPress(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.playerMode == BMBT_MODE_ACTIVE) {
        if (pkt[4] == IBUS_DEVICE_BMBT_Button_PlayPause ||
            pkt[4] == IBUS_DEVICE_BMBT_Button_Num1
        ) {
            if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
                BC127CommandPause(context->bt);
            } else {
                BC127CommandPlay(context->bt);
            }
        }
        if (pkt[4] == IBUS_DEVICE_BMBT_Button_Knob) {
            if (context->status.displayMode == BMBT_DISPLAY_ON &&
                context->menu == BMBT_MENU_DASHBOARD &&
                context->ibus->gtVersion == IBUS_GT_MKIV_STATIC
            ) {
                BMBTMenuMain(context);
            }
        }
        if (pkt[4] == IBUS_DEVICE_BMBT_Button_Display) {
            if (context->status.playerMode == BMBT_MODE_ACTIVE &&
                context->status.displayMode == BMBT_DISPLAY_OFF
            ) {
                context->status.displayMode = BMBT_DISPLAY_ON;
                if (context->menu != BMBT_MENU_DASHBOARD_FRESH) {
                    context->menu = BMBT_MENU_NONE;
                }
                context->status.navState = BMBT_NAV_STATE_OFF;
            }
        }
        if (pkt[4] == IBUS_DEVICE_BMBT_Button_Mode) {
            context->status.playerMode = BMBT_MODE_INACTIVE;
        }
        // Handle the SEL and Info buttons gracefully
        if (pkt[3] == IBUS_CMD_BMBT_BUTTON0 && pkt[1] == 0x05) {
            if (pkt[5] == IBUS_DEVICE_BMBT_Button_Info) {
                context->status.displayMode = BMBT_DISPLAY_INFO;
            } else if (pkt[5] == IBUS_DEVICE_BMBT_Button_SEL) {
                context->status.displayMode = BMBT_DISPLAY_TONE_SEL;
            }
        }
    }
    // Handle calls at any time
    if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
        if (pkt[4] == IBUS_DEVICE_BMBT_Button_TEL_Release) {
            if (context->bt->callStatus == BC127_CALL_ACTIVE) {
                BC127CommandCallEnd(context->bt);
            } else if (context->bt->callStatus == BC127_CALL_INCOMING) {
                BC127CommandCallAnswer(context->bt);
            } else if (context->bt->callStatus == BC127_CALL_OUTGOING) {
                BC127CommandCallEnd(context->bt);
            }
        } else if (context->bt->callStatus == BC127_CALL_INACTIVE &&
                   pkt[4] == IBUS_DEVICE_BMBT_Button_TEL_Hold
        ) {
            BC127CommandToggleVR(context->bt);
        }
    }
}

/**
 * BMBTIBusCDChangerStatus()
 *     Description:
 *         Track the CD Changer state so we know if we can write to the
 *         screen or not, as well as handle playback state
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *pkt - A pointer to the data packet
 *     Returns:
 *         void
 */
void BMBTIBusCDChangerStatus(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    unsigned char requestedCommand = pkt[4];
    if (requestedCommand == IBUS_CDC_CMD_STOP_PLAYING) {
        // Stop Playing
        if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
            BC127CommandPause(context->bt);
        }
        context->menu = BMBT_MENU_NONE;
        context->status.playerMode = BMBT_MODE_INACTIVE;
        context->status.displayMode = BMBT_DISPLAY_OFF;
        BMBTSetMainDisplayText(context, "Bluetooth", 0, 0);
        IBusCommandRADEnableMenu(context->ibus);
    } else if (requestedCommand == IBUS_CDC_CMD_START_PLAYING ||
        (context->ibus->cdChangerFunction == IBUS_CDC_FUNC_PLAYING &&
         context->status.playerMode == BMBT_MODE_INACTIVE)
    ) {
        BMBTSetMainDisplayText(context, "Bluetooth", 0, 0);
        if (ConfigGetSetting(CONFIG_SETTING_AUTOPLAY) == CONFIG_SETTING_ON) {
            BC127CommandPlay(context->bt);
        } else if (context->bt->playbackStatus == BC127_AVRCP_STATUS_PLAYING) {
            BC127CommandPause(context->bt);
        }
        IBusCommandRADDisableMenu(context->ibus);
        context->status.playerMode = BMBT_MODE_ACTIVE;
        BMBTTriggerWriteHeader(context);
        BMBTTriggerWriteMenu(context);
    }
}

/**
 * BMBTIBusGTChangeUIRequest()
 *     Description:
 *         Display the Telephone UI when the GT requests it
 *     Params:
 *         void *context - A void pointer to the BMBTContext_t struct
 *         unsigned char *pkt - A pointer to the data packet
 *     Returns:
 *         void
 */
void BMBTIBusGTChangeUIRequest(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (pkt[4] == 0x02 && pkt[5] == 0x0C) {
        if (ConfigGetSetting(CONFIG_SETTING_HFP) == CONFIG_SETTING_ON) {
            IBusCommandTELSetGTDisplayMenu(context->ibus);
        }
    }
}

void BMBTIBusMenuSelect(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    uint8_t selectedIdx = (uint8_t) pkt[6];
    if (selectedIdx < 10 && context->status.displayMode == BMBT_DISPLAY_ON) {
        if (context->menu == BMBT_MENU_MAIN) {
            if (selectedIdx == BMBT_MENU_IDX_DASHBOARD) {
                BMBTMenuDashboard(context);
            } else if (selectedIdx == BMBT_MENU_IDX_DEVICE_SELECTION) {
                BMBTMenuDeviceSelection(context);
            } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS) {
                BMBTMenuSettings(context);
            }
        } else if (context->menu == BMBT_MENU_DASHBOARD) {
            BMBTMenuMain(context);
        } else if (context->menu == BMBT_MENU_DEVICE_SELECTION) {
            if (selectedIdx == BMBT_MENU_IDX_PAIRING_MODE) {
                uint8_t state;
                if (context->bt->discoverable == BC127_STATE_ON) {
                    BMBTGTWriteIndex(context, BMBT_MENU_IDX_PAIRING_MODE, "Pairing: Off");
                    state = BC127_STATE_OFF;
                } else {
                    BMBTGTWriteIndex(context, BMBT_MENU_IDX_PAIRING_MODE, "Pairing: On");
                    state = BC127_STATE_ON;
                    if (context->bt->activeDevice.deviceId != 0) {
                        // To pair a new device, we must disconnect the active one
                        EventTriggerCallback(UIEvent_CloseConnection, 0x00);
                    }
                }
                IBusCommandGTUpdate(context->ibus, context->status.navIndexType);
                BC127CommandBtState(context->bt, context->bt->connectable, state);
            } else if (selectedIdx == BMBT_MENU_IDX_CLEAR_PAIRING) {
                BC127CommandUnpair(context->bt);
                BC127ClearPairedDevices(context->bt);
                BMBTMenuDeviceSelection(context);
            } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
                // Back Button
                BMBTMenuMain(context);
            } else {
                uint8_t deviceId = selectedIdx - BMBT_MENU_IDX_FIRST_DEVICE;
                BC127PairedDevice_t *dev = &context->bt->pairedDevices[deviceId];
                if (strcmp(dev->macId, context->bt->activeDevice.macId) != 0 &&
                    dev != 0
                ) {
                    // Trigger device selection event
                    EventTriggerCallback(
                        UIEvent_InitiateConnection,
                        (unsigned char *)&deviceId
                    );
                }
            }
        } else if (context->menu == BMBT_MENU_SETTINGS) {
            if (selectedIdx == BMBT_MENU_IDX_SETTINGS_AUDIO) {
                BMBTMenuSettingsAudio(context);
            } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_COMFORT) {
                BMBTMenuSettingsComfort(context);
            } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_CALLING) {
                BMBTMenuSettingsCalling(context);
            } else if (selectedIdx == BMBT_MENU_IDX_SETTINGS_UI) {
                BMBTMenuSettingsUI(context);
            } else if (selectedIdx == BMBT_MENU_IDX_BACK) {
                BMBTMenuMain(context);
            }
        } else if (context->menu == BMBT_MENU_SETTINGS_AUDIO) {
            BMBTSettingsUpdateAudio(context, selectedIdx);
        } else if (context->menu == BMBT_MENU_SETTINGS_COMFORT) {
            BMBTSettingsUpdateComfort(context, selectedIdx);
        } else if (context->menu == BMBT_MENU_SETTINGS_CALLING) {
            BMBTSettingsUpdateCalling(context, selectedIdx);
        } else if (context->menu == BMBT_MENU_SETTINGS_UI) {
            BMBTSettingsUpdateUI(context, selectedIdx);
        }
    }
}

/**
 * BMBTRADDisplayMenu()
 *     Description:
 *         This callback is triggered when the radio writes the "TONE" or
 *         "SEL" menus. This way, we can stop acting upon UI inputs until the
 *         screen is restored to our menu.
 *     Params:
 *         void *ctx - The context
 *         unsigned char *pkt - The IBus Message received
 *     Returns:
 *         void
 */
void BMBTRADDisplayMenu(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    context->status.displayMode = BMBT_DISPLAY_TONE_SEL;
}

/**
 * BMBTRADUpdateMainArea()
 *     Description:
 *         This callback is triggered when the radio writes the main area of
 *         the screen (Where the mode is usually displayed). In here, we can
 *         register our state as well as overwrite what the radio has written
 *         to the screen so the UI is always usable.
 *     Params:
 *         void *ctx - The context
 *         unsigned char *pkt - The IBus Message received
 *     Returns:
 *         void
 */
void BMBTRADUpdateMainArea(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (pkt[4] == IBUS_C43_TITLE_MODE) {
        context->status.radType = IBUS_RADIO_TYPE_C43;
    }
    if (context->status.playerMode == BMBT_MODE_ACTIVE && pkt[5] != 0x30) {
        uint8_t pktLen = (uint8_t) pkt[1] + 2;
        uint8_t textLen = pktLen - 7;
        char text[textLen];
        uint8_t idx = 0;
        uint8_t strIdx = 0;
        // Copy the text from the packet but avoid any preceding spaces
        while (strIdx < textLen) {
            char byte = pkt[strIdx + 6];
            if (byte > 0x20 || idx > 0) {
                text[idx] = byte;
                idx++;
            }
            strIdx++;
        }
        idx--;
        while (text[idx] == 0x20 || text[idx] == 0x00) {
            text[idx] = '\0';
            idx--;
        }
        text[textLen - 1] = '\0';
        if (UtilsStricmp("NO TAPE", text) == 0 ||
            UtilsStricmp("NO CD", text) == 0
        ) {
            context->status.displayMode = BMBT_DISPLAY_OFF;
        } else {
            // Clear the radio display if we have a C43 in a "new UI" nav
            if (pkt[4] == IBUS_C43_TITLE_MODE &&
                context->ibus->gtVersion >= IBUS_GT_MKIII_NEW_UI
            ) {
                IBusCommandRADClearMenu(context->ibus);
            }
            if (context->status.displayMode == BMBT_DISPLAY_OFF) {
                context->status.displayMode = BMBT_DISPLAY_ON;
            } else {
                if (UtilsStricmp("NO DISC", text) == 0) {
                    BMBTTriggerWriteMenu(context);
                }
            }
            if (ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) == CONFIG_SETTING_OFF ||
                context->bt->playbackStatus == BC127_AVRCP_STATUS_PAUSED
            ) {
                BMBTGTWriteTitle(context, "Bluetooth");
            } else {
                BMBTMainAreaRefresh(context);
            }
            BMBTTriggerWriteHeader(context);
            BMBTTriggerWriteMenu(context);
        }
    }
}

void BMBTIBusValueUpdate(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    char oilTemp[4];
    snprintf(oilTemp, 4, "%d", context->ibus->oilTemperature);
    oilTemp[3] = '\0';
    //IBusCommandGTWriteZone(context->ibus, BMBT_HEADER_GAIN, oilTemp);
    //IBusCommandGTUpdate(context->ibus, IBUS_CMD_GT_WRITE_ZONE);
}

/**
 * BMBTScreenModeUpdate()
 *     Description:
 *         This callback tracks the screen mode that is broadcast by the GT.
 *         We use this to know if we can write to the screen or not.
 *     Params:
 *         void *ctx - The context
 *         unsigned char *pkt - The IBus Message received
 *     Returns:
 *         void
 */
void BMBTScreenModeUpdate(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (pkt[4] == 0x01 || pkt[4] == IBUS_GT_RADIO_SCREEN_OFF) {
        if (context->menu == BMBT_MENU_DASHBOARD) {
            context->menu = BMBT_MENU_DASHBOARD_FRESH;
        } else {
            context->menu = BMBT_MENU_NONE;
        }
        context->status.displayMode = BMBT_DISPLAY_OFF;
    }
    if (pkt[4] == IBUS_GT_MENU_CLEAR &&
        context->status.navState == BMBT_NAV_STATE_OFF
    ) {
        if (context->status.playerMode == BMBT_MODE_ACTIVE) {
            IBusCommandRADDisableMenu(context->ibus);
        }
        context->status.navState = BMBT_NAV_STATE_ON;
    }
    if (pkt[4] == IBUS_GT_MENU_CLEAR &&
        context->status.playerMode == BMBT_MODE_ACTIVE &&
        (context->status.displayMode == BMBT_DISPLAY_ON ||
         context->status.displayMode == BMBT_DISPLAY_INFO)
    ) {
        BMBTTriggerWriteMenu(context);
    } else if (pkt[4] == IBUS_GT_TONE_MENU_OFF ||
         pkt[4] == IBUS_GT_SEL_MENU_OFF
    ) {
         context->status.displayMode = BMBT_DISPLAY_ON;
    }
}

/**
 * BMBTScreenModeSet()
 *     Description:
 *         The GT sends this screen mode post-boot to tell the radio it can
 *         display to the UI. We set the menu to none so that on the next
 *         screen clear, we know to write the UI. Set the nav state to "off"
 *         so we know that we need to disable the radio updates on next
 *         screen clear
 *     Params:
 *         void *ctx - The context
 *         unsigned char *pkt - The IBus Message received
 *     Returns:
 *         void
 */
void BMBTScreenModeSet(void *ctx, unsigned char *pkt)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (pkt[4] == BMBT_NAV_BOOT) {
        context->menu = BMBT_MENU_NONE;
        if (context->status.playerMode == BMBT_MODE_ACTIVE) {
            context->status.navState = BMBT_NAV_STATE_OFF;
        }
    }
}

/**
 * BMBTTimerHeaderWrite()
 *     Description:
 *         Write out the header after a given timeout so the radio does not
 *         fight us when writing to the screen
 *     Params:
 *         void *ctx - The context
 *     Returns:
 *         void
 */
void BMBTTimerHeaderWrite(void *ctx)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.playerMode == BMBT_MODE_ACTIVE &&
        context->status.displayMode == BMBT_DISPLAY_ON
    ) {
        if (context->timerHeaderIntervals != BMBT_MENU_HEADER_TIMER_OFF) {
            uint16_t time = context->timerHeaderIntervals * BMBT_HEADER_TIMER_WRITE_INT;
            if (time >= BMBT_HEADER_TIMER_WRITE_TIMEOUT) {
                BMBTHeaderWrite(context);
                // Increment the intervals so we aren't called again
                context->timerHeaderIntervals = BMBT_MENU_HEADER_TIMER_OFF;
            } else {
                context->timerHeaderIntervals++;
            }
        }
    }
}

/**
 * BMBTTimerMenuWrite()
 *     Description:
 *         Write out the menu after a given timeout so the radio does not
 *         fight us when re-writing the menu to the screen
 *     Params:
 *         void *ctx - The context
 *     Returns:
 *         void
 */
void BMBTTimerMenuWrite(void *ctx)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.playerMode == BMBT_MODE_ACTIVE &&
        context->status.displayMode == BMBT_DISPLAY_ON
    ) {
        if (context->timerMenuIntervals != BMBT_MENU_HEADER_TIMER_OFF) {
            uint16_t time = context->timerMenuIntervals * BMBT_MENU_TIMER_WRITE_INT;
            if (time == BMBT_MENU_TIMER_WRITE_TIMEOUT) {
                switch (context->menu) {
                    case BMBT_MENU_MAIN:
                        BMBTMenuMain(context);
                        break;
                    case BMBT_MENU_DASHBOARD:
                    case BMBT_MENU_DASHBOARD_FRESH:
                        BMBTMenuDashboard(context);
                        break;
                    case BMBT_MENU_DEVICE_SELECTION:
                        BMBTMenuDeviceSelection(context);
                        break;
                    case BMBT_MENU_SETTINGS:
                        BMBTMenuSettings(context);
                        break;
                    case BMBT_MENU_SETTINGS_AUDIO:
                        BMBTMenuSettingsAudio(context);
                        break;
                    case BMBT_MENU_SETTINGS_COMFORT:
                        BMBTMenuSettingsComfort(context);
                        break;
                    case BMBT_MENU_SETTINGS_CALLING:
                        BMBTMenuSettingsCalling(context);
                        break;
                    case BMBT_MENU_SETTINGS_UI:
                        BMBTMenuSettingsUI(context);
                        break;
                    case BMBT_MENU_NONE:
                        if (ConfigGetSetting(CONFIG_SETTING_BMBT_DEFAULT_MENU) == 0x01) {
                            BMBTMenuDashboard(context);
                        } else {
                            BMBTMenuMain(context);
                        }
                        break;
                }
                // Increment the intervals so we aren't called again
                context->timerMenuIntervals = BMBT_MENU_HEADER_TIMER_OFF;
            } else {
                context->timerMenuIntervals++;
            }
        }
    }
}

/**
 * BMBTTimerScrollDisplay()
 *     Description:
 *         Write the scrolling display
 *     Params:
 *         void *ctx - The context
 *     Returns:
 *         void
 */
void BMBTTimerScrollDisplay(void *ctx)
{
    BMBTContext_t *context = (BMBTContext_t *) ctx;
    if (context->status.playerMode == BMBT_MODE_ACTIVE &&
        context->status.displayMode == BMBT_DISPLAY_ON &&
        ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) != CONFIG_SETTING_OFF
    ) {
        // Display the main text if there isn't a timeout set
        if (context->mainDisplay.timeout > 0) {
            context->mainDisplay.timeout--;
        } else {
            if (context->mainDisplay.length > 9) {
                char text[10];
                strncpy(
                    text,
                    &context->mainDisplay.text[context->mainDisplay.index],
                    9
                );
                text[9] = '\0';
                BMBTGTWriteTitle(context, text);
                // Pause at the beginning of the text
                if (context->mainDisplay.index == 0) {
                    context->mainDisplay.timeout = 5;
                }
                uint8_t idxEnd = context->mainDisplay.index + 9;
                if (idxEnd >= context->mainDisplay.length) {
                    // Pause at the end of the text
                    context->mainDisplay.timeout = 2;
                    context->mainDisplay.index = 0;
                } else {
                    if (ConfigGetSetting(CONFIG_SETTING_METADATA_MODE) ==
                        BMBT_METADATA_MODE_CHUNK
                    ) {
                        context->mainDisplay.timeout = 2;
                        context->mainDisplay.index += 9;
                    } else {
                        context->mainDisplay.index++;
                    }
                }
            } else {
                if (context->mainDisplay.index == 0) {
                    BMBTGTWriteTitle(context, context->mainDisplay.text);
                }
                context->mainDisplay.index = 1;
            }
        }
    }
}
