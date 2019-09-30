
#include "lib_storage.h"

#include "lib_battery.h"
#include "lib_battery_dispatch.h"
#include "lib_power_electronics.h"
#include "lib_utility_rate.h"

#include "vartab.h"

storage_interface::storage_interface():
    storage_type(-1){

}

storage_interface::~storage_interface() {
}

storage_interface* storage_interface::Create(int storage_type) {
    switch (storage_type){
        case BATT_BTM_AUTO: return new battery_BTM_automated();
        case BATT_BTM_MAN: return new battery_BTM_manual();
        case BATT_BTM_CSTM: return new battery_BTM_custom();
        case BATT_FOM_AUTO: return new battery_FOM_automated();
        case BATT_FOM_MAN: return new battery_FOM_manual();
        case BATT_FOM_CSTM: return new battery_FOM_custom();
        case FUELCELL: return new fuelcell();
        default: return nullptr;
    }
}

void storage_interface::storage_from_data(var_table &vt) {
    auto time_params = const_cast<storage_time_params*>(&time);
    initialize_from_data(vt);

    auto forecast_params = const_cast<storage_forecast*>(&forecast);
    initialize_from_data(vt);

    // state reset
    auto state_params = const_cast<storage_state*>(&state);
    state_params->year = 0;
    state_params->hour = 0;
    state_params->step = 0;
    state_params->index = 0;
    state_params->year_index = 0;
}

battery::battery():
        storage_interface(),
        properties(battery_properties_params()),
        losses(battery_losses_params()),
        lifetime_batt(battery_lifetime_params()),
        power_outputs(battery_power_outputs()),
        battery_model(nullptr),
        battery_metrics(nullptr),
        charge_control(nullptr){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->is_batt = true;
}

void battery::battery_from_data(var_table &vt) {
    storage_interface::storage_from_data(vt);

    auto lifetime_params = const_cast<battery_lifetime_params*>(&lifetime_batt);
    battery_lifetime_params_from_data(lifetime_params, vt, 0);

    auto losses_params = const_cast<battery_losses_params*>(&losses);
    initialize_from_data(losses_params, vt);
    if (losses_params->loss_monthly_or_timeseries == storage_params::LOSSES::TIMESERIES
        && !(losses_params->losses.size() == 1 || losses_params->losses.size() == time.step_per_year)) {
        throw general_error("system loss input length must be 1 or equal to weather file length for time series input mode");
    }

    auto properties_params = const_cast<battery_properties_params*>(&properties);
    initialize_from_data(properties_params, vt);

    initialize_models();
}

void battery_models::initialize_models(){
    if (dispatch_model == nullptr){
        throw general_error("dispatch_model must be initialized before calling battery::initialize_models");
    }

    battery_model = new battery_t(time.dt_hour, &properties, &lifetime_batt, &losses);
    battery_metrics = new battery_metrics_t(time.dt_hour);

    if (properties.connection == storage_params::CONNECTION::DC_CONNECTED){
        charge_control = new DCBatteryController(dispatch_model, battery_metrics,
                                                 properties.dc_dc_bms_efficiency, properties.inverter.inverter_efficiency_cutoff);
    }
    else if (properties.connection == storage_params::CONNECTION::AC_CONNECTED){
        charge_control = new ACBatteryController(dispatch_model, battery_metrics,
                                                 properties.ac_dc_efficiency, properties.dc_ac_efficiency);
    }
    else{
        throw general_error("Battery connection "  + to_string(properties.connection) + " must be 0 or 1");
    }
}

battery_FOM::battery_FOM():
battery(),
FOM(storage_FOM_params())
{
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->meter_position = storage_params::METERING::FRONT;
}

void battery_FOM::battery_FOM_from_data(var_table &vt){
    auto FOM_params = const_cast<storage_FOM_params *>(&FOM);
    storage_FOM_params_from_data(FOM_params, vt, time.step_per_hour);
}

battery_FOM_automated::battery_FOM_automated():
battery_FOM(),
automated_dispatch(storage_automated_dispatch_params()),
look_ahead_hours(0),
dispatch_update_frequency_hours(-1){
}

void battery_FOM_automated::battery_FOM_automated_from_data(var_table &vt) {
    battery_FOM::battery_FOM_from_data(vt);

    if (vt.is_assigned("en_electricity_rates")) {
        bool ec_use_realtime;
        util::matrix_t<size_t> ec_weekday_schedule;
        util::matrix_t<size_t> ec_weekend_schedule;
        util::matrix_t<double> ec_tou_matrix;
        std::vector<double> ec_realtime_buy;
        if (vt.as_integer("en_electricity_rates"))
        {
            ec_use_realtime = vt.as_boolean("ur_en_ts_sell_rate");
            if (!ec_use_realtime) {
                ec_weekday_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekday");
                ec_weekend_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekend");
                ec_tou_matrix = vt.as_matrix("ur_ec_tou_mat");
                utilityRate = new UtilityRate(std::move(ec_weekday_schedule), std::move(ec_weekend_schedule),
                        std::move(ec_tou_matrix));
            }
            else {
                ec_realtime_buy = vt.as_vector_double("ur_ts_buy_rate");
                utilityRate = new UtilityRate(std::move(ec_realtime_buy));
            }
        }
        else {
            ec_realtime_buy = FOM.ppa_price_series_dollar_per_kwh;
            utilityRate = new UtilityRate(std::move(ec_realtime_buy));
        }
    }
    look_ahead_hours = vt.as_unsigned_long("batt_look_ahead_hours");
    dispatch_update_frequency_hours = vt.as_double("batt_dispatch_update_frequency_hours");
}

