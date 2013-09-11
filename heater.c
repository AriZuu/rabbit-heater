
#include <msp430.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

static void initConsole()
{
  UCA0CTL1 |= UCSWRST;

  P1DIR  |= BIT2;           // Set P1.2 as TX output
  P1SEL  |= BIT1 + BIT2;    // Select P1.1 & P1.2 to UART function
  P1SEL2 |= BIT1 + BIT2;    // Select P1.1 & P1.2 to UART function

  UCA0CTL1 = UCSSEL_2;      // SMCLK, running at MCLK

  UCA0BR0  = 0x68;
  UCA0BR1  = 0;
  UCA0MCTL = (1 << 1);

  UCA0CTL1 &= ~UCSWRST;     // Initialize USCI state machine

}

 int putchar(int c)
{
  while (!(UC0IFG & UCA0TXIFG)); // Wait for char to be transmitted
  UCA0TXBUF = c;

  return 0;
}

static uint16_t* tempCal30 = (uint16_t*)(0x10DA + 0x8);
static volatile int adcPot = 0;
static volatile int adcTemp = 0;

#define MOSFET_GATE_PIN BIT6
#define ZERO_DETECT_PIN BIT0

#define PWM_HZ	0.5
#define ACLK_HZ 10000

 int main(void)
{
  int prevPwm = 0;
  int prevDegC = 0;
  int degC;
  int pwm;

  WDTCTL = WDTPW  + WDTSSEL + WDTCNTCL; // ACLK / 32768: 3,2s

  // config clk
  BCSCTL1 = CALBC1_1MHZ;
  DCOCTL  = CALDCO_1MHZ;
  BCSCTL3 = LFXT1S_2;         // Use VLO

  // Unused pins as inputs with pull-down.

  P1DIR = 0;
  P1REN = ~(MOSFET_GATE_PIN | BIT1 | BIT2 | ZERO_DETECT_PIN);
  P2DIR = 0;
  P2REN = 0xff;

  initConsole();

  printf("Start\r\n");

  ADC10AE0 |= (1 << 4);              // PA.1 ADC option select

  // Configure MOSFET gate pin

  P1OUT &= ~MOSFET_GATE_PIN;
  P1DIR |= MOSFET_GATE_PIN;                     // P1.6 output

  // Configure voltage zero level detect pin

  P1IE |= ZERO_DETECT_PIN;
  P1IES |= ZERO_DETECT_PIN;

  // Timer A0 for PWM

  TA0CCR0 = (ACLK_HZ / PWM_HZ) - 1;

  TA0CCR1 = 0;
  TA0CCTL1 = OUTMOD_7 | CCIE;        // Positive PWM
  TA0CCTL0 = CCIE;
  TA0CTL = TASSEL_1 + MC_1 + TACLR + TAIE;  // ACLK, upmode

  __eint();

  while (1) {

    WDTCTL = WDTPW  + WDTSSEL + WDTCNTCL; // ACLK / 32768: 3,2s
    __bis_status_register(CPUOFF);

    degC = adcTemp - *tempCal30;
    degC = 30 + ((float)degC) / 1024 * 1500 / 3.55;

    // Filter temperature changes,
    // during cooling react only if change
    // is more than 2 degrees.

    if (degC > prevDegC)
      prevDegC = degC;
    else if (prevDegC - degC >= 2)
      prevDegC = degC;

    printf ("adc %d degc %d filtdegc %d ", adcPot, degC, prevDegC);

    if (adcPot < 20)
      adcPot = 0;
    else if (adcPot > 1000)
      adcPot = 1000;

    pwm = adcPot / 10; // PWM %

    if (pwm > 10 && prevDegC > 40) { // check overheat

      pwm = 10;
      printf ("overheat ");
    }

    printf("pwm %d %%\r\n", pwm);
    if (abs(prevPwm - pwm) > 2) {

      prevPwm = pwm;
      TA0CCR1 = ((float)pwm) / 100.0 * (ACLK_HZ / PWM_HZ);
    }
  }
}

static bool currentGate = false;

void pendOnOff(bool onOff)
{
  currentGate = onOff;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timerIrqHandler()
{
  // Setup ADC to read pot value

  ADC10CTL0 = ADC10SHT_3 + ADC10ON + ADC10IE + SREF_0 + REFON;
  ADC10CTL1 = ADC10DIV_3 + INCH_4;

  ADC10CTL0 |= ENC + ADC10SC;
}

void __attribute__((interrupt(TIMER0_A1_VECTOR))) timerIrqHandlerPwm()
{
  // Software assisted PWM. Allows delaying MOSFET state change until
  // input voltage is zero.

  int i = TA0IV;

  if (TA0CCR1 == 0) {

    pendOnOff(false);
    return;
  }

  switch (i) {
  case 2:
    pendOnOff(false);
    break;

  case 10:
    pendOnOff(true);
    break;
  }
}

void __attribute__((interrupt(ADC10_VECTOR))) ADC10_ISR(void)
{
  if ((ADC10CTL1 & (INCH0 | INCH1 | INCH2 | INCH3)) == INCH_4) {

    adcPot = ADC10MEM;

    // Pot done, read temperature

    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = ADC10SHT_3 + ADC10ON + ADC10IE + SREF_1 + REFON;
    ADC10CTL1 = ADC10DIV_3 + INCH_10;

    ADC10CTL0 |= ENC + ADC10SC;
  }
  else {

    adcTemp = ADC10MEM;
    ADC10CTL0 &= ~ENC;

    // All done, wake up main.

    __bic_status_register_on_exit(CPUOFF);
  }
}


void __attribute__((interrupt(PORT1_VECTOR))) zeroDetect(void)
{
  P1IFG &= ~ZERO_DETECT_PIN;

  switch (currentGate) {
  case false:
    P1OUT &= ~MOSFET_GATE_PIN;
    break;

  case true:
    P1OUT |= MOSFET_GATE_PIN;
    break;
  }
}
