#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H

#include "vartab.h"

#include "lib_storage_outputs.h"
#include "lib_storage_params.h"

class SharedInverter;
class lifetime_cycle_t;
class lifetime_calendar_t;
class battery_t;
class battery_metrics_t;
class dispatch_t;
class ChargeController;
class UtilityRate;

enum storage_type{
    BATT_BTM_AUTO, BATT_BTM_MAN, BATT_BTM_CSTM, BATT_FOM_AUTO, BATT_FOM_MAN, BATT_FOM_CSTM, FUELCELL
};

class storage {
private:
    int storage_type;

protected:
    /*! Constant storage properties */
    const storage_config_params config;
    const storage_replacement_params lifetime;
    const storage_time_params time;

    /*! Dynamic storage properties */
    storage_forecast forecast;
    storage_state state;

    /*! Accumulated storage outputs */
    storage_accumulated_outputs accumulated_outputs;

    /*! Dynamic storage outputs */
    storage_state_outputs charge_outputs;

    /*! Models */
    dispatch_t *dispatch_model;
public:
    explicit storage();

    static storage* Create(int storage_type);

    int get_storage_type() {return storage_type;}
    storage_config_params get_config() {return config;}
    storage_replacement_params get_lifetime() {return lifetime;}
    storage_time_params get_time() {return time;}

    bool storage_from_data(var_table& vt);

    bool initialize_models();
};

class battery : public storage {
protected:
    /*! Constant battery properties */
    const battery_properties_params properties;
    const battery_lifetime_params lifetime_batt;
    const battery_losses_params losses;

    /*! Accumulated battery outputs */
    battery_power_outputs power_outputs;

    /*! Models */
    lifetime_cycle_t *lifetime_cycle_model;
    lifetime_calendar_t *lifetime_calendar_model;
    battery_t *battery_model;
    battery_metrics_t *battery_metrics;
    ChargeController *charge_control;

public:
    explicit battery();

    bool battery_from_data(var_table& vt);

    bool initialize_models();

};

/**
 * Front of the meter battery models
 */
class battery_FOM : public battery
{
protected:
    const storage_FOM_params FOM;

public:
    explicit battery_FOM();

    bool battery_FOM_from_data(var_table& vt);

};

class battery_FOM_automated : public battery_FOM
{
protected:
    storage_automated_dispatch_params automated_dispatch;

    /* Energy rates */
    bool ec_use_realtime;
    util::matrix_t<size_t> ec_weekday_schedule;
    util::matrix_t<size_t> ec_weekend_schedule;
    util::matrix_t<double> ec_tou_matrix;
    std::vector<double> ec_realtime_buy;

    /* Automated Dispatch */
    size_t look_ahead_hours;
    double dispatch_update_frequency_hours;

public:
    explicit battery_FOM_automated();

    bool battery_FOM_automated_from_data(var_table &vt);


};

class battery_FOM_manual : public battery_FOM
{
protected:
    storage_manual_dispatch_params manual_dispatch;

public:
    explicit battery_FOM_manual();
    bool battery_FOM_manual_from_data(var_table &vt);


};

class battery_FOM_custom : public battery_FOM
{
protected:
    std::vector<double> custom_dispatch_kw;

public:
    explicit battery_FOM_custom();

    bool battery_FOM_custom_from_data(var_table &vt);
};

/**
 * Behind the meter battery models
 */
class battery_BTM : public battery
{
protected:
    std::vector<double> target_power;

    /*! Models */
    UtilityRate * utilityRate;
public:
    explicit battery_BTM();

    bool battery_BTM_from_data(var_table& vt);
};

class battery_BTM_automated : public battery_BTM
{
protected:
    storage_automated_dispatch_params automated_dispatch;

public:
    explicit battery_BTM_automated();
    bool battery_BTM_automated_from_data(var_table& vt);

};

class battery_BTM_manual : public battery_BTM
{
protected:
    storage_manual_dispatch_params manual_dispatch;

public:
    explicit battery_BTM_manual();
    bool battery_BTM_manual_from_data(var_table& vt);

};

class battery_BTM_custom : public battery_BTM
{
protected:
    std::vector<double> custom_dispatch_kw;

public:
    explicit battery_BTM_custom();
    bool battery_BTM_custom_from_data(var_table& vt);

};

class fuelcell : public storage {
protected:
    fuelcell_power_outputs fuelcell_outputs;

public:
    explicit fuelcell();
    bool fuelcell_from_data(var_table& vt);

};
#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H
