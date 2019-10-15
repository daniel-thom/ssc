#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H

#include <vector>

#include "vartab.h"
#include "lib_util.h"

const double low_tolerance = 0.01;
const double tolerance = 0.0001;

struct storage_config_params
{
    bool is_batt;
    bool is_fuelcell;
    int dispatch;                                   // from FOM_MODES or BTM_MODES
    int meter_position;                             // from METERING
};

struct storage_replacement_params
{
    enum REP {NONE,CAPACITY,SCHEDULE} option;
    double replacement_capacity;                    // kWh
    std::vector<int> replacement_per_yr_schedule;   // number/year
    std::vector<int> replacement_per_yr;            // number/year
    std::vector<double> replacement_percent_per_yr_schedule; // %
    double cost_per_kwh;                            // $/kWh

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
    size_t index;                                   // lifetime_index (0 - nyears * steps_per_hour * 8760)
    size_t lifetime_index;                              // index for one year (0- steps_per_hour * 8760)

public:
    const size_t steps_per_hour;
    const size_t steps_per_year;

    // initialized to -1, use increment() in all usages
    storage_time_state(size_t step_hr);

    size_t get_year() const {return year;}
    size_t get_hour_year() const {return index / steps_per_hour;}
    size_t get_hour_lifetime() const {return lifetime_index / steps_per_hour;}
    size_t get_index() const {return index;}
    size_t get_lifetime_index() const {return lifetime_index;}

    storage_time_state start();

    storage_time_state increment(size_t steps = 1);

    storage_time_state reset_storage_time(size_t index = 0);
};


/**
 * Battery specific paramters
 */
struct battery_lifetime_params
{
    std::shared_ptr<const storage_time_params> time;

    util::matrix_t<double> cycle_matrix;

    enum CALENDAR {NONE, MODEL, TABLE} choice;

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

    enum VOLTAGE{MODEL, TABLE} choice;

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
    double surface_area;
    double resistance;                          // internal battery resistance (Ohm)
    double Cp;                                  // [J/KgK] - battery specific heat capacity
    util::matrix_t<double> cap_vs_temp;         // [Ah] [K]
    double h;                                   // [Wm2K] - general heat transfer coefficient
    std::vector<double> T_room_K;               // [K] - storage room temperature

    void initialize_from_data(var_table& vt, storage_time_params&);
};

struct battery_losses_params
{
    std::shared_ptr<const storage_time_params> time;

    enum MODE {MONTHLY, TIMESERIES} mode;
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
    enum CHEM{ LEAD_ACID, LITHIUM_ION, VANADIUM_REDOX, IRON_FLOW} chem;

    std::shared_ptr<const battery_capacity_params> capacity;

    std::shared_ptr<const battery_voltage_params> voltage;

    std::shared_ptr<const battery_thermal_params> thermal;

    std::shared_ptr<const battery_lifetime_params> lifetime;

    std::shared_ptr<const battery_losses_params> losses;

    std::shared_ptr<const storage_replacement_params> replacement;

    void initialize_from_data(var_table& vt, storage_time_params& t);
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_PARAMS_H
