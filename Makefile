all:
	msp430-gcc -mmcu=msp430g2533 -c heater.c
	msp430-gcc -mmcu=msp430g2533 -o heater.elf heater.o

clean:
	rm -f heater.o heater.elf
