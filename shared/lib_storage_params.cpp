#include "vartab.h"
#include "lib_shared_inverter.h"
#include "lib_storage_params.h"

void storage_replacement_params::initialize_from_data(var_table &vt, bool batt_not_fuelcell) {
    option = static_cast<REP>(vt.as_integer("batt_replacement_option"));

    if (option == NONE)
        return;

    if (batt_not_fuelcell && vt.is_assigned("om_replacement_cost1")){
        if (option == CAPACITY){
            replacement_capacity = vt.as_double("batt_replacement_capacity");
        }
        else if (option == SCHEDULE){
            replacement_per_yr_schedule = vt.as_vector_integer("batt_replacement_schedule");
            replacement_percent_per_yr_schedule = vt.as_vector_double("batt_replacement_schedule_percent");
        }
        // other battery-specific lifetime variables are processed in battery replacement params
    }
    if (!batt_not_fuelcell && vt.is_assigned("fuelcell_per_kWh")){
        if (option == CAPACITY){
            replacement_capacity = vt.as_double("fuelcell_replacement_percent");
        }
        else if (option == SCHEDULE){
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

storage_time_state::storage_time_state(size_t step_hr):
steps_per_hour(step_hr),
steps_per_year(8760 * steps_per_hour){
    year = 0;
    index = lifetime_index = -1;
}

storage_time_state storage_time_state::start(){
    index = lifetime_index = 0;
}

storage_time_state storage_time_state::increment(size_t steps){
    index += steps;
    lifetime_index += steps;
    if (index >= steps_per_year ){
        year += index / steps_per_year;
        index %= steps_per_year;
    }
    return *this;
}

storage_time_state storage_time_state::reset_storage_time(size_t lifetime_idx) {
    lifetime_index = lifetime_idx;
    index = lifetime_index % steps_per_year;
    year = lifetime_index/steps_per_year;
    return *this;
}

void battery_lifetime_params::initialize_from_data(var_table &vt, storage_time_params &t){
    time = std::make_shared<storage_time_params>(t);

    cycle_matrix = vt.as_matrix("batt_lifetime_matrix");
    if (cycle_matrix.nrows() < 3 || cycle_matrix.ncols() != 3)
        throw general_error("Battery lifetime matrix must have three columns and at least three rows");

    choice = static_cast<CALENDAR>(vt.as_integer("batt_calendar_choice"));
    switch (choice){
        case NONE:
            break;
        case MODEL : {
            calendar_q0 = vt.as_double("batt_calendar_q0");
            calendar_a = vt.as_double("batt_calendar_a");
            calendar_b = vt.as_double("batt_calendar_b");
            calendar_c = vt.as_double("batt_calendar_c");
            break;
        }
        case TABLE : {
            calendar_matrix = vt.as_matrix("batt_calendar_lifetime_matrix");
            if ((calendar_matrix.nrows() < 2 || calendar_matrix.ncols() != 2))
                throw general_error("Battery calendar lifetime matrix must have 2 columns and at least 2 rows");
            break;
        }
        default:
            throw general_error("batt_calendar_choice not recognized. Must be 0 (None), 1 (Table) or 2 (Model)");
    }
}

void battery_losses_params::initialize_from_data(var_table &vt, storage_time_params &t){
    time = std::make_shared<storage_time_params>(t);
    auto charging = vt.as_vector_double("batt_losses_charging");
    auto discharging = vt.as_vector_double("batt_losses_discharging");
    auto idle = vt.as_vector_double("batt_losses_idle");
    auto full = vt.as_vector_double("batt_losses");

    mode = static_cast<MODE>(vt.as_integer("batt_loss_choice"));

    switch (mode){
        case MONTHLY: {
            if (!(charging.size() == 1 || charging.size() == 12))
                throw general_error("charging loss length must be 1 or 12 for monthly input mode");
            if (!(discharging.size() == 1 || discharging.size() == 12))
                throw general_error("discharging loss length must be 1 or 12 for monthly input mode");
            if (!(idle.size() == 1 || idle.size() == 12))
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
            break;
        }
        case TIMESERIES: {
            if (full.size() != (size_t)(8760 / time->dt_hour)) {
                for (size_t i = 0; i < (size_t)(8760 / time->dt_hour); i++) {
                    losses_full.emplace_back(full[0]);
                }
            }
            else {
                losses_full = std::move(full);
            }
            break;
        }
        default:
            throw general_error("batt_loss_choice not recognized, must be 0 (monthly) or 1 (timeseries)");
    }

}

void battery_voltage_params::initialize_from_data(var_table& vt){
    num_cells_series = vt.as_integer("batt_computed_series");
    num_strings = vt.as_integer("batt_computed_strings");
    Vnom_default = vt.as_double("batt_Vnom_default");

    choice = static_cast<VOLTAGE>(vt.as_integer("batt_voltage_choice"));

    if (choice == MODEL){
        Vfull = vt.as_double("batt_Vfull");
        Vexp = vt.as_double("batt_Vexp");
        Vnom = vt.as_double("batt_Vnom");
//        Qfull_flow = vt.as_double("batt_Qfull_flow");
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
    surface_area = pow(vt.as_double("batt_length"), 2) * 6;
    Cp = vt.as_double("batt_Cp");
    resistance = vt.as_double("batt_resistance");
    h = vt.as_double("batt_h_to_ambient");
    T_room_K = vt.as_vector_double("batt_room_temperature_celsius");

    for (auto& i : T_room_K) {
        i += 273.15; // convert C to K
    }

    size_t n = cap_vs_temp.nrows();
    for (size_t i = 0; i < n; i++)
    {
        cap_vs_temp(i,0) += 273.15; // convert C to K
    }
}

void battery_capacity_params::initialize_from_data(var_table &vt, storage_time_params &t) {
    time = std::make_shared<storage_time_params>(t);
    int chem = vt.as_integer("batt_chem");

    initial_SOC = vt.as_double("batt_initial_SOC");
    maximum_SOC = vt.as_double("batt_maximum_soc");
    minimum_SOC = vt.as_double("batt_minimum_soc");

    if (chem == battery_properties_params::LEAD_ACID){
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
    chem = static_cast<CHEM>(vt.as_integer("batt_chem"));

    auto therm = new battery_thermal_params();
    therm->initialize_from_data(vt, t);
    thermal = std::shared_ptr<battery_thermal_params>(therm);

    auto volt = new battery_voltage_params();
    volt->initialize_from_data(vt);
    voltage = std::shared_ptr<battery_voltage_params>(volt);

    auto cap = new battery_capacity_params();
    cap->initialize_from_data(vt, t);
    capacity = std::shared_ptr<const battery_capacity_params>(cap);

    auto lif = new battery_lifetime_params();
    lif->initialize_from_data(vt, t);
    lifetime = std::shared_ptr<const battery_lifetime_params>(lif);

    auto los = new battery_losses_params();
    los->initialize_from_data(vt, t);
    losses = std::shared_ptr<const battery_losses_params>(los);

    if (vt.as_integer("batt_replacement_option") > 0){
        auto rep = new storage_replacement_params();
        rep->initialize_from_data(vt, true);

        if (t.system_use_lifetime_output && rep->option == storage_replacement_params::SCHEDULE){
            if (rep->replacement_per_yr_schedule.size() != t.nyears || rep->replacement_percent_per_yr_schedule.size() != t.nyears)
                throw general_error("battery replacements per year array must be same length as analysis_years.");
        }
        replacement = std::shared_ptr<storage_replacement_params>(rep);
    }
}