/***************************************************************************//**
 * @file
 * @brief Callback implementation for ZigbeeMinimal sample application.
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

// This callback file is created for your convenience. You may add application
// code to this file. If you regenerate this file over a previous version, the
// previous version will be overwritten and any code you have added will be
// lost.

#include "app/framework/include/af.h"
#include "app/framework/plugin/network-creator/network-creator.h"
#include "app/framework/plugin/find-and-bind-initiator/find-and-bind-initiator.h"
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "led_effect.h"
#include "button.h"
#include "dbg_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define OFF_CMD_SEND_DELAY          5000

static uint32_t off_cmd_ts = 0;
static bool    exec_reboot = false;
static bool    net_initialized = false;

static void sendOffToBindings(void);

extern uint8_t emDecimalStringWrite(uint32_t value,
                                    bool signedValue,
                                    char* buffer);

static void ota_version_init(void)
{
    char sw_build[32] = {0};

    emDecimalStringWrite(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION >> 24, false, &sw_build[1]);
    int len = strlen(&sw_build[1]);
    sw_build[1 + len] = '.';

    emDecimalStringWrite((EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION >> 16) & 0xFF, false, &sw_build[1 + len + 1]);
    len = strlen(&sw_build[1]);
    sw_build[1 + len] = '.';

    emDecimalStringWrite((EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION) & 0xFFFF, false, &sw_build[1 + len + 1]);
    sw_build[0] = strlen(&sw_build[1]);

    emberAfWriteServerAttribute(emberAfPrimaryEndpointForCurrentNetworkIndex(),
                                ZCL_BASIC_CLUSTER_ID,
                                ZCL_SW_BUILD_ID_ATTRIBUTE_ID, (uint8_t*)sw_build,
                                ZCL_CHAR_STRING_ATTRIBUTE_TYPE);

    DBG_LOG("Running app version %s", &sw_build[1]);
}

static void deviceAnnounceCommand(void)
{
  EmberNodeId shortAddress = emberAfGetNodeId();
  uint8_t frame[12] = {0};
  EmberStatus status;

  frame[0] = emberNextZigDevRequestSequence();

  emberAfCopyInt16u(frame, 1, shortAddress);
  memcpy(frame + 3, emberGetEui64(), EUI64_SIZE);
  frame[11] = 0xE0; // capability: fake like we are a router

  status = emberSendZigDevRequest(EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS,
                          END_DEVICE_ANNOUNCE,
                          EMBER_APS_OPTION_NONE,
                          frame,
                          sizeof(frame));

  DBG_LOG("Broadcast device announce, status %02x",
                     status);
}


void emberAfMainInitCallback(void)
{
    INFO_LOG("Starting main %d...", halCommonGetInt32uMillisecondTick());

    led_effect_init();
    button_init();
    ota_version_init();
}

void emberAfPluginFindAndBindInitiatorCompleteCallback(EmberStatus status)
{
    DBG_LOG("Find&bind completed with status %d", status);
    led_effect_run(LedEffect_None, 0);
}

bool emberAfPluginFindAndBindInitiatorBindTargetCallback(EmberNodeId nodeId,
                                                         EmberBindingTableEntry *bindingEntry,
                                                         uint8_t *groupName)
{
    if (bindingEntry != NULL &&
        bindingEntry->clusterId == ZCL_ON_OFF_CLUSTER_ID)
    {
        DBG_LOG("Binding to %04x on/off cluster!", nodeId);
        return true;
    }

    return false;
}

void emberAfMainTickCallback(void)
{
    if (net_initialized == false)
    {
        net_initialized = true;
        if (emberAfNetworkState() != EMBER_JOINED_NETWORK)
        {
            DBG_LOG("Not joined!");
            EmberStatus status = emberAfPluginNetworkSteeringStart();
            DBG_LOG("%s network %s: 0x%02X", "Join", "start", status);
        }
        else
        {
            DBG_LOG("In net!");
            deviceAnnounceCommand();
        }

        led_effect_run(LedEffect_Reboot, 1);
    }

    if (off_cmd_ts != 0 &&
        halCommonGetInt32uMillisecondTick() - off_cmd_ts > OFF_CMD_SEND_DELAY)
    {
        off_cmd_ts = 0;
        sendOffToBindings();
    }

    button_process();
}

void button_on_event(int num_of_press)
{
    switch(num_of_press)
    {
        case BUTTON_LONG_PRESS_EV:
        {
            DBG_LOG("Leaving network...");
            emberLeaveNetwork();

            EmberAfOtaStorageStatus status = emberAfOtaStorageClearTempDataCallback();
            DBG_LOG("OTA storage clear with status: 0x%X", status);

            EmberStatus eb_s = emberClearBindingTable();
            DBG_LOG("Binding table clear with status %02x!", eb_s);

            exec_reboot = true;
            break;
        }
        case 1:
        {
            DBG_LOG("Send OFF to bindings!");
            sendOffToBindings();
            break;
        }
        case 2:
        {
            DBG_LOG("Initiating find&bind!");
            led_effect_run(LedEffect_FindAndBind, 0);
            emberAfPluginFindAndBindInitiatorStart(emberAfPrimaryEndpointForCurrentNetworkIndex());
            break;
        }
        case 3:
        {
            EmberStatus eb_s = emberClearBindingTable();
            DBG_LOG("Binding table clear with status %02x!", eb_s);
            break;
        }
        default:
        {
            break;
        }
    }
}

static void sendOffToBindings(void)
{
    EmberApsFrame apsFrame =
    {
        .profileId = HA_PROFILE_ID,
        .sourceEndpoint = emberAfPrimaryEndpointForCurrentNetworkIndex(),
        .options = EMBER_APS_OPTION_RETRY,
        .radius = ZA_MAX_HOPS
    };

    uint8_t cmd[] = {ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_DISABLE_DEFAULT_RESPONSE_MASK | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER,
                     0x00, ZCL_OFF_COMMAND_ID};
    uint16_t len = sizeof(cmd);
    EmberStatus status = EMBER_INVALID_BINDING_INDEX;
    uint8_t i;

    if (emberAfNetworkState() != EMBER_JOINED_NETWORK)
    {
        DBG_LOG("No network! Canceling OFF to bindings!");
        return;
    }

    for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++)
    {
      EmberBindingTableEntry binding;
      status = emberGetBinding(i, &binding);
      if (status != EMBER_SUCCESS ||
          binding.type == EMBER_UNUSED_BINDING ||
          binding.clusterId != ZCL_ON_OFF_CLUSTER_ID)
      {
        continue;
      }

      if (binding.type == EMBER_UNICAST_BINDING)
      {
          apsFrame.clusterId = binding.clusterId;
          apsFrame.destinationEndpoint = binding.remote;

          cmd[1] = emberAfNextSequence();

          status = emberAfSendUnicast(EMBER_OUTGOING_VIA_BINDING,
                                      i,
                                      &apsFrame,
                                      len,
                                      cmd);
          DBG_LOG("Unicast to binding %d status %d", i, status);
      }
      else if (binding.type == EMBER_MULTICAST_BINDING)
      {
          uint16_t multicastId = binding.identifier[0] | (binding.identifier[1] << 8);
          apsFrame.clusterId = binding.clusterId;
          apsFrame.destinationEndpoint = binding.remote;

          cmd[1] = emberAfNextSequence();

          status = emberAfSendMulticast(multicastId,
                                       &apsFrame,
                                       len,
                                       cmd);
          DBG_LOG("Multicast to group %04x status %d", multicastId, status);
      }
      else
      {
          DBG_LOG("Skipping binding %d", binding.type);
      }
    }
}


/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action.  The return code
 * from this callback is ignored by the framework.  The framework will always
 * process the stack status after the callback returns.
 *
 * @param status   Ver.: always
 */