void battery_FOM_automated::initialize_models(){
    dispatch_model = dispatch_automatic_front_of_meter_t()

    battery::initialize_models();
}

void battery_FOM_automated::from_data(var_table &vt) {
    battery_FOM_automated_from_data(vt);
    initialize_models();
}

battery_FOM_manual::battery_FOM_manual():
battery_FOM(),
manual_dispatch(storage_manual_dispatch_params()){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->dispatch = storage_params::FOM_MODES::FOM_MANUAL;
}

void battery_FOM_manual::battery_FOM_manual_from_data(var_table &vt){
    battery_FOM::battery_FOM_from_data(vt);
    auto params = const_cast<storage_manual_dispatch_params*>(&manual_dispatch);
    storage_manual_dispatch_params_from_data(params, vt);
}


void battery_FOM_manual::from_data(var_table &vt) {
    battery_FOM_manual_from_data(vt);
    initialize_models();
}

battery_FOM_custom::battery_FOM_custom():
battery_FOM(){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->dispatch = storage_params::FOM_MODES::FOM_CUSTOM_DISPATCH;
}

void battery_FOM_custom::battery_FOM_custom_from_data(var_table &vt){
    battery_FOM::battery_FOM_from_data(vt);
    custom_dispatch_kw = vt.as_vector_double("batt_custom_dispatch");
}

void battery_FOM_custom::from_data(var_table &vt) {
    battery_FOM_custom_from_data(vt);
}

battery_BTM::battery_BTM():
battery(){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->meter_position = storage_params::METERING::BEHIND;
}

void battery_BTM::battery_BTM_from_data(var_table &vt) {
    battery_from_data(vt);
}

battery_BTM_automated::battery_BTM_automated():
battery_BTM(),
automated_dispatch(storage_automated_dispatch_params()){
}

void battery_BTM_automated::battery_BTM_automated_from_data(var_table &vt) {
    battery_BTM_from_data(vt);
    auto params = const_cast<storage_automated_dispatch_params *>(&automated_dispatch);
    storage_automated_dispatch_params_from_data(params, vt);

    if (config.dispatch == storage_params::BTM_MODES ::MAINTAIN_TARGET)
    {
        int batt_target_choice = vt.as_integer("batt_target_choice");
        auto target_power_monthly = vt.as_vector_double("batt_target_power_monthly");
        target_power = vt.as_vector_double("batt_target_power");

        if (batt_target_choice == storage_params::BTM_TARGET_MODES::TARGET_SINGLE_MONTHLY)
        {
            target_power.clear();
            target_power.reserve(8760 * time.step_per_hour);
            for (size_t month = 0; month != 12; month++)
            {
                double target = target_power_monthly[month];
                for (size_t h = 0; h != util::hours_in_month(month + 1); h++)
                {
                    for (size_t s = 0; s != time.step_per_hour; s++)
                        target_power.push_back(target);
                }
            }
        }

        if (target_power.size() != time.step_per_year)
            throw general_error("invalid number of target powers, must be equal to number of records in weather file");

        // extend target power to lifetime internally
        for (size_t y = 1; y < time.nyears; y++) {
            for (size_t i = 0; i < time.step_per_year; i++) {
                target_power.push_back(target_power[i]);
            }
        }
    }
}

void battery_BTM_automated::from_data(var_table &vt) {
    battery_BTM_automated_from_data(vt);
}

battery_BTM_manual::battery_BTM_manual():
battery_BTM(),
manual_dispatch(storage_manual_dispatch_params()){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->dispatch = storage_params::BTM_MODES::MANUAL;
}

void battery_BTM_manual::battery_BTM_manual_from_data(var_table &vt) {
    battery_BTM_from_data(vt);
    auto params = const_cast<storage_manual_dispatch_params *>(&manual_dispatch);
    storage_manual_dispatch_params_from_data(params, vt);
}

void battery_BTM_manual::from_data(var_table &vt) {
    battery_BTM_manual_from_data(vt);
}

battery_BTM_custom::battery_BTM_custom():
battery_BTM(){
    auto config_params = const_cast<storage_config_params *>(&config);
    config_params->dispatch = storage_params::BTM_MODES::CUSTOM_DISPATCH;
}

void battery_BTM_custom::battery_BTM_custom_from_data(var_table& vt){
    battery_BTM_from_data(vt);
    custom_dispatch_kw = vt.as_vector_double("batt_custom_dispatch");
}

void battery_BTM_custom::from_data(var_table &vt) {
    battery_BTM_custom_from_data(vt);
}

fuelcell::fuelcell():
        storage_interface(),
        replacement(storage_replacement_params()),
        fuelcell_outputs(fuelcell_power_outputs())
{}

void fuelcell::fuelcell_from_data(var_table& vt){

}


void fuelcell::from_data(var_table &vt) {

}