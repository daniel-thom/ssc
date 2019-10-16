#include "lib_time.h"

#include "lib_shared_inverter.h"
#include "lib_dispatch_params.h"

void storage_forecast::initialize_from_data(var_table &vt) {
    prediction_index = 0;
    pv_prediction = vt.as_vector_double("gen");
    load_prediction = vt.as_vector_double("load");
    cliploss_prediction = std::vector<double>(pv_prediction.size(), 0);
}

void storage_FOM_params::initialize_from_data(var_table& vt, size_t step_per_hour){
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

void dispatch_automated_params::initialize_from_data(var_table& vt){
    look_ahead_hours = vt.as_unsigned_long("batt_look_ahead_hours");
    dispatch_update_frequency_hours = vt.as_double("batt_dispatch_update_frequency_hours");

    can_charge = true;
    can_clipcharge = true;
    can_gridcharge = false;
    can_fuelcellcharge = true;

    if (vt.is_assigned("batt_dispatch_auto_can_gridcharge")) {
        can_gridcharge = vt.as_boolean("batt_dispatch_auto_can_gridcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_charge")) {
        can_charge = vt.as_boolean("batt_dispatch_auto_can_charge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_clipcharge")) {
        can_clipcharge = vt.as_boolean("batt_dispatch_auto_can_clipcharge");
    }
    if (vt.is_assigned("batt_dispatch_auto_can_fuelcellcharge")) {
        can_fuelcellcharge = vt.as_boolean("batt_dispatch_auto_can_fuelcellcharge");
    }
}

void dispatch_manual_params::initialize_from_data(var_table& vt){
    can_charge = vt.as_vector_bool("dispatch_manual_charge");
    can_discharge = vt.as_vector_bool("dispatch_manual_discharge");
    can_gridcharge = vt.as_vector_bool("dispatch_manual_gridcharge");
    can_fuelcellcharge = vt.as_vector_bool("dispatch_manual_fuelcellcharge");
    auto percent_discharge = vt.as_vector_double("dispatch_manual_percent_discharge");
    auto percent_gridcharge = vt.as_vector_double("dispatch_manual_percent_gridcharge");
    discharge_schedule_weekday = vt.as_matrix_unsigned_long("dispatch_manual_sched");
    discharge_schedule_weekend = vt.as_matrix_unsigned_long("dispatch_manual_sched_weekend");

    /*! Generic manual dispatch model inputs */
    if (can_charge.size() != 6 || can_discharge.size() != 6 || can_gridcharge.size() != 6)
        throw general_error("invalid manual dispatch control vector lengths, must be length 6");

    if (discharge_schedule_weekday.nrows() != 12 || discharge_schedule_weekday.ncols() != 24)
        throw general_error("invalid manual dispatch schedule matrix dimensions, must be 12 x 24");

    if (discharge_schedule_weekend.nrows() != 12 || discharge_schedule_weekend.ncols() != 24)
        throw general_error("invalid weekend manual dispatch schedule matrix dimensions, must be 12 x 24");

    if (auto vd = vt.lookup("fuelcell_power")) {
        if (can_fuelcellcharge.size() != 6)
            throw general_error("invalid manual dispatch control vector lengths, must be length 6");
    }

    size_t discharge_index = 0;
    size_t gridcharge_index = 0;
    for (size_t i = 0; i < can_discharge.size(); i++)
    {
        if (can_discharge[i])
            discharge_percent[i + 1] = percent_discharge[discharge_index++];

        if (can_gridcharge[i])
            gridcharge_percent[i + 1] = percent_gridcharge[gridcharge_index++];
    }
}

void battery_charging_params::initialize_from_data(var_table &vt, std::shared_ptr<const battery_capacity_params> cap) {

}
void battery_controller_params::initialize_from_data(var_table &vt, std::shared_ptr<battery_properties_params>& p) {
    capacity = p->capacity;

    restriction = static_cast<RESTRICTION >(vt.as_integer("batt_current_choice"));
    minimum_modetime = vt.as_double("batt_minimum_modetime");

    if (restriction == RESTRICT_CURRENT ||
        restriction == RESTRICT_BOTH){
        current_charge_max = vt.as_double("batt_current_charge_max");
        current_discharge_max = vt.as_double("batt_current_discharge_max");
    }
    if (restriction == RESTRICT_POWER ||
        restriction == RESTRICT_BOTH) {
        power_charge_max_kwdc = vt.as_double("batt_power_charge_max_kwdc");
        power_discharge_max_kwdc = vt.as_double("batt_power_discharge_max_kwdc");
        power_charge_max_kwac = vt.as_double("batt_power_charge_max_kwac");
        power_discharge_max_kwac = vt.as_double("batt_power_discharge_max_kwac");
    }

    connection = static_cast<CONNECT>(vt.as_integer("batt_ac_or_dc"));
    if (connection == DC){
        double inverter_eff;
        if (vt.is_assigned("inverter_model"))
        {
            int inverter_model = vt.as_integer("inverter_model");
            int inverter_count = vt.as_number("inverter_count");
            if (inverter_model == SharedInverter::SANDIA_INVERTER)
            {
                inverter_eff = vt.as_double("inv_snl_eff_cec");
                inverter_AC_nameplate_kW = inverter_count * vt.as_double("inv_snl_paco") * util::watt_to_kilowatt;
            }
            else if (inverter_model == SharedInverter::DATASHEET_INVERTER)
            {
                inverter_eff = vt.as_double("inv_ds_eff");
                inverter_AC_nameplate_kW = inverter_count * vt.as_double("inv_ds_paco") * util::watt_to_kilowatt;

            }
            else if (inverter_model == SharedInverter::PARTLOAD_INVERTER)
            {
                inverter_eff = vt.as_double("inv_pd_eff");
                inverter_AC_nameplate_kW = inverter_count * vt.as_double("inv_pd_paco") * util::watt_to_kilowatt;
            }
            else if (inverter_model == SharedInverter::COEFFICIENT_GENERATOR)
            {
                inverter_eff = vt.as_double("inv_cec_cg_eff_cec");
                inverter_AC_nameplate_kW = inverter_count * vt.as_double("inv_cec_cg_paco") * util::watt_to_kilowatt;
            }
            else
                throw general_error("inverter model must be between 0 and 3");
        }
        else
        {
            inverter_eff = vt.as_double("batt_ac_dc_efficiency");
            inverter_AC_nameplate_kW = vt.as_double("batt_power_discharge_max_kwdc");
        }
        auto dc_dc_bms_efficiency = vt.as_double("batt_dc_dc_efficiency");
        discharge_eff = dc_dc_bms_efficiency * 0.01 * inverter_eff;
        pvcharge_eff = dc_dc_bms_efficiency;
        gridcharge_eff = discharge_eff;
    }
    else{
        pvcharge_eff = vt.as_double("batt_ac_dc_efficiency");
        discharge_eff = vt.as_double("batt_dc_ac_efficiency");
        gridcharge_eff = pvcharge_eff;
    }

}

void battery_power_params::initialize_from_data(var_table &vt, std::shared_ptr<battery_properties_params> p) {
    kwh = vt.as_double("batt_computed_bank_capacity");
    kw = vt.as_double("batt_power_discharge_max_kwdc");

    capacity = p->capacity;

}
