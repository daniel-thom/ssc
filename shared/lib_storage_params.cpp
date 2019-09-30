#include "vartab.h"
#include "lib_shared_inverter.h"
#include "lib_storage_params.h"

void storage_replacement_params::initialize_from_data(var_table &vt, bool batt_not_fuelcell) {
    replacement_option = vt.as_integer("batt_replacement_option");

    if (replacement_option == 0)
        return;

    if (batt_not_fuelcell && vt.is_assigned("om_replacement_cost1")){
        cost_per_kwh = vt.as_vector_double("om_replacement_cost1")[0];
        if (replacement_option == storage_params::REPLACEMENT::CAPACITY){
            replacement_capacity = vt.as_double("batt_replacement_capacity");
        }
        else if (replacement_option == storage_params::REPLACEMENT::SCHEDULE){
            replacement_per_yr_schedule = vt.as_vector_integer("batt_replacement_schedule");
            replacement_percent_per_yr_schedule = vt.as_vector_double("batt_replacement_schedule_percent");
        }
        // other battery-specific lifetime variables are processed in battery replacement params
    }
    if (!batt_not_fuelcell && vt.is_assigned("fuelcell_per_kWh")){
        cost_per_kwh = vt.as_number("fuelcell_per_kWh");
        if (replacement_option == storage_params::REPLACEMENT::CAPACITY){
            replacement_capacity = vt.as_double("fuelcell_replacement_percent");
        }
        else if (replacement_option == storage_params::REPLACEMENT::SCHEDULE){
            replacement_per_yr = vt.as_vector_integer("fuelcell_replacement");
            replacement_per_yr_schedule = vt.as_vector_integer("fuelcell_replacement_schedule");
            replacement_percent_per_yr_schedule = vt.as_vector_double("batt_replacement_schedule_percent");
        }
    }
    // issues as capacity approaches 0%
    if (replacement_capacity == 0.) {
        replacement_capacity = 2.;
    }
}

void storage_time_params::initialize_from_data(var_table &vt) {
    dt_hour = vt.as_number("dt_hr");
    step_per_hour = static_cast<size_t>(1. / dt_hour);
    system_use_lifetime_output = vt.as_boolean("system_use_lifetime_output");
    if (system_use_lifetime_output)
        nyears = vt.as_integer("analysis_period");
    else
        nyears = 1;
}


void battery_lifetime_params::initialize_from_data(var_table &vt, storage_time_params &t, bool batt_not_fuelcell){
    time = std::make_shared<storage_time_params>(t);

    lifetime_matrix = vt.as_matrix("batt_lifetime_matrix");
    if (lifetime_matrix.nrows() < 3 || lifetime_matrix.ncols() != 3)
        throw general_error("Battery lifetime matrix must have three columns and at least three rows");

    calendar_choice = vt.as_integer("batt_calendar_choice");
    if (calendar_choice == storage_params::CALENDAR_OPTIONS::CALENDAR_LOSS_TABLE){
        calendar_lifetime_matrix = vt.as_matrix("batt_calendar_lifetime_matrix");
        if ((calendar_lifetime_matrix.nrows() < 2 || calendar_lifetime_matrix.ncols() != 2))
            throw general_error("Battery calendar lifetime matrix must have 2 columns and at least 2 rows");
    }
    if (calendar_choice == storage_params::CALENDAR_OPTIONS::LITHIUM_ION_CALENDAR_MODEL) {
        calendar_q0 = vt.as_double("batt_calendar_q0");
        calendar_a = vt.as_double("batt_calendar_a");
        calendar_b = vt.as_double("batt_calendar_b");
        calendar_c = vt.as_double("batt_calendar_c");
    }

    replacement.initialize_from_data(vt, batt_not_fuelcell);
}

