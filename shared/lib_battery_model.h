#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H

#include "lib_util.h"

#include "lib_storage_params.h"
#include "lib_battery_capacity.h"
#include "lib_battery_voltage.h"
#include "lib_battery_lifetime.h"

struct thermal_state{
    double capacity_percent; //[%]
    double T_batt_avg;       // avg during timestep [K]
    double T_batt_init;
    double T_room_init;
    double next_time_at_current_T_room;
};

class battery_thermal
{
public:
    battery_thermal(const std::shared_ptr<const battery_thermal_params>& p);

    battery_thermal(const battery_thermal &);

    static constexpr const double T_max = 400;

    void updateTemperature(double I, const storage_time_state &time);
    void replace_battery();

    // outputs
    double get_T_battery();
    double get_capacity_percent();

    thermal_state get_state() {return state;}
    void set_state(const thermal_state& s) {state = s;}

    std::shared_ptr<const battery_thermal_params> get_params() const {return params; }

private:
    void init();

    const std::shared_ptr<const battery_thermal_params> params;

    thermal_state state;

    double dt_sec;
    // time after which the diffusion term is negligible so the temp comes from source only
    double t_threshold;
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

    battery_losses(const std::shared_ptr<const battery_losses_params>& p);

    std::shared_ptr<const battery_losses_params> get_params() const {return params;}

    /// Get the loss at the specified simulation index (year 1)
    double getLoss(const storage_time_state &time, const size_t &charge_mode);

protected:

    const std::shared_ptr<const battery_losses_params> params;

};

/*
Class which encapsulates a battery and all its models
*/

struct battery_state{
    capacity_state capacity;
    double batt_voltage;
    lifetime_state lifetime;
    thermal_state thermal;
    size_t replacements;
};

class battery
{
public:
    battery(const std::shared_ptr<const battery_properties_params>& prop);

    battery(const battery& battery);

    ~battery();

    // Run all for single time step
    void run(const storage_time_state& time, double I_guess);

    // Compute how battery system to modification of a single state variable
    void change_power(const double P);

    double estimateCycleDamage(){
        return lifetime->estimateCycleDamage();
    }

    void reset_replacements() {replacements = 0;}

    void set_state(const battery_state& state);
    battery_state get_state();

    // Get capacity quantities
    double get_I(){return capacity->get_I();}
    double get_charge_mode(){return capacity->get_charge_mode();}
    double get_SOC();
    double get_q0(){return capacity->get_q1();}
    double get_q2(){return capacity->get_q2();}
    double get_qmax_thermal(){return capacity->get_qmax_thermal();}
    double get_qmax_lifetime(){return capacity->get_qmax();}

    double get_V(){return voltage->get_battery_voltage();}
    double get_V_cell(){return voltage->get_cell_voltage();}
    double get_V_nom(); // the nominal battery voltage

    double get_battery_charge_needed(double SOC_max);
    double battery_charge_maximum();
    double battery_charge_maximum_thermal();
    double get_battery_energy_nominal();
    double get_battery_energy_to_fill(double SOC_max);
    double get_battery_power_to_fill(double SOC_max);

    double get_T_K(){return thermal->get_T_battery();}

    double get_capacity_percent_thermal(){return thermal->get_capacity_percent();}
    double get_capacity_percent_lifetime(){return lifetime->get_capacity_percent();}
    double get_capacity_percent_cycle(){return lifetime->get_capacity_percent_cycle();}
    double get_capacity_percent_calendar(){return lifetime->get_capacity_percent_calendar();}
    double get_cycle_range() {return lifetime->get_cycle_range();}
    double get_avg_cycle_range() {return lifetime->get_avg_cycle_range();}
    size_t get_cycles_elapsed(){return lifetime->get_cycles_elapsed();}
    size_t get_n_replacements(){return replacements;}

    const std::shared_ptr<const battery_properties_params> params;
private:


    battery_capacity_interface * capacity;
    battery_thermal * thermal;
    battery_lifetime * lifetime;
    battery_voltage_interface * voltage;
    battery_losses * losses;

    size_t replacements;

    // Run a component level model
    void run_capacity_model(double &I);
    void run_voltage_model();
    void run_thermal_model(double I, const storage_time_state &time);
    void run_lifetime_model(const size_t &lifetime_index);
    void run_losses_model(const storage_time_state &time);
    void run_replacement(const storage_time_state &time);

};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_MODEL_H
