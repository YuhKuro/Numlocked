import usb.core
import usb.util

# IDs. To-Do: Figure out how to grab these from the device
VID = 0xcafe  
PID = 0x4004  

device = usb.core.find(idVendor=VID, idProduct=PID)
if device is None:
    print("Device not found")
else:
    print("Device found")
    for cfg in device:
        print("Configuration:", cfg.bConfigurationValue)
        for intf in cfg:
            print("  Interface:", intf.bInterfaceNumber, intf.bAlternateSetting)
            for ep in intf:
                print("    Endpoint:", ep.bEndpointAddress)
                OUT_ENDPOINT_ADDRESS = ep.bEndpointAddress


# Set the configuration
device.set_configuration()

# Send data to the device
def sendUSBCommand(id, message):

    try:
        device.write(OUT_ENDPOINT_ADDRESS, id + "" + message)
        print("Message sent!")
    except usb.core.USBError as e:
        print(f"Error sending message: {e}")


