#include "lib_dispatch_params.h"


void storage_forecast::initialize_from_data(var_table &vt) {
    prediction_index = 0;
    pv_prediction = vt.as_vector_double("gen");
    load_prediction = vt.as_vector_double("load");
    cliploss_prediction = std::vector<double>(pv_prediction.size(), 0);
}

void storage_FOM_params_from_data(storage_FOM_params* params, var_table& vt, size_t step_per_hour){
    pv_clipping_forecast = vt.as_vector_double("batt_pv_clipping_forecast");
    pv_dc_power_forecast = vt.as_vector_double("batt_pv_dc_forecast");
    double ppa_price = vt.as_double("ppa_price_input");
    int ppa_multiplier_mode = vt.as_integer("ppa_multiplier_model");

    if (ppa_multiplier_mode == 0) {
        ppa_price_series_dollar_per_kwh = flatten_diurnal(
                vt.as_matrix_unsigned_long("dispatch_sched_weekday"),
                vt.as_matrix_unsigned_long("dispatch_sched_weekend"),
                step_per_hour,
                vt.as_vector_double("dispatch_tod_factors"), ppa_price);
    }
    else {
        ppa_price_series_dollar_per_kwh = vt.as_vector_double("dispatch_factors_ts");
        for (auto& i : ppa_price_series_dollar_per_kwh) {
            i *= ppa_price;
        }
    }

    cycle_cost_choice = vt.as_integer("batt_cycle_cost_choice");
    cycle_cost = vt.as_double("batt_cycle_cost");
}

void storage_automated_dispatch_params_from_data(storage_automated_dispatch_params* params, var_table& vt){
    dispatch_auto_can_charge = true;
    dispatch_auto_can_clipcharge = true;
    dispatch_auto_can_gridcharge = false;
    dispatch_auto_can_fuelcellcharge = true;

    if (vt.is_assigned("batt_dispatch_auto_can_gridcharge")) {
        dispatch_auto_can_gridcharge = vt.as_boolean("batt_dispatch_auto_can_gridcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_charge")) {
        dispatch_auto_can_charge = vt.as_boolean("batt_dispatch_auto_can_charge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_clipcharge")) {
        dispatch_auto_can_clipcharge = vt.as_boolean("batt_dispatch_auto_can_clipcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_fuelcellcharge")) {
        dispatch_auto_can_fuelcellcharge = vt.as_boolean("batt_dispatch_auto_can_fuelcellcharge");
    }
}

void storage_manual_dispatch_params_from_data(storage_manual_dispatch_params* params, var_table& vt){
    can_charge = vt.as_vector_bool("dispatch_manual_charge");
    can_discharge = vt.as_vector_bool("dispatch_manual_discharge");
    can_gridcharge = vt.as_vector_bool("dispatch_manual_gridcharge");
    discharge_percent = vt.as_vector_double("dispatch_manual_percent_discharge");
    gridcharge_percent = vt.as_vector_double("dispatch_manual_percent_gridcharge");
    discharge_schedule_weekday = vt.as_matrix_unsigned_long("dispatch_manual_sched");
    discharge_schedule_weekend = vt.as_matrix_unsigned_long("dispatch_manual_sched_weekend");
}

void battery_charging_params_from_data(battery_charging_params* params, var_table& vt){
    params->current_choice = vt.as_integer("batt_current_choice");
    minimum_modetime = vt.as_double("batt_minimum_modetime");

    if (params->current_choice == storage_params::CURRENT_CHOICE::RESTRICT_CURRENT ||
        params->current_choice == storage_params::CURRENT_CHOICE::RESTRICT_BOTH){
        params->current_charge_max = vt.as_double("batt_current_charge_max");
        params->current_discharge_max = vt.as_double("batt_current_discharge_max");
    }
    if (params->current_choice == storage_params::CURRENT_CHOICE::RESTRICT_POWER ||
        params->current_choice == storage_params::CURRENT_CHOICE::RESTRICT_BOTH) {
        params->power_charge_max_kwdc = vt.as_double("batt_power_charge_max_kwdc");
        params->power_discharge_max_kwdc = vt.as_double("batt_power_discharge_max_kwdc");
        params->power_charge_max_kwac = vt.as_double("batt_power_charge_max_kwac");
        params->power_discharge_max_kwac = vt.as_double("batt_power_discharge_max_kwac");
    }
}

void battery_inverter_params_from_data(battery_inverter_params* params, var_table& vt){
    if (vt.is_assigned("inverter_model"))
    {
        params->inverter_model = vt.as_integer("inverter_model");
        params->inverter_count = vt.as_integer("inverter_count");
        inverter_efficiency_cutoff = vt.as_double("batt_inverter_efficiency_cutoff");

        if (params->inverter_model == SharedInverter::SANDIA_INVERTER)
        {
            params->inverter_efficiency = vt.as_double("inv_snl_eff_cec");
            params->inverter_paco = params->inverter_count * vt.as_double("inv_snl_paco") * util::watt_to_kilowatt;
        }
        else if (params->inverter_model == SharedInverter::DATASHEET_INVERTER)
        {
            params->inverter_efficiency = vt.as_double("inv_ds_eff");
            params->inverter_paco = params->inverter_count * vt.as_double("inv_ds_paco") * util::watt_to_kilowatt;

        }
        else if (params->inverter_model == SharedInverter::PARTLOAD_INVERTER)
        {
            params->inverter_efficiency = vt.as_double("inv_pd_eff");
            params->inverter_paco = params->inverter_count * vt.as_double("inv_pd_paco") * util::watt_to_kilowatt;
        }
        else if (params->inverter_model == SharedInverter::COEFFICIENT_GENERATOR)
        {
            params->inverter_efficiency = vt.as_double("inv_cec_cg_eff_cec");
            params->inverter_paco = params->inverter_count * vt.as_double("inv_cec_cg_paco") * util::watt_to_kilowatt;
        }
    }
    else
    {
        params->inverter_model = SharedInverter::NONE;
        params->inverter_count = 1;
        params->inverter_efficiency = vt.as_double("batt_ac_dc_efficiency");
        params->inverter_paco = vt.as_double("batt_power_discharge_max_kwdc");
    }
}