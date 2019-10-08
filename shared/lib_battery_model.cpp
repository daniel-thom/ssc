#include "lib_battery_model.h"


/*
Define Thermal Model
*/

void thermal_state::init(){
    R = 0.004;
    capacity_percent = 100;
}

battery_thermal::battery_thermal(const battery_thermal_params& p):
params(p){
    // assume all surfaces are exposed
    A = 2 * (p.length*p.width + p.length*p.height + p.width*p.height);
    state.init();
}

battery_thermal::battery_thermal(const battery_thermal& thermal):
params(thermal.params)
{
    A = thermal.A;
    state = thermal.get_state();
}
void battery_thermal::replace_battery(size_t lifetimeIndex)
{
    state.T_battery = params.T_room_K[util::yearOneIndex(params.time->dt_hour, lifetimeIndex)];
    state.capacity_percent = 100.;
}

#define HR2SEC 3600.0
void battery_thermal::updateTemperature(double I, const storage_time_state &time)
{
    double dt = params.time->dt_hour * HR2SEC;
    size_t idx = time.get_lifetime_index();
    if (trapezoidal(I, dt, idx) < T_max && trapezoidal(I, dt, idx) > 0)
        state.T_battery = trapezoidal(I, dt, idx);
    else if (rk4(I, dt, idx) < T_max && rk4(I, dt, idx) > 0)
        state.T_battery = rk4(I, dt, idx);
    else if (implicit_euler(I, dt, idx) < T_max && implicit_euler(I, dt, idx) > 0)
        state.T_battery = implicit_euler(I, dt, idx);
    else
        state.messages.push_back("Computed battery temperature below zero or greater than max allowed, consider reducing C-rate");
}

double battery_thermal::f(double T_battery, double I, size_t lifetimeindex)
{
    return (1 / (params.mass*params.Cp)) * ((params.h*(params.T_room_K[util::yearOneIndex(params.time->dt_hour, lifetimeindex)]  - T_battery)*A) + pow(I, 2)*params.resistance);
}
double battery_thermal::rk4(double I, double dt, size_t lifetimeindex)
{
    double k1 = dt*f(state.T_battery, I, lifetimeindex);
    double k2 = dt*f(state.T_battery + k1 / 2, I, lifetimeindex);
    double k3 = dt*f(state.T_battery + k2 / 2, I, lifetimeindex);
    double k4 = dt*f(state.T_battery + k3, I, lifetimeindex);
    return (state.T_battery + (1. / 6)*(k1 + k4) + (1. / 3.)*(k2 + k3));
}
double battery_thermal::trapezoidal(double I, double dt, size_t lifetimeindex)
{
    double B = 1 / (params.mass*params.Cp); // [K/J]
    double C = params.h*A;			// [W/K]
    double D = pow(I, 2)*params.resistance;	// [Ohm A*A]
    double T_prime = f(state.T_battery, I, lifetimeindex);	// [K]

    return (state.T_battery + 0.5*dt*(T_prime + B*(C*params.T_room_K[util::yearOneIndex(params.time->dt_hour, lifetimeindex)] + D))) / (1 + 0.5*dt*B*C);
}
double battery_thermal::implicit_euler(double I, double dt, size_t lifetimeIndex)
{
    double B = 1 / (params.mass*params.Cp); // [K/J]
    double C = params.h*A;			// [W/K]
    double D = pow(I, 2)*params.resistance;	// [Ohm A*A]
//	double T_prime = f(state.get_T_battery, I);	// [K]

    return (state.T_battery + dt*(B*C*params.T_room_K[util::yearOneIndex(params.time->dt_hour, lifetimeIndex)] + D)) / (1 + dt*B*C);
}
double battery_thermal::get_T_battery(){ return state.T_battery; }
double battery_thermal::get_capacity_percent()
{
    double percent = util::linterp_col(params.cap_vs_temp, 0, state.T_battery, 1);

    if (percent < 0 || percent > 100)
    {
        percent = 100;
        state.messages.push_back("Unable to determine capacity adjustment for temperature, ignoring");
    }
    state.capacity_percent = percent;
    return state.capacity_percent;
}

/*
Define Losses
*/
battery_losses::battery_losses(const battery_losses_params& p):
params(p){
}

