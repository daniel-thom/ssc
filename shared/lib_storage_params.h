#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H

#include <vector>

#include "vartab.h"
#include "lib_util.h"

const double low_tolerance = 0.01;
const double tolerance = 0.001;

namespace storage_params{
    enum CHEMS{ LEAD_ACID, LITHIUM_ION, VANADIUM_REDOX, IRON_FLOW};
    enum REPLACEMENT { NONE, CAPACITY, SCHEDULE };
    enum LOSSES { MONTHLY, TIMESERIES};
}

struct storage_config_params
{
    bool is_batt;
    bool is_fuelcell;
    int dispatch;                                   // from FOM_MODES or BTM_MODES
    int meter_position;                             // from METERING
};

struct storage_replacement_params
{
    /* Replacement options */
    int replacement_option;                         // 0=none,1=capacity based,2=user schedule
    double replacement_capacity;                    // kWh
    double cost_per_kwh;                            // $/kWh
    std::vector<int> replacement_per_yr;            // number/year
    std::vector<int> replacement_per_yr_schedule;   // number/year
    std::vector<double> replacement_percent_per_yr_schedule; // %

    void initialize_from_data(var_table &vt, bool batt_not_fuelcell);
};


struct storage_time_params
{
    /* Constant values */
    size_t step_per_hour;
    size_t nyears;
    double dt_hour;

    bool system_use_lifetime_output;                // true or false

public:
    storage_time_params(double dt_hr, int n_years){
        dt_hour = dt_hr;
        step_per_hour = static_cast<size_t>(1. / dt_hour);
        nyears = n_years;
        system_use_lifetime_output = false;
        if (nyears > 1)
            system_use_lifetime_output = true;
    }

    void initialize_from_data(var_table &vt);
};


class storage_time_state
{
private:
    /* Changing time values */
    size_t year;
    size_t hour;
    size_t steps_per_hour;
    size_t index;                                   // lifetime_index (0 - nyears * steps_per_hour * 8760)
    size_t lifetime_index;                              // index for one year (0- steps_per_hour * 8760)

public:
    storage_time_state(size_t step_hr);

    size_t get_year() const {return year;}
    size_t get_hour() const {return hour;}
    size_t get_step() const {return steps_per_hour;}
    size_t get_index() const {return index;}
    size_t get_lifetime_index() const {return lifetime_index;}

    void increment_storage_time();

    void reset_storage_time(size_t index = 0);
};


/**
 * Battery specific paramters
 */
struct battery_lifetime_params
{
    std::shared_ptr<const storage_time_params> time;

    util::matrix_t<double> cycle_matrix;

    enum CALENDAR_OPTIONS {NONE, MODEL, TABLE} calendar_choice;

    double calendar_q0;
    double calendar_a;
    double calendar_b;
    double calendar_c;

    util::matrix_t<double> calendar_matrix;

    void initialize_from_data(var_table &vt, storage_time_params&);

};


struct battery_voltage_params
{
    int num_cells_series;
    int num_strings;
    double Vnom_default;
    double resistance;

    enum {MODEL, TABLE} voltage_choice;

    //voltage choice 0
    double Vfull;
    double Vexp;
    double Vnom;
    double Qfull;
    double Qexp;
    double Qnom;
    double C_rate;

    // voltage_choice 1
    util::matrix_t<double> voltage_matrix;

    void initialize_from_data(var_table& vt);
};


struct battery_thermal_params
{
    std::shared_ptr<const storage_time_params> time;

    double mass;
    double length;
    double width;
    double height;
    double resistance;                          // internal battery resistance (Ohm)
    double Cp;                                  // [J/KgK] - battery specific heat capacity
    util::matrix_t<double> cap_vs_temp;
    double h;                                   // [Wm2K] - general heat transfer coefficient
    std::vector<double> T_room_K;               // [K] - storage room temperature

    void initialize_from_data(var_table& vt, storage_time_params&);
};

struct battery_losses_params
{
    std::shared_ptr<const storage_time_params> time;

    int loss_monthly_or_timeseries;
    std::vector<double> losses_charging;
    std::vector<double> losses_discharging;
    std::vector<double> losses_idle;
    std::vector<double> losses_full;

    void initialize_from_data(var_table &vt, storage_time_params &time);
};

struct battery_capacity_params{
    std::shared_ptr<const storage_time_params> time;

    double qmax;
    double initial_SOC;                     // 0-100
    double minimum_SOC;
    double maximum_SOC;

    struct{
        double q20;                // equal to qmax
        double t1;
        double q1;
        double q10;
    } lead_acid;

    void initialize_from_data(var_table &vt, storage_time_params &time);
};


struct battery_properties_params
{
    int chem;

    battery_thermal_params thermal;

    battery_voltage_params voltage_vars;

    std::shared_ptr<const battery_capacity_params> capacity_vars;

    std::shared_ptr<const battery_lifetime_params> lifetime;

    battery_losses_params losses;

    void initialize_from_data(var_table& vt, storage_time_params& t);
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
