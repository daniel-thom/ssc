#ifndef SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H
#define SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H

#include <vector>

#include "vartab.h"

#include "lib_storage_params.h"

namespace dispatch_params{
    enum METERING { BEHIND, FRONT };
    enum CONNECTION { DC_CONNECTED, AC_CONNECTED };
    enum BTM_MODES { LOOK_AHEAD, LOOK_BEHIND, MAINTAIN_TARGET, CUSTOM_DISPATCH, MANUAL, RESILIENCE };
    enum BTM_TARGET_MODES {TARGET_SINGLE_MONTHLY, TARGET_TIME_SERIES};
    enum FOM_MODES { FOM_LOOK_AHEAD, FOM_LOOK_BEHIND, FOM_FORECAST, FOM_CUSTOM_DISPATCH, FOM_MANUAL, FOM_RESILIENCE };
    enum FOM_CYCLE_COST {MODEL_CYCLE_COST, INPUT_CYCLE_COST};

    const size_t constraintCount = 10;
}

struct battery_charging_params
{
    void initialize_from_data(var_table &vt, std::shared_ptr<const battery_capacity_params> cap);
};

struct storage_forecast
{
    int prediction_index;
    std::vector<double> pv_prediction;
    std::vector<double> load_prediction;
    std::vector<double> cliploss_prediction;        // from inverter model in pvsam
    std::vector<double> energy_price_forecast;

    void initialize_from_data(var_table &vt);
};

struct battery_controller_params
{
    enum RESTRICTION { RESTRICT_POWER, RESTRICT_CURRENT, RESTRICT_BOTH } restriction;

    double current_charge_max;
    double current_discharge_max;

    double power_charge_max_kwdc;
    double power_discharge_max_kwdc;
    double power_charge_max_kwac;
    double power_discharge_max_kwac;

    double minimum_modetime;                        // minimum time in a charging mode

    std::shared_ptr<const battery_capacity_params> capacity;

    enum CONNECT{DC, AC} connection;
    double pvcharge_eff;                // 0-100, same as batt_ac_dc_efficiency
    double discharge_eff;               // 0-100, same as batt_dc_ac_efficiency
    double gridcharge_eff;

    // dc-connected
    double inverter_AC_nameplate_kW;
    double inverter_efficiency_cutoff;

    void initialize_from_data(var_table& vt, std::shared_ptr<battery_properties_params>& p);

};

struct electricity_prices_params
{
    struct{
    bool ec_use_realtime;
    util::matrix_t<double> ec_weekday_schedule;
    util::matrix_t<double> ec_weekend_schedule;
    util::matrix_t<double> ec_tou_matrix;
    } energy_rate;

    std::vector<double> ec_realtime_buy;
    std::vector<double> ppa_price_real_timeseries;

    void initialize_from_data(var_table& vt, double step_per_hour){
        size_t count_ppa_price_input;
        ssc_number_t* ppa_price = vt.as_array("ppa_price_input", &count_ppa_price_input);

        int ppa_multiplier_mode = vt.as_integer("ppa_multiplier_model");

        if (ppa_multiplier_mode == 0) {
            ppa_price_real_timeseries = flatten_diurnal(
                    vt.as_matrix_unsigned_long("dispatch_sched_weekday"),
                    vt.as_matrix_unsigned_long("dispatch_sched_weekend"),
                    step_per_hour,
                    vt.as_vector_double("dispatch_tod_factors"), ppa_price[0]);
        }
        else {
            ppa_price_real_timeseries = vt.as_vector_double("dispatch_factors_ts");
            for (auto& i : ppa_price_real_timeseries) {
                i *= ppa_price[0];
            }
        }
        if (vt.is_assigned("en_electricity_rates")) {
            if (vt.as_integer("en_electricity_rates"))
            {
                energy_rate.ec_use_realtime = vt.as_boolean("ur_en_ts_sell_rate");
                if (!energy_rate.ec_use_realtime) {
                    energy_rate.ec_weekday_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekday");
                    energy_rate.ec_weekend_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekend");
                    energy_rate.ec_tou_matrix = vt.as_matrix("ur_ec_tou_mat");
                }
                else {
                    ec_realtime_buy = vt.as_vector_double("ur_ts_buy_rate");
                }
            }
            else {
                energy_rate.ec_use_realtime = true;
                ec_realtime_buy = ppa_price_real_timeseries;
            }
        }
    }
};

struct storage_FOM_params{
    double ppa_price;
    std::vector<double> ppa_price_series_dollar_per_kwh;

    std::vector<double> pv_clipping_forecast;
    std::vector<double> pv_dc_power_forecast;

    // Battery cycle costs
    int cycle_cost_choice;
    double cycle_cost;

    void initialize_from_data(var_table& vt, size_t step_per_hour);
};


struct dispatch_automated_params
{
    size_t look_ahead_hours;

    double dispatch_update_frequency_hours;

    /* Determines if the battery is allowed to charge from the grid using automated control*/
    bool can_gridcharge;

    /* Determines if the battery is allowed to charge from the RE using automated control*/
    bool can_charge;

    /* Determines if the battery is allowed to charge from PV clipping using automated control*/
    bool can_clipcharge;

    /* Determines if the battery is allowed to charge from fuel cell using automated control*/
    bool can_fuelcellcharge;

    void initialize_from_data(var_table& vt);
};


struct dispatch_manual_params
{
    /* Vector of periods and if battery can charge from PV*/
    std::vector<bool> can_charge;

    /* Vector of periods if battery can charge from Fuel Cell*/
    std::vector<bool> can_fuelcellcharge;

