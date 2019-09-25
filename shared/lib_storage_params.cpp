#include "vartab.h"
#include "lib_time.h"
#include "lib_shared_inverter.h"
#include "lib_storage_params.h"

void storage_replacement_params_from_data(storage_replacement_params* params, var_table& vt, bool batt_not_fuelcell){
    params->replacement_option = vt.as_integer("batt_replacement_option");

    if (params->replacement_option == 0)
        return;

    if (batt_not_fuelcell && vt.is_assigned("om_replacement_cost1")){
        params->cost_per_kwh = vt.as_vector_double("om_replacement_cost1")[0];
        if (params->replacement_option == storage_params::REPLACEMENT::CAPACITY){
            params->replacement_capacity = vt.as_double("batt_replacement_capacity");
        }
        else if (params->replacement_option == storage_params::REPLACEMENT::SCHEDULE){
            params->replacement_per_yr = vt.as_vector_integer("batt_bank_replacement");
            params->replacement_per_yr_schedule = vt.as_vector_integer("batt_replacement_schedule");
            params->replacement_percent_per_yr = vt.as_vector_double("batt_replacement_schedule_percent");
        }
        // other battery-specific lifetime variables are processed in battery replacement params
    }
    if (!batt_not_fuelcell && vt.is_assigned("fuelcell_per_kWh")){
        params->cost_per_kwh = vt.as_number("fuelcell_per_kWh");
        if (params->replacement_option == storage_params::REPLACEMENT::CAPACITY){
            params->replacement_capacity = vt.as_double("fuelcell_replacement_percent");
        }
        else if (params->replacement_option == storage_params::REPLACEMENT::SCHEDULE){
            params->replacement_per_yr = vt.as_vector_integer("fuelcell_replacement");
            params->replacement_per_yr_schedule = vt.as_vector_integer("fuelcell_replacement_schedule");
            params->replacement_percent_per_yr = vt.as_vector_double("batt_replacement_schedule_percent");
        }
    }
}

void storage_time_params_from_data(storage_time_params* time_params, var_table& vt){
    time_params->dt_hour = vt.as_number("timestep_hr");
    time_params->step_per_hour = static_cast<size_t>(1. / time_params->dt_hour);
    time_params->step_per_year = 8760 * time_params->step_per_hour;
    time_params->analysis_period = vt.as_integer("analysis_period");
    time_params->system_use_lifetime_output = vt.as_boolean("system_use_lifetime_output");
    if (time_params->system_use_lifetime_output)
        time_params->nyears = vt.as_integer("analysis_period");
    else
        time_params->nyears = 1;
}

void storage_forecast_from_data(storage_forecast* params, var_table& vt){
    params->prediction_index = 0;
    params->pv_prediction = vt.as_vector_double("gen");
    params->load_prediction = vt.as_vector_double("load");
    params->cliploss_prediction = std::vector<double>(params->pv_prediction.size(), 0);
}

void storage_FOM_params_from_data(storage_FOM_params* params, var_table& vt, size_t step_per_hour){
    params->pv_clipping_forecast = vt.as_vector_double("batt_pv_clipping_forecast");
    params->pv_dc_power_forecast = vt.as_vector_double("batt_pv_dc_forecast");
    double ppa_price = vt.as_double("ppa_price_input");
    int ppa_multiplier_mode = vt.as_integer("ppa_multiplier_model");

    if (ppa_multiplier_mode == 0) {
        params->ppa_price_series_dollar_per_kwh = flatten_diurnal(
                vt.as_matrix_unsigned_long("dispatch_sched_weekday"),
                vt.as_matrix_unsigned_long("dispatch_sched_weekend"),
                step_per_hour,
                vt.as_vector_double("dispatch_tod_factors"), ppa_price);
    }
    else {
        params->ppa_price_series_dollar_per_kwh = vt.as_vector_double("dispatch_factors_ts");
        for (auto& i : params->ppa_price_series_dollar_per_kwh) {
            i *= ppa_price;
        }
    }

    params->cycle_cost_choice = vt.as_integer("batt_cycle_cost_choice");
    params->cycle_cost = vt.as_double("batt_cycle_cost");
}

