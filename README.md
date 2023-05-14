# MG1 Repeater

This is custom firmware for IKEA Tradfri Zigbee Repeater (E1746). Firmware is build from latest supported Gecko SDK 6.10.3 for EFR32MG1 family.

## Why custom firmware

I need custom firmware to implement additional functionalities on this device. In my case, was to send Zigbee ZCL OFF command when power loss occurs (when device boots). This is helpful in case when your lamps are configured to toggle on power cut - this setup is common when you have 'non smart' electrical installation in your home and wants to install smart bulbs with break switches (always on). So on accidental power loss, your lamps might be turned on and light until some click the wall switch - annoying in the middle of the night or when nobody's home. This firmware will send OFF command when power loss detected to avoid such case.

## What's in the firmware

This firmware setups the IKEA repeater device as 'Home Automation Range Extender'. The manufacturer ID is set to Ember (0x1002), OTA image type is 0x1000. So it's normal Zigbee 3.0 router node with OnOff client cluster from which OFF command is sent after boot up.

### Button functionality
- long press (3s) - factory reset: leaves current zigbee network,clears OTA storage and binding table.
- one press - send OFF command to all bound devices
- double press - initiates Find&Bind Target - in short: device will bind to On/Off cluster of all network devices which are currently identifying - so this is the method preferred to 'link' devices in the network (of course you can send manually ZDO bind command if you want to bind to zigbee group for instance).
- triple press - clear binding table - useful function when you want to stay in the network and only 'reset the links' between our device and others.

## Adding your new functionalities

It's easy to add new functionality to the firmware, as app source code is relatively small. Unfortunately the EFR32MG1 has limited flash space storage and it's almost full. You can disable serial logs to save some bytes. Also using IAR compiler will make code smaller.

After changing source code don't forget to increase firmware version to have generated updatable OTA file. This is done in mg1_repeater.isc->Plugins->OTA Bootload Client Cluster Policy->Firmware version field (each change in the ISC configuration file requires project regeneration).

## Importing to Simplicity Studio V5

Select import 'MCU Project', browse for location of downloaded repo. There should be listed two projects, select 'Simplicity Studio (.sls)' one.

After importing to your workspace, open 'mg1_repeater.isc' and click 'Generate' button located on top right side of the window.

When generating of the source code finish, you can build it using context menu option or toolbar hammer button.

## Flashing firmware

There are two options:
- using SWD - you have to open the device and solder SWD wires to flash it.
- using OTA - this will only work for IKEA firmwares < 2.3.74 (newer firmware versions are checking signature and reject unsigned OTA images like ours) - see bellow how to generate appropriate OTA firmware to update

### Generating 'convert OTA' firmware

```bash
~/SimplicityStudio/SDKs/gecko_sdk/protocol/zigbee/tool/image-builder/image-builder-linux -c mg1_repeater_convert.ota -v 0x25000000 -m 0x117c -i 0x1102 -t 0x0000 -f mg1_repeater.gbl

```

## License

[GNU](https://www.gnu.org/licenses/)
