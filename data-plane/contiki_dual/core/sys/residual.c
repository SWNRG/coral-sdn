#include "residual.h"
#ifdef ZOLERTIA_Z1
const energy_t ENERGEST_DISSIPATION_RATE[] = {
  1, // ENERGEST_TYPE_CPU,
  0, //ENERGEST_TYPE_LPM,
  0, // ENERGEST_TYPE_IRQ,
  0, // ENERGEST_TYPE_LED_GREEN,
  0, // ENERGEST_TYPE_LED_YELLOW,
  0, // ENERGEST_TYPE_LED_RED,
  0, // ENERGEST_TYPE_TRANSMIT,
  0, // ENERGEST_TYPE_LISTEN,

  0, // ENERGEST_TYPE_TRANSMIT_LR,
  0, // ENERGEST_TYPE_LISTEN_LR,

  0, // ENERGEST_TYPE_FLASH_READ,
  0, // ENERGEST_TYPE_FLASH_WRITE,

  0, // ENERGEST_TYPE_SENSORS,

  0 // ENERGEST_TYPE_SERIAL,
};
#endif

#ifdef COOJA
extern energy_t COOJA_radioOn;
extern energy_t COOJA_radioTx;
extern energy_t COOJA_radioRx;
extern energy_t COOJA_radioOnLong;
extern energy_t COOJA_radioTxLong;
extern energy_t COOJA_radioRxLong;
extern energy_t COOJA_duration;
const energy_t DISSIPATION_RATE[] = {
    10,  // radioOn
    0,  // radioTx
    0,  // radioRx
    10,  // radioOnLong
    5,  // radioTxLong
    0,  // radioRxLong
    0,  // duration: simulation time
};
static const int DISSIPATION_RATE_DIVISOR = 16;
#endif

energy_t
get_residual_energy(void){
    int energy = RESIDUAL_ENERGY_MAX;
#ifdef ZOLERTIA_Z1
    int i;
    for(i = 0; i < ENERGEST_TYPE_MAX; i++){
        energy -= ENERGEST_DISSIPATION_RATE[i] * energest_total_time[i].current;
    }
#endif

#ifdef COOJA
    energy -= COOJA_radioOn * DISSIPATION_RATE[0] / DISSIPATION_RATE_DIVISOR;
    energy -= COOJA_radioTx * DISSIPATION_RATE[1] / DISSIPATION_RATE_DIVISOR;
    energy -= COOJA_radioRx * DISSIPATION_RATE[2] / DISSIPATION_RATE_DIVISOR;
#if DUAL_RADIO
    energy -= COOJA_radioOnLong * DISSIPATION_RATE[3] / DISSIPATION_RATE_DIVISOR;
    energy -= COOJA_radioTxLong * DISSIPATION_RATE[4] / DISSIPATION_RATE_DIVISOR;
    energy -= COOJA_radioRxLong * DISSIPATION_RATE[5] / DISSIPATION_RATE_DIVISOR;
#endif
    energy -= COOJA_duration * DISSIPATION_RATE[6] / DISSIPATION_RATE_DIVISOR;
    
    energy = energy < 0 ? 0 : energy;
#endif
    return energy;
}