void storage_automated_dispatch_params_from_data(storage_automated_dispatch_params* params, var_table& vt){
    params->dispatch_auto_can_charge = true;
    params->dispatch_auto_can_clipcharge = true;
    params->dispatch_auto_can_gridcharge = false;
    params->dispatch_auto_can_fuelcellcharge = true;

    if (vt.is_assigned("batt_dispatch_auto_can_gridcharge")) {
        params->dispatch_auto_can_gridcharge = vt.as_boolean("batt_dispatch_auto_can_gridcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_charge")) {
        params->dispatch_auto_can_charge = vt.as_boolean("batt_dispatch_auto_can_charge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_clipcharge")) {
        params->dispatch_auto_can_clipcharge = vt.as_boolean("batt_dispatch_auto_can_clipcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_fuelcellcharge")) {
        params->dispatch_auto_can_fuelcellcharge = vt.as_boolean("batt_dispatch_auto_can_fuelcellcharge");
    }
}

void storage_manual_dispatch_params_from_data(storage_manual_dispatch_params* params, var_table& vt){
    params->can_charge = vt.as_vector_bool("dispatch_manual_charge");
    params->can_discharge = vt.as_vector_bool("dispatch_manual_discharge");
    params->can_gridcharge = vt.as_vector_bool("dispatch_manual_gridcharge");
    params->discharge_percent = vt.as_vector_double("dispatch_manual_percent_discharge");
    params->gridcharge_percent = vt.as_vector_double("dispatch_manual_percent_gridcharge");
    params->discharge_schedule_weekday = vt.as_matrix_unsigned_long("dispatch_manual_sched");
    params->discharge_schedule_weekend = vt.as_matrix_unsigned_long("dispatch_manual_sched_weekend");
}

void battery_lifetime_params_from_data(battery_lifetime_params* params, var_table& vt){
    storage_replacement_params_from_data(&params->replacement, vt, true);

    params->lifetime_matrix = vt.as_matrix("batt_lifetime_matrix");
    if (params->lifetime_matrix.nrows() < 3 || params->lifetime_matrix.ncols() != 3)
        throw general_error("Battery lifetime matrix must have three columns and at least three rows");

    params->calendar_choice = vt.as_integer("batt_calendar_choice");
    if (params->calendar_choice == storage_params::CALENDAR_OPTIONS::CALENDAR_LOSS_TABLE){
        params->calendar_lifetime_matrix = vt.as_matrix("batt_calendar_lifetime_matrix");
        if ((params->calendar_lifetime_matrix.nrows() < 2 || params->calendar_lifetime_matrix.ncols() != 2))
            throw general_error("Battery calendar lifetime matrix must have 2 columns and at least 2 rows");
    }
    if (params->calendar_choice == storage_params::CALENDAR_OPTIONS::LITHIUM_ION_CALENDAR_MODEL) {
        params->calendar_q0 = vt.as_double("batt_calendar_q0");
        params->calendar_a = vt.as_double("batt_calendar_a");
        params->calendar_b = vt.as_double("batt_calendar_b");
        params->calendar_c = vt.as_double("batt_calendar_c");
    }
}

void battery_losses_params_from_data(battery_losses_params* params, var_table& vt){
    params->loss_monthly_or_timeseries = vt.as_integer("batt_loss_choice");
    params->losses_charging = vt.as_vector_double("batt_losses_charging");
    params->losses_discharging = vt.as_vector_double("batt_losses_discharging");
    params->losses_idle = vt.as_vector_double("batt_losses_idle");
    params->losses = vt.as_vector_double("batt_losses");

    // Check loss inputs
    if (params->loss_monthly_or_timeseries == storage_params::LOSSES::MONTHLY
        && !(params->losses_charging.size() == 1 || params->losses_charging.size() == 12)) {
        throw general_error("charging loss length must be 1 or 12 for monthly input mode");
    }
    if (params->loss_monthly_or_timeseries == storage_params::LOSSES::MONTHLY
        && !(params->losses_discharging.size() == 1 || params->losses_discharging.size() == 12)) {
        throw general_error("discharging loss length must be 1 or 12 for monthly input mode");
    }
    if (params->loss_monthly_or_timeseries == storage_params::LOSSES::MONTHLY
        && !(params->losses_idle.size() == 1 || params->losses_idle.size() == 12)) {
        throw general_error("discharging loss length must be 1 or 12 for monthly input mode");
    }
}

