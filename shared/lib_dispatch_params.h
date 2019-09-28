#ifndef SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H
#define SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H

#include <vector>

#include "vartab.h"

#include "lib_storage_params.h"

namespace dispatch_params{
    enum METERING { BEHIND, FRONT };
    enum CONNECTION { DC_CONNECTED, AC_CONNECTED };
    enum CURRENT_CHOICE { RESTRICT_POWER, RESTRICT_CURRENT, RESTRICT_BOTH };
    enum BTM_MODES { LOOK_AHEAD, LOOK_BEHIND, MAINTAIN_TARGET, CUSTOM_DISPATCH, MANUAL, RESILIENCE };
    enum BTM_TARGET_MODES {TARGET_SINGLE_MONTHLY, TARGET_TIME_SERIES};
    enum FOM_MODES { FOM_LOOK_AHEAD, FOM_LOOK_BEHIND, FOM_FORECAST, FOM_CUSTOM_DISPATCH, FOM_MANUAL, FOM_RESILIENCE };
    enum FOM_CYCLE_COST {MODEL_CYCLE_COST, INPUT_CYCLE_COST};

    const size_t constraintCount = 10;
}

struct storage_forecast
{
    int prediction_index;
    std::vector<double> pv_prediction;
    std::vector<double> load_prediction;
    std::vector<double> cliploss_prediction;        // from inverter model in pvsam

    void initialize_from_data(var_table &vt);
};

struct battery_charging_params
{
    int current_choice;                             // from CURRENT_CHOICE

    double minimum_modetime;                        // minimum time in a charging mode

    double current_charge_max;
    double current_discharge_max;

    double power_charge_max_kwdc;
    double power_discharge_max_kwdc;
    double power_charge_max_kwac;
    double power_discharge_max_kwac;
};

void battery_charging_params_from_data(battery_charging_params* params, var_table& vt);

// for DC-connected batteries
struct battery_inverter_params
{
    size_t inverter_model;
    size_t inverter_count;
    double inverter_efficiency;
    double inverter_paco;
};

void battery_inverter_params_from_data(battery_inverter_params* params, var_table& vt);

struct battery_power_params
{
    // Battery bank sizing
    double kw;
    double kwh;

    std::shared_ptr<battery_capacity_params> capacity;

    // power converters and connection
    int meter_position;					 ///< 0 if behind-the-meter, 1 if front-of-meter
    int connection;
    double ac_dc_efficiency;
    double dc_ac_efficiency;
    double dc_dc_bms_efficiency;
    double pv_dc_dc_mppt_efficiency;
    double inverter_efficiency_cutoff;

    battery_charging_params charging;

};

struct storage_FOM_params{
    double ppa_price;
    std::vector<double> ppa_price_series_dollar_per_kwh;

    std::vector<double> pv_clipping_forecast;
    std::vector<double> pv_dc_power_forecast;

    // Battery cycle costs
    int cycle_cost_choice;
    double cycle_cost;
};

void storage_FOM_params_from_data(storage_FOM_params* params, var_table& vt, size_t step_per_hour);

struct storage_automated_dispatch_params
{
    /* Determines if the battery is allowed to charge from the grid using automated control*/
    bool dispatch_auto_can_gridcharge;

    /* Determines if the battery is allowed to charge from the RE using automated control*/
    bool dispatch_auto_can_charge;

    /* Determines if the battery is allowed to charge from PV clipping using automated control*/
    bool dispatch_auto_can_clipcharge;

    /* Determines if the battery is allowed to charge from fuel cell using automated control*/
    bool dispatch_auto_can_fuelcellcharge;

};

void storage_automated_dispatch_params_from_data(storage_automated_dispatch_params* params, var_table& vt);

struct storage_manual_dispatch_params
{
    /* Vector of periods and if battery can charge from PV*/
    std::vector<bool> can_charge;

    /* Vector of periods if battery can charge from Fuel Cell*/
    std::vector<bool> can_fuelcellcharge;

    /* Vector of periods and if battery can discharge*/
    std::vector<bool> can_discharge;

    /* Vector of periods and if battery can charge from the grid*/
    std::vector<bool> can_gridcharge;

    /* Vector of percentages that battery is allowed to charge for periods*/
    std::vector<double> discharge_percent;

    /* Vector of percentages that battery is allowed to gridcharge for periods*/
    std::vector<double> gridcharge_percent;

    /* Schedule of manual discharge for weekday*/
    util::matrix_t<size_t> discharge_schedule_weekday;

    /* Schedule of manual discharge for weekend*/
    util::matrix_t<size_t> discharge_schedule_weekend;
};

void storage_manual_dispatch_params_from_data(storage_manual_dispatch_params* params, var_table& vt);


#endif //SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H
