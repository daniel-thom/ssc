#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_LIFETIME_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_LIFETIME_H

#include "lib_storage_params.h"

struct cycle_lifetime_state {
    double relative_q;
    double Xlt;
    double Ylt;
    double Range;
    double average_range;
    size_t nCycles;
    std::vector<double> Peaks;
    double jlt;

    void init(double rel_q);
};

/*
Lifetime cycling class.
*/

class lifetime_cycle
{

public:
    explicit lifetime_cycle(const battery_lifetime_params*);

    virtual ~lifetime_cycle();

    lifetime_cycle(const lifetime_cycle &);

    /// return q, the effective capacity percent
    double runCycleLifetime(double DOD);

    /// return hypothetical dq the average cycle
    double estimateCycleDamage();

    /// Replace or partially replace a battery
    void replaceBattery(double replacement_percent);

    cycle_lifetime_state get_state(){return state; };

    void set_state(const cycle_lifetime_state& new_state){ state = new_state; }

private:

    /// Run the rainflow counting algorithm at the current depth-of-discharge to determine cycle
    void rainflow(double DOD);
    void rainflow_ranges();
    void rainflow_ranges_circular(int index);
    int rainflow_compareRanges();

    /// Bilinear interpolation, given the depth-of-discharge and cycle number, return the capacity percent
    double bilinear(double DOD, int cycle_number);

    util::matrix_t<double> _cycles_vs_DOD;
    std::vector<double> _DOD_vect;
    std::vector<double> _cycles_vect;
    std::vector<double> _capacities_vect;

    cycle_lifetime_state state;

    const std::shared_ptr<const battery_lifetime_params> params_p;

    enum RETURN_CODES
    {
        LT_SUCCESS,
        LT_GET_DATA,
        LT_RERANGE
    };
};

struct calendar_lifetime_state {
    size_t last_idx;
    size_t day_age_of_battery;
    double q;

    // Li Ion model, relative capacity (0 - 1)
    double dq_old;
    double dq_new;

    void init(double q0);
};

/*
Lifetime calendar model
*/
class lifetime_calendar
{
public:
    explicit lifetime_calendar();

    lifetime_calendar(const battery_lifetime_params*);

    explicit lifetime_calendar(double q0=1.02, double a=2.66e-3, double b=7280, double c=930, double dt_hour=1);

    lifetime_calendar(const lifetime_calendar&);

    virtual ~lifetime_calendar(){/* Nothing to do */};

    /// Given the index of the simulation, the tempertature and SOC, return the effective capacity percent
    double runLifetimeCalendarModel(size_t idx, double T, double SOC);

    /// Reset or augment the capacity
    void replaceBattery(double replacement_percent);

    double get_calendar_choice();

    calendar_lifetime_state get_state() { return state;}
    void set_state(const calendar_lifetime_state& new_state) {state = new_state;}

protected:

    void runLithiumIonModel(double T, double SOC);
    void runTableModel();

private:

    calendar_lifetime_state state;

    const std::shared_ptr<const battery_lifetime_params> params_p;

    double dt_day;
    std::vector<int> table_days;
    std::vector<double> table_capacity;
};


struct lifetime_state{
    cycle_lifetime_state cycle;
    calendar_lifetime_state calendar;

    double q;               // battery relative capacity (0 - 100%)
    int replacements;       // Number of replacements this year

};

/*
Class to encapsulate multiple lifetime models, and linearly combined the associated degradation and handle replacements
*/
class battery_lifetime
{
public:

    battery_lifetime(const battery_lifetime_params& params);

    battery_lifetime(const battery_lifetime &);

    virtual ~battery_lifetime(){};

    /// Execute the lifetime models given the current lifetime run index, capacity model, and temperature
    void runLifetimeModels(const storage_state& time, const capacity_state& cap, double T_battery);

    /// Check if the battery should be replaced based upon the replacement criteria
    bool checkReplacement(const storage_state& time);

    /// Reset the number of replacements at the year end
    void reset_replacements();

    /// Return the relative capacity percentage of nominal (%)
    double get_capacity_percent();

    /// Return the relative capacity percentage of nominal caused by cycle damage (%)
    double get_capacity_percent_cycle();

    /// Return the relative capacity percentage of nominal caused by calendar fade (%)
    double get_capacity_percent_calendar();

    /// Return the number of total replacements in the year
    int get_replacements();

    /// Return the replacement percent
    double get_replacement_percent();

    /// Set the replacement option
    void set_replacement_option(int option);

    void set_state(const lifetime_state& s) {state = s;}

    lifetime_state get_state() const {return state;}

    battery_lifetime_params get_params() const {return params;}

protected:

    const battery_lifetime_params params;

    lifetime_state state;

    /// Underlying lifetime cycle model
    std::unique_ptr<lifetime_cycle> cycle_model;

    /// Underlying lifetime calendar model
    std::unique_ptr<lifetime_calendar> calendar_model;
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_LIFETIME_H