    /* Vector of periods and if battery can discharge*/
    std::vector<bool> can_discharge;

    /* Vector of periods and if battery can charge from the grid*/
    std::vector<bool> can_gridcharge;

    /* Schedule of manual discharge for weekday*/
    util::matrix_t<size_t> discharge_schedule_weekday;

    /* Schedule of manual discharge for weekend*/
    util::matrix_t<size_t> discharge_schedule_weekend;

    /* Vector of percentages that battery is allowed to charge for periods*/
    std::map<size_t, double> discharge_percent;

    /* Vector of percentages that battery is allowed to gridcharge for periods*/
    std::map<size_t, double> gridcharge_percent;

    void initialize_from_data(var_table& vt);
};

struct battery_BTM_peak_shaving{

    const int meter_position = dispatch_params::BEHIND;
    enum MODE{LOOK_AHEAD, LOOK_BEHIND} mode;

    std::shared_ptr<battery_charging_params> charging;

    std::shared_ptr<dispatch_automated_params> auto_dispatch;

    void initialize_from_data(var_table& vt, std::shared_ptr<battery_properties_params>& p){
        charging->initialize_from_data(vt, p->capacity);
        auto_dispatch->initialize_from_data(vt);
    }
};

struct battery_BTM_target_power{
    const int dispatch_mode = dispatch_params::BTM_MODES::MAINTAIN_TARGET;
    int meter_position = dispatch_params::BEHIND;

    std::shared_ptr<battery_charging_params> charging;

    std::vector<double> target_power_monthly;
    std::vector<double> target_power_timeseries;

    enum TARGET { MONTHLY, TIMESERIES} target;

    void initialize_from_data(var_table& vt, std::shared_ptr<battery_properties_params>& p){
        charging->initialize_from_data(vt, p->capacity);

        target = static_cast<TARGET>(vt.as_integer("batt_target_choice"));
        target_power_monthly = vt.as_vector_double("batt_target_power_monthly");
        target_power_timeseries = vt.as_vector_double("batt_target_power");

        if (target == MONTHLY)
        {
            target_power_monthly.clear();
            target_power_monthly.reserve(8760 * p->lifetime->time->step_per_hour);
            for (size_t month = 0; month != 12; month++)
            {
                double target_month = target_power_monthly[month];
                for (size_t h = 0; h != util::hours_in_month(month + 1); h++)
                {
                    for (size_t s = 0; s != p->lifetime->time->step_per_hour; s++)
                        target_power_monthly.emplace_back(target_month);
                }
            }

        }
        else{
            if (target_power_timeseries.size() != p->lifetime->time->step_per_hour * 8760)
                throw general_error("invalid number of target powers, must be equal to number of records in weather file");
        }

        // extend target power to lifetime internally
//        for (size_t y = 1; y < nyears; y++) {
//            for (size_t i = 0; i < nrec; i++) {
//                target_power.push_back(target_power[i]);
//            }
//        }
//        batt_vars->target_power = target_power;
    }
};

struct dispatch_BTM_custom{
    const int dispatch_mode = dispatch_params::BTM_MODES::CUSTOM_DISPATCH;
    int meter_position = dispatch_params::BEHIND;

    std::vector<double> custom_dispatch;

};

struct dispatch_BTM_manual{
    const int dispatch_mode = dispatch_params::BTM_MODES::MANUAL;
    int meter_position = dispatch_params::BEHIND;

    std::shared_ptr<dispatch_manual_params> manual;
};

struct dispatch_FTM_prices{
    enum MODE{LOOK_AHEAD, LOOK_BEHIND, INPUT_FORECAST} mode;
    const int meter_position = dispatch_params::FRONT;

    std::shared_ptr<battery_charging_params> charging;

    std::shared_ptr<electricity_prices_params> prices;

    std::shared_ptr<storage_forecast> forecast;

    double cost_per_kwh;
    double cost_per_kwh_fuelcell;
    double cycle_cost_choice;
    double cycle_cost;

    void initialize_from_data(var_table& vt, std::shared_ptr<battery_capacity_params>& cap){
        mode = static_cast<MODE>(vt.as_integer("batt_dispatch_choice"));
        charging->initialize_from_data(vt, cap);
        prices->initialize_from_data(vt, cap->time->step_per_hour);
        forecast->initialize_from_data(vt);

        cost_per_kwh_fuelcell = vt.as_number("fuelcell_per_kWh");
        cost_per_kwh = vt.as_vector_double("om_replacement_cost1")[0];
        cycle_cost_choice = vt.as_integer("batt_cycle_cost_choice");
        cycle_cost = vt.as_double("batt_cycle_cost");
    }

};

struct dispatch_FTM_custom{
    const int dispatch_mode = dispatch_params::FOM_MODES::FOM_CUSTOM_DISPATCH;
    int meter_position = dispatch_params::FRONT;

    std::vector<double> custom_dispatch;

    void initialize_from_data(var_table& vt, size_t step_per_hour){
        custom_dispatch = vt.as_vector_double("batt_custom_dispatch");
        if (custom_dispatch.size() != 8760 * step_per_hour) {
            throw general_error("invalid custom dispatch, must be 8760 * steps_per_hour");
        }
    }
};

struct dispatch_FTM_manual{
    std::shared_ptr<dispatch_manual_params> manual;

    pv_clipping_forecast = vt.as_vector_double("batt_pv_clipping_forecast");
    pv_dc_power_forecast = vt.as_vector_double("batt_pv_dc_forecast");

};

#endif //SYSTEM_ADVISOR_MODEL_LIB_DISPATCH_PARAMS_H