battery_losses::battery_losses(const battery_losses & losses):
params(losses.params),
loss_ts(losses.loss_ts){
}

double battery_losses::getLoss(size_t indexFirstYear) { return loss_ts[indexFirstYear]; }

void battery_losses::run_losses(const storage_time_state &time, const capacity_state &cap)
{
    size_t indexYearOne = util::yearOneIndex(params.time->dt_hour, time.get_lifetime_index());
    auto hourOfYear = (size_t)std::floor(indexYearOne * params.time->dt_hour);
    size_t monthIndex = util::month_of((double)(hourOfYear)) - 1;

    // update system losses depending on user input
    if (params.loss_monthly_or_timeseries == storage_params::LOSSES::MONTHLY) {
        if (cap.charge_mode == capacity_state::CHARGE)
            loss_ts[indexYearOne] = params.losses_charging[monthIndex];
        if (cap.charge_mode == capacity_state::DISCHARGE)
            loss_ts[indexYearOne] = params.losses_discharging[monthIndex];
        if (cap.charge_mode == capacity_state::NO_CHARGE)
            loss_ts[indexYearOne] = params.losses_idle[monthIndex];
    }
}

/*
Define Battery
*/
battery::battery(const battery_properties_params& p):
params(p)
{
    if (p.chem != storage_params::CHEMS::LEAD_ACID) {
        capacity = new capacity_lithium_ion(p.capacity_vars);
    }
    else {
        capacity = new capacity_kibam(p.capacity_vars);
    }

    if (p.voltage_vars.voltage_choice == 0){
        if (p.chem == storage_params::CHEMS::VANADIUM_REDOX){
            voltage = new voltage_vanadium_redox(p.voltage_vars);
        }
        else if (p.chem == storage_params::CHEMS::LEAD_ACID || p.chem == storage_params::CHEMS::LITHIUM_ION){
            voltage = new voltage_dynamic(p.voltage_vars);
        }
    }
    else{
        voltage = new voltage_table(p.voltage_vars);
    }

    lifetime = new battery_lifetime(p.lifetime);
    thermal = new battery_thermal(p.thermal);
    losses = new battery_losses(p.losses);

    state.last_idx = 0;
}

battery::battery(const battery& battery):
params(battery.params),
state(battery.state),
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
    losses->set_loss_ts(s.loss_ts_ptr);
    state.last_idx = s.last_idx;
}

battery_state battery::get_state(){
    battery_state s;
    s.capacity = capacity->get_state();
    s.batt_voltage = voltage->get_battery_voltage();
    s.lifetime = lifetime->get_state();
    s.thermal = thermal->get_state();
    s.loss_ts_ptr = losses->get_loss_ts();
    s.last_idx = state.last_idx;
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
    double I = P/state.batt_voltage;
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
    voltage->updateVoltage(capacity->get_state(), thermal->get_state().T_battery);
}

void battery::run_lifetime_model(const storage_time_state &time)
{
    lifetime->runLifetimeModels(time, state.capacity, thermal_model()->get_T_battery());
}
void battery::run_losses_model(const storage_time_state &time)
{
    if (time.get_index() > state.last_idx || time.get_index() == 0)
    {
        losses->run_losses(time, capacity->get_state());
        state.last_idx = time.get_index();
    }
}
battery_capacity_interface * battery::capacity_model() const { return capacity; }
battery_voltage_interface * battery::voltage_model() const { return voltage; }
battery_lifetime * battery::lifetime_model() const { return lifetime; }
battery_thermal * battery::thermal_model() const { return thermal; }
battery_losses * battery::losses_model() const { return losses; }

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
    return (this->get_battery_energy_to_fill(SOC_max) / params.capacity_vars->time->dt_hour);
}

double battery::battery_charge_maximum(){ return capacity->get_state().qmax; }
double battery::battery_charge_maximum_thermal() { return capacity->get_state().qmax_thermal; }
double battery::battery_voltage(){ return voltage->get_battery_voltage();}
double battery::battery_voltage_nominal(){ return voltage->get_battery_voltage_nominal(); }
double battery::battery_soc(){ return capacity->get_state().SOC; }
