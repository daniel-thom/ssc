#include "lib_battery_model.h"


/*
Define Thermal Model
*/

battery_thermal::battery_thermal(const std::shared_ptr<const battery_thermal_params>& p):
params(p),
dt_sec(params->time->dt_hour * 3600){

    state.capacity_percent = 100;

    state.T_room_init = params->T_room_K[0];
    state.T_batt_init = state.T_room_init;
    state.T_batt_avg = state.T_room_init;

    state.next_time_at_current_T_room = dt_sec;

    // exp(-A*h*t/m/Cp) < tol
    t_threshold = -params->mass * params->Cp / params->surface_area / params->h * log(tolerance) + dt_sec;
}

battery_thermal::battery_thermal(const battery_thermal& thermal):
params(thermal.params)
{
    state = thermal.state;
}
void battery_thermal::replace_battery()
{
    state.T_room_init = 0;
    state.T_batt_avg = state.T_room_init;
    state.capacity_percent = 100.;
}

void battery_thermal::updateTemperature(double I, const storage_time_state &time)
{
    double T_room = params->T_room_K[time.get_lifetime_index() % params->T_room_K.size()];
    double source = pow(I,2) * params->resistance/params->surface_area/params->h + T_room;

    // if initial temp of batt will change, old initial conditions are invalid and need new T(t) piece
    if (abs(T_room - state.T_room_init) > tolerance){
        state.next_time_at_current_T_room = dt_sec;
        state.T_room_init = T_room;
        state.T_batt_init = state.T_batt_avg;

        // then the battery temp is the average temp over that step
        double diffusion = exp(-params->surface_area * params->h * state.next_time_at_current_T_room / params->mass / params->Cp);
        double coeff_avg = params->mass * params->Cp / params->surface_area / params->h / state.next_time_at_current_T_room;
        state.T_batt_avg = (state.T_batt_init - state.T_room_init) * coeff_avg * (1 - diffusion) + source;
    }
        // if initial temp of batt will not be changed, then continue with previous T(t) piece
    else  {
        // otherwise, given enough time, the diffusion term is negligible so the temp comes from source only
        if (state.next_time_at_current_T_room > t_threshold){
            state.T_batt_avg = source;
        }
            // battery temp is still adjusting to room
        else{
            double diffusion = exp(-params->surface_area * params->h * state.next_time_at_current_T_room / params->mass / params->Cp);
            state.T_batt_avg = (state.T_batt_init - state.T_room_init) * diffusion + source;
        }
    }
    state.next_time_at_current_T_room += dt_sec;

    double percent = util::linterp_col(params->cap_vs_temp, 0, state.T_batt_avg, 1);

    if (percent < 0 || percent > 100)
    {
        percent = 100;
    }
    state.capacity_percent = percent;
}

double battery_thermal::get_T_battery(){ return state.T_batt_avg; }
double battery_thermal::get_capacity_percent(){return state.capacity_percent;}

/*
Define Losses
*/
battery_losses::battery_losses(const std::shared_ptr<const battery_losses_params>& p):
params(p){
}

double battery_losses::getLoss(const storage_time_state &time, const size_t &charge_mode) {

    size_t monthIndex = util::month_of((double)(time.get_hour())) - 1;

    // update system losses depending on user input
    if (params->mode == battery_losses_params::MONTHLY) {
        if (charge_mode == capacity_state::CHARGE)
            return params->losses_charging[monthIndex];
        if (charge_mode == capacity_state::DISCHARGE)
            return params->losses_discharging[monthIndex];
        if (charge_mode == capacity_state::NO_CHARGE)
            return params->losses_idle[monthIndex];
    }
    else
        return params->losses_full[time.get_index()];
}


/*
Define Battery
*/
battery::battery(const battery_properties_params& p):
lifetime(new battery_lifetime(p.lifetime)),
thermal(new battery_thermal(p.thermal)),
losses(new battery_losses(p.losses)),
params(p)
{
    if (p.chem != battery_properties_params::LEAD_ACID) {
        capacity = new capacity_lithium_ion(p.capacity);
    }
    else {
        capacity = new capacity_kibam(p.capacity);
    }

    if (p.voltage->choice == 0){
        if (p.chem == battery_properties_params::VANADIUM_REDOX){
            voltage = new voltage_vanadium_redox(p.voltage);
        }
        else if (p.chem == battery_properties_params::LEAD_ACID || p.chem == battery_properties_params::LITHIUM_ION){
            voltage = new voltage_dynamic(p.voltage);
        }
    }
    else{
        voltage = new voltage_table(p.voltage);
    }
    last_idx = 0;
}