void battery_losses_params::initialize_from_data(var_table &vt, storage_time_params &t){
    time = std::make_shared<storage_time_params>(t);
    loss_monthly_or_timeseries = vt.as_integer("batt_loss_choice");
    auto charging = vt.as_vector_double("batt_losses_charging");
    auto discharging = vt.as_vector_double("batt_losses_discharging");
    auto idle = vt.as_vector_double("batt_losses_idle");
    auto full = vt.as_vector_double("batt_losses");

    // Check loss inputs
    if (loss_monthly_or_timeseries == storage_params::LOSSES::MONTHLY){
        if (!(losses_charging.size() == 1 || losses_charging.size() == 12))
            throw general_error("charging loss length must be 1 or 12 for monthly input mode");
        if (!(losses_discharging.size() == 1 || losses_discharging.size() == 12))
            throw general_error("discharging loss length must be 1 or 12 for monthly input mode");
        if (!(losses_idle.size() == 1 || losses_idle.size() == 12))
            throw general_error("idle loss length must be 1 or 12 for monthly input mode");

        std::vector<double>* source[3] = {&charging, &discharging, &idle};
        std::vector<double>* dest[3] = {&losses_charging, &losses_discharging, &losses_idle};
        for (size_t i = 0; i < 3; i++) {
            if (source[i]->size() == 1) {
                for (size_t m = 0; m < 12; m++) {
                    dest[i]->emplace_back((*source[i])[0]);
                }
            }
            else if (charging.empty()) {
                for (size_t m = 0; m < 12; m++) {
                    dest[i]->push_back(0);
                }
            }
            else {
                *(dest[i]) = std::move(*(source[i]));
            }
        }
        for (size_t i = 0; i < (size_t)(8760 / time->dt_hour); i++) {
            losses_full.push_back(0);
        }
    }
    else{
        if (full.size() != (size_t)(8760 / time->dt_hour)) {
            for (size_t i = 0; i < (size_t)(8760 / time->dt_hour); i++) {
                losses_full.emplace_back(full[0]);
            }
        }
        else {
            losses_full = std::move(full);
        }
    }
}

void battery_voltage_params::initialize_from_data(var_table& vt){
    num_cells_series = vt.as_integer("batt_computed_series");
    num_strings = vt.as_integer("batt_computed_strings");

    voltage_choice = vt.as_integer("batt_voltage_choice");

    if (voltage_choice == 0){
        Vnom_default = vt.as_double("batt_Vnom_default");
        Vfull = vt.as_double("batt_Vfull");
        Vexp = vt.as_double("batt_Vexp");
        Vnom = vt.as_double("batt_Vnom");
        Qfull_flow = vt.as_double("batt_Qfull_flow");
        Qfull = vt.as_double("batt_Qfull");
        Qexp = vt.as_double("batt_Qexp");
        Qnom = vt.as_double("batt_Qnom");
        C_rate = vt.as_double("batt_C_rate");
        resistance = vt.as_double("batt_resistance");
    }
    else{
        voltage_matrix = vt.as_matrix("batt_voltage_matrix");
        if (voltage_matrix.nrows() < 2 || voltage_matrix.ncols() != 2)
            throw general_error("Battery voltage matrix must have 2 columns and at least 2 rows");
    }
}

void battery_thermal_params::initialize_from_data(var_table &vt, storage_time_params &t){
    time = std::make_shared<storage_time_params>(t);
    cap_vs_temp = vt.as_matrix("cap_vs_temp");
    mass = vt.as_double("batt_mass");
    length = vt.as_double("batt_length");
    width = vt.as_double("batt_width");
    height = vt.as_double("batt_height");
    Cp = vt.as_double("batt_Cp");
    resistance = vt.as_double("batt_resistance");
    h = vt.as_double("batt_h_to_ambient");
    T_room_K = vt.as_vector_double("batt_room_temperature_celsius");

    for (auto& i : T_room_K) {
        i += 273.15; // convert C to K
    }
}

void battery_capacity_params::initialize_from_data(var_table &vt, storage_time_params &t) {
    time = std::make_shared<storage_time_params>(t);
    int chem = vt.as_integer("batt_chem");

    initial_SOC = vt.as_double("batt_initial_SOC");
    maximum_SOC = vt.as_double("batt_maximum_soc");
    minimum_SOC = vt.as_double("batt_minimum_soc");

    if (chem == storage_params::CHEMS::LEAD_ACID){
        lead_acid.q10 = vt.as_double("LeadAcid_q10_computed");
        lead_acid.q20 = vt.as_double("LeadAcid_q20_computed");
        lead_acid.q1 = vt.as_double("LeadAcid_qn_computed");
        lead_acid.t1 = vt.as_double("LeadAcid_tn");
        qmax = lead_acid.q20;
    }
    else
        qmax = vt.as_double("batt_Qfull") * vt.as_double("batt_computed_strings");
}

void battery_properties_params::initialize_from_data(var_table& vt, storage_time_params& t){
    chem = vt.as_integer("batt_chem");

    thermal.initialize_from_data(vt, t);

    voltage_vars.initialize_from_data(vt);

    capacity_vars.initialize_from_data(vt, t);

    lifetime.initialize_from_data(vt, t, true);

    losses.initialize_from_data(vt, t);
}