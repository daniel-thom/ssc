#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H

#include <vector>

#include "lib_util.h"

namespace storage_params{
    enum CHEMS{ LEAD_ACID, LITHIUM_ION, VANADIUM_REDOX, IRON_FLOW};
    enum METERING { BEHIND, FRONT };
    enum CONNECTION { DC_CONNECTED, AC_CONNECTED };
    enum CURRENT_CHOICE { RESTRICT_POWER, RESTRICT_CURRENT, RESTRICT_BOTH };
    enum BTM_MODES { LOOK_AHEAD, LOOK_BEHIND, MAINTAIN_TARGET, CUSTOM_DISPATCH, MANUAL, RESILIENCE };
    enum BTM_TARGET_MODES {TARGET_SINGLE_MONTHLY, TARGET_TIME_SERIES};
    enum FOM_MODES { FOM_LOOK_AHEAD, FOM_LOOK_BEHIND, FOM_FORECAST, FOM_CUSTOM_DISPATCH, FOM_MANUAL, FOM_RESILIENCE };
    enum FOM_CYCLE_COST {MODEL_CYCLE_COST, INPUT_CYCLE_COST};
    enum REPLACEMENT { NONE, CAPACITY, SCHEDULE };
    enum CALENDAR_LOSS_OPTIONS {NA, LITHIUM_ION_CALENDAR_MODEL, CALENDAR_LOSS_TABLE};
}

struct storage_config_params
{
    bool is_batt;
    bool is_fuelcell;
    int dispatch;                                   // from FOM_MODES or BTM_MODES
    int meter_position;                             // from METERING
};

struct storage_replacement_params
{
    /* Replacement options */
    int replacement_option;                         // 0=none,1=capacity based,2=user schedule
    double replacement_capacity;                    // kWh
    double cost_per_kwh;                            // $/kWh
    std::vector<int> replacement_per_yr;            // number/year
    std::vector<int> replacement_per_yr_schedule;   // number/year
    std::vector<double> replacement_percent_per_yr; // %
};

void storage_replacement_params_from_data(storage_replacement_params* params, var_table& vt, bool batt_not_fuelcell);

struct storage_time_params
{
    /* Constant values */
    size_t step_per_hour;
    size_t step_per_year;
    size_t nyears;
    double dt_hour;

    bool system_use_lifetime_output;                // true or false
    int analysis_period;
};

void storage_time_params_from_data(storage_time_params* params, var_table& vt);

struct storage_state
{
    /* Changing time values */
    size_t year;
    size_t hour;
    size_t step;
    size_t index;                                   // lifetime_index (0 - nyears * steps_per_hour * 8760)
    size_t year_index;                              // index for one year (0- steps_per_hour * 8760)
};

struct storage_forecast
{
    int prediction_index;
    std::vector<double> pv_prediction;
    std::vector<double> load_prediction;
    std::vector<double> cliploss_prediction;        // from inverter model in pvsam
};

void storage_forecast_from_data(storage_forecast* params, var_table& vt);

struct storage_FOM_params{
    double ppa_price;
    std::vector<double> ppa_price_series_dollar_per_kwh;

    std::vector<double> pv_clipping_forecast;
    std::vector<double> pv_dc_power_forecast;

    // Battery cycle costs
    int cycle_cost_choice;
    double cycle_cost;
};

void storage_FOM_params_from_data(storage_FOM_params* params, var_table& vt);

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

/**
 * Battery specific paramters
 */
struct battery_lifetime_params
{
    util::matrix_t<double> lifetime_matrix;

    int calendar_choice;                            // 0=NoCalendarDegradation,1=LithiomIonModel,2=InputLossTable

    double calendar_q0;
    double calendar_a;
    double calendar_b;
    double calendar_c;

    util::matrix_t<double> calendar_lifetime_matrix;
};

void battery_lifetime_params_from_data(battery_lifetime_params* params, var_table& vt);

struct battery_voltage_params
{
    int voltage_choice;                             // 0=UseVoltageModel,1=InputVoltageTable

    double Vnom_default;
    double Vfull;
    double Vexp;
    double Vnom;
    double Qfull;
    double Qfull_flow;
    double Qexp;
    double Qnom;
    double C_rate;
    double resistance;

    // voltage_choice 1
    util::matrix_t<double> voltage_matrix;
};

void battery_voltage_params_from_data(battery_voltage_params* params, var_table& vt);

struct battery_thermal_params
{
    double mass;
    double length;
    double width;
    double height;
    double Cp;
    util::matrix_t<double> cap_vs_temp;
    double h_to_ambient;
    std::vector<double> T_room;
};

void battery_thermal_params_from_data(battery_thermal_params* params, var_table& vt);

struct battery_capacity_params
{
    int current_choice;                             // from CURRENT_CHOICE

    double current_charge_max;
    double current_discharge_max;

    double power_charge_max_kwdc;
    double power_discharge_max_kwdc;
    double power_charge_max_kwac;
    double power_discharge_max_kwac;
};

void battery_capacity_params_from_data(battery_capacity_params* params, var_table& vt);

// for DC-connected batteries
struct battery_inverter_params
{
    size_t inverter_model;
    size_t inverter_count;
    double inverter_efficiency;
    double inverter_efficiency_cutoff;
    double inverter_paco;
};

void battery_inverter_params_from_data(battery_inverter_params* params, var_table& vt);

struct battery_losses_params
{
    int loss_monthly_or_timeseries;
    std::vector<double> losses_charging;
    std::vector<double> losses_discharging;
    std::vector<double> losses_idle;
    std::vector<double> losses;
};

void battery_losses_params_from_data(battery_losses_params* params, var_table& vt);

struct LeadAcid_properties_params
{
    double q20_computed;
    double tn;
    double qn_computed;
    double q10_computed;
};

void LeadAcid_properties_params_from_data(LeadAcid_properties_params* params, var_table& vt);

struct battery_properties_params
{
    int chem;
    LeadAcid_properties_params leadAcid;

    battery_thermal_params thermal;

    // Battery bank sizing
    int computed_series;
    int computed_strings;
    double kw;
    double kwh;

    // voltage properties using either voltage model or table
    battery_voltage_params voltage_vars;

    battery_capacity_params capacity_vars;

    // charge limits
    double initial_SOC;
    double maximum_SOC;
    double minimum_SOC;
    double minimum_modetime;

    // power converters and topology
    int topology;
    double ac_dc_efficiency;
    double dc_ac_efficiency;
    double dc_dc_bms_efficiency;
    double pv_dc_dc_mppt_efficiency;

    battery_inverter_params inverter;
};

void battery_properties_params_from_data(battery_properties_params* params, var_table& vt);

#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
