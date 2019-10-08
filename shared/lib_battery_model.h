#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H

#include "lib_util.h"

#include "lib_storage_params.h"
#include "lib_battery_capacity.h"
#include "lib_battery_voltage.h"
#include "lib_battery_lifetime.h"

/*
Thermal classes
*/
struct thermal_state{
    double R;			// [Ohm] - internal resistance
    double T_battery;   // [K]
    double capacity_percent; //[%]
    std::vector<std::string> messages;

    void init();
};

class battery_thermal
{
public:
    battery_thermal(const battery_thermal_params& p);

    battery_thermal(const battery_thermal &);

    static constexpr const double T_max = 400;

    void updateTemperature(double I, const storage_time_state &time);
    void replace_battery(size_t lifetimeIndex);

    // outputs
    double get_T_battery();
    double get_capacity_percent();

    thermal_state get_state() const {return state;}
    void set_state(const thermal_state& s) {state = s;}

    battery_thermal_params get_params() const {return params; }

private:
    double f(double T_battery, double I, size_t lifetimeIndex);
    double rk4(double I, double dt, size_t lifetimeIndex);
    double trapezoidal(double I, double dt, size_t lifetimeIndex);
    double implicit_euler(double I, double dt, size_t lifetimeIndex);

    void init();

    const battery_thermal_params params;

    thermal_state state;

    double A;                                           // area assumes all surfaces exposed
};

/**
* \class losses
*
* \brief
*
*  The Battery losses class takes generic losses which occur during charging, discharge, or idle operation modes:
*  The model also accepts a time-series vector of losses defined for every time step of the first year of simulation
*  which may be used in lieu of the losses for operational mode.
*/

class battery_losses
{
public:

    battery_losses(const battery_losses_params& p);

    /// Copy input losses to this object
    battery_losses(const battery_losses&);

    /// Run the losses model at the present simulation index (for year 1 only)
    void run_losses(const storage_time_state &time, const capacity_state &cap);

    battery_losses_params get_params() const {return params;}

    /// Get the loss at the specified simulation index (year 1)
    double getLoss(size_t indexFirstYear);

    std::vector<double> get_loss_ts() const { return loss_ts; }
    void set_loss_ts(const std::vector<double> l) {loss_ts = l;}

protected:

    const battery_losses_params params;

    std::vector<double> loss_ts;
};

/*
Class which encapsulates a battery and all its models
*/

struct battery_state{
    capacity_state capacity;
    double batt_voltage;
    lifetime_state lifetime;
    thermal_state thermal;
    std::vector<double> loss_ts_ptr;

    size_t last_idx;
};

class battery
{
public:
    battery(const battery_properties_params& prop);

    battery(const battery& battery);

    ~battery();

    // Run all for single time step
    void run(const storage_time_state& time, double& I);

    // Compute how battery system to modification of a single state variable
    void change_power(const double P);

    void set_state(const battery_state& state);
    battery_state get_state();

    battery_capacity_interface * capacity_model() const;
    battery_voltage_interface * voltage_model() const;
    battery_lifetime * lifetime_model() const;
    battery_thermal * thermal_model() const;
    battery_losses * losses_model() const;

    // Get capacity quantities
    double get_battery_charge_needed(double SOC_max);
    double battery_charge_maximum();
    double battery_charge_maximum_thermal();
    double get_battery_energy_nominal();
    double get_battery_energy_to_fill(double SOC_max);
    double get_battery_power_to_fill(double SOC_max);
    double battery_soc();

    // Get Voltage
    double battery_voltage(); // the actual battery voltage
    double battery_voltage_nominal(); // the nominal battery voltage

private:

    battery_state state;

    const battery_properties_params& params;

    battery_capacity_interface * capacity;
    battery_thermal * thermal;
    battery_lifetime * lifetime;
    battery_voltage_interface * voltage;
    battery_losses * losses;

    // Run a component level model
    void run_capacity_model(double &I);
    void run_voltage_model();
    void run_thermal_model(double I, const storage_time_state &time);
    void run_lifetime_model(const storage_time_state &time);
    void run_losses_model(const storage_time_state &time);



};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
