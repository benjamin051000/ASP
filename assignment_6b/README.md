# Assignment 6b
## Functionality
I am able to swap between MODE1 and MODE2 by pressing the NUMLOCK button on a hardware keyboard. When I press NUMLOCK, both the NUMLOCK LED and CAPSLOCK LED illuminate. 

## Issues
I have a problem where pressing CAPSLOCK does not trigger `usb_kbd_event`. I tried 3 different keyboards (Corsair K70 Lux Gaming Keyboard set to BIOS compatibility mode, Keychron K3 keyboard, and a Logitech Wireless keyboard) and the VirtualBox software keyboard on Ubuntu server 20.04.4 LTS. The terminal was able to tell caps lock was enabled because subsequent text would show up in all capital letters. However, the LED would not turn on. These were all tested with the default HID driver assigned by the kernel when I first connected the devices.

I plan on testing further on a newer release of Ubuntu. I believe my logic is correct, however, and should still invert the CAPSLOCK LED in MODE2.
