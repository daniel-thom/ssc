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
    t_threshold = -params->mass * params->Cp / params->surface_area / params->h * log(10) + dt_sec;
}

battery_thermal::battery_thermal(const battery_thermal& thermal):
params(thermal.params),
state(thermal.state),
dt_sec(thermal.dt_sec),
t_threshold(thermal.t_threshold)
{
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

    size_t monthIndex = util::month_of((double)(time.get_hour_year())) - 1;

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
battery::battery(const std::shared_ptr<const battery_properties_params>& p):
lifetime(new battery_lifetime(p->lifetime)),
thermal(new battery_thermal(p->thermal)),
losses(new battery_losses(p->losses)),
params(p)
{
    if (p->chem != battery_properties_params::LEAD_ACID) {
        capacity = new capacity_lithium_ion(p->capacity);
    }
    else {
        capacity = new capacity_kibam(p->capacity);
    }

    if (p->voltage->choice == battery_voltage_params::MODEL){
        if (p->chem == battery_properties_params::VANADIUM_REDOX){
            voltage = new voltage_vanadium_redox(p->voltage);
        }
        else if (p->chem == battery_properties_params::LEAD_ACID || p->chem == battery_properties_params::LITHIUM_ION){
            voltage = new voltage_dynamic(p->voltage);
        }
    }
    else{
        voltage = new voltage_table(p->voltage);
    }
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
}

void battery::set_state(const battery_state &s) {
    capacity->set_state(s.capacity);
    voltage->set_batt_voltage(s.batt_voltage);
    lifetime->set_state(s.lifetime);
    thermal->set_state(s.thermal);
    replacements = s.replacements;
}

battery_state battery::get_state(){
    battery_state s;
    s.capacity = capacity->get_state();
    s.batt_voltage = voltage->get_battery_voltage();
    s.lifetime = lifetime->get_state();
    s.thermal = thermal->get_state();
    s.replacements = replacements;
    return s;
}

void battery::run(const storage_time_state& time, double I_guess)
{
    if (params->replacement)
        run_replacement(time);

    // Temperature affects capacity, but capacity model can reduce current, which reduces temperature, need to iterate
    double I_initial = I_guess;
    size_t iterate_count = 0;
    auto capacity_initial = capacity->get_state();
    auto thermal_initial = thermal->get_state();

    while (iterate_count < 5)
    {
        run_thermal_model(I_guess, time);
        run_capacity_model(I_guess);

        if (fabs(I_guess - I_initial) / fabs(I_initial) > tolerance)
        {
            thermal->set_state(thermal_initial);
            capacity->set_state(capacity_initial);
            I_initial = I_guess;
            iterate_count++;
        }
        else {
            break;
        }

    }
    run_voltage_model();
    run_lifetime_model(time.get_lifetime_index());
    capacity->updateCapacityForLifetime(lifetime->get_capacity_percent());
    run_losses_model(time);
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
    voltage->updateVoltage(capacity->get_I(), capacity->get_q1(), capacity->get_q10(), thermal->get_T_battery());
}

void battery::run_lifetime_model(const size_t &lifetime_index)
{
    lifetime->runLifetimeModels(lifetime_index, capacity->get_SOC(), capacity->get_charge_changed(),
                                thermal->get_T_battery());
}
void battery::run_losses_model(const storage_time_state &time)
{
    losses->getLoss(time, capacity->get_charge_mode());
    // what will be done with these losses?
}

void battery::run_replacement(const storage_time_state& time){
    bool replace = false;
    if (time.get_year() < params->replacement->replacement_per_yr_schedule.size())
    {
        auto num_repl = (size_t)params->replacement->replacement_per_yr_schedule[time.get_year()];
        for (size_t j_repl = 0; j_repl < num_repl; j_repl++)
        {
            if ((time.get_hour_year() == (j_repl * 8760 / num_repl)) && time.get_index() == 0)
            {
                replace = true;
                break;
            }
        }
    }
    bool replaced = false;
    double q = lifetime->get_capacity_percent();
    double replacement_percent = 0;
    if ((params->replacement->option == storage_replacement_params::CAPACITY
        && (q - tolerance) <= params->replacement->replacement_capacity) || replace)
    {
        replacements++;

        replacement_percent = params->replacement->replacement_percent_per_yr_schedule[time.get_year()];
        q += replacement_percent;

        // for now, only allow augmenting up to original installed capacity
        q = fmin(100., q);

        replaced = true;

        lifetime->replaceBattery(replacement_percent);

        assert(lifetime->get_capacity_percent() == q);
    }

    if (replaced)
    {
        capacity->replace_battery(replacement_percent);
        thermal->replace_battery();
    }
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
    double battery_voltage = this->get_V_nom(); // [V]
    double charge_needed_to_fill = this->get_battery_charge_needed(SOC_max); // [Ah] - qmax - q0
    return (charge_needed_to_fill * battery_voltage)*util::watt_to_kilowatt;  // [kWh]
}
double battery::get_battery_energy_nominal()
{
    return get_V_nom() * capacity->get_state().qmax * util::watt_to_kilowatt;
}
double battery::get_battery_power_to_fill(double SOC_max)
{
    // in one time step
    return (this->get_battery_energy_to_fill(SOC_max) / params->capacity->time->dt_hour);
}

double battery::battery_charge_maximum(){ return capacity->get_qmax(); }
double battery::battery_charge_maximum_thermal() { return capacity->get_qmax_thermal(); }
double battery::get_V_nom(){ return voltage->get_battery_voltage_nominal(); }
double battery::get_SOC(){ return capacity->get_SOC(); }