battery::battery(const battery& battery):
params(battery.params),
capacity(battery.capacity),
voltage(battery.voltage),
thermal(battery.thermal),
lifetime(battery.lifetime),
losses(battery.losses){
}

battery::~battery()
{
    delete capacity;
    delete voltage;
    delete lifetime;
    delete thermal;
    delete losses;
}

void battery::set_state(const battery_state &s) {
    capacity->set_state(s.capacity);
    voltage->set_batt_voltage(s.batt_voltage);
    lifetime->set_state(s.lifetime);
    thermal->set_state(s.thermal);
    last_idx = s.last_idx;
}

battery_state battery::get_state(){
    battery_state s;
    s.capacity = capacity->get_state();
    s.batt_voltage = voltage->get_battery_voltage();
    s.lifetime = lifetime->get_state();
    s.thermal = thermal->get_state();
    s.last_idx = last_idx;
    return s;
}

void battery::run(const storage_time_state& time, double& I)
{
    // Temperature affects capacity, but capacity model can reduce current, which reduces temperature, need to iterate
    double I_initial = I;
    size_t iterate_count = 0;
    auto capacity_initial = capacity->get_state();
    auto thermal_initial = thermal->get_state();

    while (iterate_count < 5)
    {
        run_thermal_model(I, time);
        run_capacity_model(I);

        if (fabs(I - I_initial)/fabs(I_initial) > tolerance)
        {
            thermal->set_state(thermal_initial);
            capacity->set_state(capacity_initial);
            I_initial = I;
            iterate_count++;
        }
        else {
            break;
        }

    }
    run_voltage_model();
    run_lifetime_model(time);
    capacity->updateCapacityForLifetime(lifetime->get_capacity_percent());
    run_losses_model(time);
    I = capacity->get_state().I;
}

void battery::change_power(const double P){
    double I = P/voltage->get_battery_voltage();
    if (I == 0){
    }
}

void battery::run_thermal_model(double I, const storage_time_state &time)
{
    thermal->updateTemperature(I, time);
}

void battery::run_capacity_model(double &I)
{
    // Don't update max capacity if the battery is idle
    if (fabs(I) > tolerance) {
        // Need to first update capacity model to ensure temperature accounted for
        capacity->updateCapacityForThermal(thermal->get_capacity_percent());
    }
    capacity->updateCapacity(I);
}

void battery::run_voltage_model()
{
    voltage->updateVoltage(capacity->get_state(), thermal->get_state().T_batt_avg);
}

void battery::run_lifetime_model(const storage_time_state &time)
{
    lifetime->runLifetimeModels(time, capacity->get_state(), thermal->get_T_battery());
}
void battery::run_losses_model(const storage_time_state &time)
{
    if (time.get_index() > last_idx || time.get_index() == 0)
    {
        losses->getLoss(time.get_hour(), capacity->get_charge_mode());
        last_idx = time.get_index();
    }
    // what will be done with these losses?
}

double battery::get_battery_charge_needed(double SOC_max)
{
    double charge_needed = capacity->get_state().qmax_thermal * SOC_max * 0.01 - capacity->get_state().q0;
    if (charge_needed > 0)
        return charge_needed;
    else
        return 0.;
}
double battery::get_battery_energy_to_fill(double SOC_max)
{
    double battery_voltage = this->battery_voltage_nominal(); // [V]
    double charge_needed_to_fill = this->get_battery_charge_needed(SOC_max); // [Ah] - qmax - q0
    return (charge_needed_to_fill * battery_voltage)*util::watt_to_kilowatt;  // [kWh]
}
double battery::get_battery_energy_nominal()
{
    return battery_voltage_nominal() * capacity->get_state().qmax * util::watt_to_kilowatt;
}
double battery::get_battery_power_to_fill(double SOC_max)
{
    // in one time step
    return (this->get_battery_energy_to_fill(SOC_max) / params.capacity->time->dt_hour);
}

double battery::battery_charge_maximum(){ return capacity->get_state().qmax; }
double battery::battery_charge_maximum_thermal() { return capacity->get_state().qmax_thermal; }
double battery::battery_voltage_nominal(){ return voltage->get_battery_voltage_nominal(); }
double battery::get_SOC(){ return capacity->get_state().SOC; }