bool emberAfStackStatusCallback(EmberStatus status)
{
    switch(status)
    {
        case EMBER_NETWORK_DOWN:
        {
            DBG_LOG("Net down!");

            if (exec_reboot == true)
            {
                halReboot();
            }
            break;
        }
        case EMBER_NETWORK_UP:
        {
            DBG_LOG("Net up!");

            off_cmd_ts = halCommonGetInt32uMillisecondTick();
            break;
        }
        case EMBER_NETWORK_OPENED:
        {
            DBG_LOG("Net open!");
            break;
        }
        case EMBER_NETWORK_CLOSED:
        {
            DBG_LOG("Net closed!");
            break;
        }
        default:
        {
            break;
        }
    }

    return false;
}

/** @brief Complete
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to EMBER_SUCCESS to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt. Ver.: always
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID. Ver.: always
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network. Ver.: always
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete. Ver.: always
 */
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                                  uint8_t totalBeacons,
                                                  uint8_t joinAttempts,
                                                  uint8_t finalState)
{
  DBG_LOG("%p network %p: 0x%X", "Join", "complete", status);

  if (status != EMBER_SUCCESS)
  {
      status = emberAfPluginNetworkCreatorStart(false); // distributed
      DBG_LOG("Form network start: 0x%02X", status);
  }
}
