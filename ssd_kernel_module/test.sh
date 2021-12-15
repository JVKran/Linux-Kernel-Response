sudo insmod ssd_module.ko
sleep 2
echo "123456" > /dev/ssd_test_module
sleep 2
echo "0" > /dev/ssd_test_module
sleep 2
sudo rmmod ssd_module