sudo insmod leds_module.ko
sleep 2
echo "0xFF" > /dev/leds_test_module
sleep 2
echo "0x00" > /dev/leds_test_module
sleep 2
sudo rmmod leds_module