void battery_voltage_params_from_data(battery_voltage_params* params, var_table& vt){
    params->voltage_choice = vt.as_integer("batt_voltage_choice");

    if (params->voltage_choice == 0){
        params->Vnom_default = vt.as_double("batt_Vnom_default");
        params->Vfull = vt.as_double("batt_Vfull");
        params->Vexp = vt.as_double("batt_Vexp");
        params->Vnom = vt.as_double("batt_Vnom");
        params->Qfull_flow = vt.as_double("batt_Qfull_flow");
        params->Qfull = vt.as_double("batt_Qfull");
        params->Qexp = vt.as_double("batt_Qexp");
        params->Qnom = vt.as_double("batt_Qnom");
        params->C_rate = vt.as_double("batt_C_rate");
        params->resistance = vt.as_double("batt_resistance");
    }
    else{
        params->voltage_matrix = vt.as_matrix("batt_voltage_matrix");
        if (params->voltage_matrix.nrows() < 2 || params->voltage_matrix.ncols() != 2)
            throw general_error("Battery voltage matrix must have 2 columns and at least 2 rows");
    }
}

void battery_thermal_params_from_data(battery_thermal_params* params, var_table& vt){
    params->cap_vs_temp = vt.as_matrix("cap_vs_temp");
    params->mass = vt.as_double("batt_mass");
    params->length = vt.as_double("batt_length");
    params->width = vt.as_double("batt_width");
    params->height = vt.as_double("batt_height");
    params->Cp = vt.as_double("batt_Cp");
    params->h_to_ambient = vt.as_double("batt_h_to_ambient");
    params->T_room = vt.as_vector_double("batt_room_temperature_celsius");

    for (auto& i : params->T_room) {
        i += 273.15; // convert C to K
    }
}

void battery_capacity_params_from_data(battery_capacity_params* params, var_table& vt){
    params->current_choice = vt.as_integer("batt_current_choice");
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
        params->inverter_efficiency_cutoff = vt.as_double("batt_inverter_efficiency_cutoff");

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
        params->inverter_paco = vt.as_double("batt_power_discharge_max");
    }
}

void LeadAcid_properties_params_from_data(LeadAcid_properties_params* params, var_table& vt){
    params->q10_computed = vt.as_double("LeadAcid_q10_computed");
    params->q20_computed = vt.as_double("LeadAcid_q20_computed");
    params->qn_computed = vt.as_double("LeadAcid_qn_computed");
    params->tn = vt.as_double("LeadAcid_tn");
}

void battery_properties_params_from_data(battery_properties_params* params, var_table& vt){
    params->chem = vt.as_integer("batt_chem");

    if (params->chem == storage_params::CHEMS::LEAD_ACID)
        LeadAcid_properties_params_from_data(&params->leadAcid, vt);

    battery_thermal_params_from_data(&params->thermal, vt);

    params->computed_series = vt.as_integer("batt_computed_series");
    params->computed_strings = vt.as_integer("batt_computed_strings");
    params->kwh = vt.as_double("batt_computed_bank_capacity");
    params->kw = vt.as_double("batt_power_discharge_max");

    battery_voltage_params_from_data(&params->voltage_vars, vt);
    
    battery_capacity_params_from_data(&params->capacity_vars, vt);

    params->initial_SOC = vt.as_double("batt_initial_SOC");
    params->maximum_SOC = vt.as_double("batt_maximum_soc");
    params->minimum_SOC = vt.as_double("batt_minimum_soc");
    params->minimum_modetime = vt.as_double("batt_minimum_modetime");

    params->topology = vt.as_integer("batt_ac_or_dc");
    params->ac_dc_efficiency = vt.as_double("batt_ac_dc_efficiency");
    params->dc_ac_efficiency = vt.as_double("batt_dc_ac_efficiency");
    params->dc_dc_bms_efficiency = vt.as_double("batt_dc_dc_efficiency");
    if (vt.is_assigned("dcoptimizer_loss")) {
        params->pv_dc_dc_mppt_efficiency = 100. - vt.as_double("dcoptimizer_loss");
    }
    else {
        params->pv_dc_dc_mppt_efficiency = 100;
    }

    battery_inverter_params_from_data(&params->inverter, vt);
}