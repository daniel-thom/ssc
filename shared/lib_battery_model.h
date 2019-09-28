#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H

#include "lib_util.h"

#include "lib_storage_params.h"
#include "lib_battery_capacity.h"
#include "lib_battery_voltage.h"
#include "lib_battery_lifetime.h"

// Messages
class message
{
public:
    message(){};
    virtual ~message(){};


    void add(std::string message);
    size_t total_message_count();
    size_t message_count(int index);
    std::string get_message(int index);
    std::string construct_log_count_string(int index);

protected:
    std::vector<std::string> messages;
    std::vector<int> count;
};

/*
Thermal classes
*/
struct thermal_state{
    double R;			// [Ohm] - internal resistance
    double T_battery;   // [K]
    double capacity_percent; //[%]
    message message;

    void init();
};

class thermal_t
{
public:
    thermal_t(const battery_thermal_params& p);

    thermal_t(const thermal_t &);

    static constexpr const double T_max = 400;

    void updateTemperature(double I, const storage_state &time);
    void replace_battery(size_t lifetimeIndex);

    // outputs
    double get_T_battery();
    double get_capacity_percent();
    message get_messages(){ return state.message; }

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
* \class losses_t
*
* \brief
*
*  The Battery losses class takes generic losses which occur during charging, discharge, or idle operation modes:
*  The model also accepts a time-series vector of losses defined for every time step of the first year of simulation
*  which may be used in lieu of the losses for operational mode.
*/

class losses_t
{
public:

    losses_t(const battery_losses_params& p);

    /// Copy input losses to this object
    losses_t(const losses_t&);

    /// Run the losses model at the present simulation index (for year 1 only)
    void run_losses(const storage_state &time, const capacity_state &cap);

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

class battery_t
{
public:
    battery_t(const battery_properties_params& prop);

    battery_t(const battery_t& battery);

    ~battery_t();

    // Run all for single time step
    void run(const storage_state& time, double I);

    void set_state(const battery_state& state);
    battery_state get_state();

    capacity_interface * capacity_model() const;
    voltage_interface * voltage_model() const;
    lifetime_t * lifetime_model() const;
    thermal_t * thermal_model() const;
    losses_t * losses_model() const;

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

    capacity_interface * capacity;
    thermal_t * thermal;
    lifetime_t * lifetime;
    voltage_interface * voltage;
    losses_t * losses;

    // Run a component level model
    void runCapacityModel(double &I);
    void runVoltageModel();
    void runThermalModel(double I, const storage_state &time);
    void runLifetimeModel(const storage_state &time);
    void runLossesModel(const storage_state &time);

};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
