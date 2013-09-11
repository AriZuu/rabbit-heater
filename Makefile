all:
	msp430-gcc -mmcu=msp430g2533 -c heater.c
	msp430-gcc -mmcu=msp430g2533 -o heater.elf heater.o

clean:
	rm -f heater.o heater.elf

dist:
	rm -f ../dist/rabbit-heater-`date +%Y%m%d`.zip
	cd ..; zip -qr dist/rabbit-heater-`date +%Y%m%d`.zip rabbit-heater -x "*/.*" "*/bin/*" "*.launch" "*.patch" "*.hex"
