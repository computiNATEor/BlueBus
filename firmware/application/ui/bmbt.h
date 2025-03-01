/*
 * File: bmbt.h
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement the BoardMonitor UI Mode handler
 */
#ifndef BMBT_H
#define BMBT_H
#define _ADDED_C_LIB 1
#include <stdio.h>
#include "../lib/bc127.h"
#include "../lib/config.h"
#include "../lib/event.h"
#include "../lib/ibus.h"
#include "../lib/pcm51xx.h"
#include "../lib/timer.h"
#include "../lib/utils.h"
#define BMBT_DISPLAY_OFF 0x00
#define BMBT_DISPLAY_TONE_SEL 0x01
#define BMBT_DISPLAY_INFO 0x02
#define BMBT_DISPLAY_ON 0x03
#define BMBT_HEADER_BT 1
#define BMBT_HEADER_PB_STAT 2
#define BMBT_HEADER_GAIN 5
#define BMBT_HEADER_DEV_NAME 6
#define BMBT_MENU_NONE 0
#define BMBT_MENU_MAIN 1
#define BMBT_MENU_DASHBOARD 2
#define BMBT_MENU_DEVICE_SELECTION 3
#define BMBT_MENU_SETTINGS 4
#define BMBT_MENU_SETTINGS_AUDIO 5
#define BMBT_MENU_SETTINGS_COMFORT 6
#define BMBT_MENU_SETTINGS_CALLING 7
#define BMBT_MENU_SETTINGS_UI 8
#define BMBT_MENU_DASHBOARD_FRESH 255
#define BMBT_MENU_IDX_BACK 7
#define BMBT_MENU_IDX_DASHBOARD 0
#define BMBT_MENU_IDX_DEVICE_SELECTION 1
#define BMBT_MENU_IDX_SETTINGS 2
#define BMBT_MENU_IDX_SETTINGS_AUDIO 0
#define BMBT_MENU_IDX_SETTINGS_CALLING 1
#define BMBT_MENU_IDX_SETTINGS_COMFORT 2
#define BMBT_MENU_IDX_SETTINGS_UI 3
/* Audio Settings */
#define BMBT_MENU_IDX_SETTINGS_AUDIO_AUTOPLAY 0
#define BMBT_MENU_IDX_SETTINGS_AUDIO_DAC_GAIN 1
#define BMBT_MENU_IDX_SETTINGS_AUDIO_DSP_INPUT 2
/* Call Settings */
#define BMBT_MENU_IDX_SETTINGS_CALLING_HFP 0
#define BMBT_MENU_IDX_SETTINGS_CALLING_MIC_BIAS 1
#define BMBT_MENU_IDX_SETTINGS_CALLING_MIC_GAIN 2
#define BMBT_MENU_IDX_SETTINGS_CALLING_TCU_MODE 3
/* Comfort Settings */
#define BMBT_MENU_IDX_SETTINGS_COMFORT_LOCKS 0
#define BMBT_MENU_IDX_SETTINGS_COMFORT_BLINKERS 1
#define BMBT_MENU_IDX_SETTINGS_COMFORT_VEHICLE_TYPE 2
/* UI Settings */
#define BMBT_MENU_IDX_SETTINGS_UI_DEFAULT_MENU 0
#define BMBT_MENU_IDX_SETTINGS_UI_METADATA_MODE 1
#define BMBT_MENU_IDX_SETTINGS_UI_TEMPS 2

#define BMBT_MENU_IDX_PAIRING_MODE 0
#define BMBT_MENU_IDX_CLEAR_PAIRING 1
#define BMBT_MENU_IDX_FIRST_DEVICE 2
#define BMBT_MENU_WRITE_DELAY 300
#define BMBT_MENU_TIMER_WRITE_INT 100
#define BMBT_MENU_TIMER_WRITE_TIMEOUT 500
#define BMBT_HEADER_TIMER_WRITE_INT 50
#define BMBT_HEADER_TIMER_WRITE_TIMEOUT 100
#define BMBT_MENU_HEADER_TIMER_OFF 255
#define BMBT_METADATA_MODE_OFF 0x00
#define BMBT_METADATA_MODE_PARTY 0x01
#define BMBT_METADATA_MODE_CHUNK 0x02
#define BMBT_MODE_INACTIVE 0
#define BMBT_MODE_ACTIVE 1
#define BMBT_NAV_BOOT 0x10
#define BMBT_NAV_STATE_OFF 0
#define BMBT_NAV_STATE_ON 1
#define BMBT_SCROLL_TEXT_SIZE 255
#define BMBT_SCROLL_TEXT_SPEED 750
#define BMBT_SCROLL_TEXT_TIMER 500
typedef struct BMBTStatus_t {
    uint8_t playerMode: 1;
    uint8_t displayMode: 2;
    uint8_t navState: 1;
    uint8_t radType: 4;
    uint8_t navIndexType;
} BMBTStatus_t;
typedef struct BMBTContext_t {
    BC127_t *bt;
    IBus_t *ibus;
    uint8_t menu;
    BMBTStatus_t status;
    uint8_t writtenIndices;
    uint8_t timerHeaderIntervals;
    uint8_t timerMenuIntervals;
    uint8_t displayUpdateTaskId;
    uint8_t headerWriteTaskId;
    uint8_t menuWriteTaskId;
    UtilsAbstractDisplayValue_t mainDisplay;
} BMBTContext_t;
void BMBTInit(BC127_t *, IBus_t *);
void BMBTDestroy();
void BMBTBC127DeviceConnected(void *, unsigned char *);
void BMBTBC127DeviceDisconnected(void *, unsigned char *);
void BMBTBC127Metadata(void *, unsigned char *);
void BMBTBC127PlaybackStatus(void *, unsigned char *);
void BMBTBC127Ready(void *, unsigned char *);
void BMBTIBusBMBTButtonPress(void *, unsigned char *);
void BMBTIBusCDChangerStatus(void *, unsigned char *);
void BMBTIBusGTChangeUIRequest(void *, unsigned char *);
void BMBTIBusMenuSelect(void *, unsigned char *);
void BMBTRADDisplayMenu(void *, unsigned char *);
void BMBTRADUpdateMainArea(void *, unsigned char *);
void BMBTIBusValueUpdate(void *, unsigned char *);
void BMBTScreenModeUpdate(void *, unsigned char *);
void BMBTScreenModeSet(void *, unsigned char *);
void BMBTTimerHeaderWrite(void *);
void BMBTTimerMenuWrite(void *);
void BMBTTimerScrollDisplay(void *);
#endif /* BMBT_H */
