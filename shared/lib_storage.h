#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H

#include "vartab.h"

#include "lib_storage_outputs.h"
#include "lib_storage_params.h"
#include "lib_dispatch_params.h"

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

class storage_interface {
private:
    int storage_type;

protected:

    /*! Models */

    void storage_from_data(var_table& vt);

public:
    explicit storage_interface();

    virtual ~storage_interface();

    static storage_interface* Create(int storage_type);

//    virtual int get_storage_type() = 0;
//    virtual storage_config_params get_config() = 0;
//    storage_time_params get_time() {return time;}

    virtual void from_data(var_table &vt) = 0;

};

class battery_models {
protected:
    /*! Constant storage properties */
    const storage_config_params config;
    const storage_time_params time;

    /*! Dynamic storage properties */
    storage_forecast forecast;
    storage_state state;

    /*! Accumulated storage outputs */
    storage_accumulated_outputs accumulated_outputs;

    /*! Dynamic storage outputs */
    storage_state_outputs charge_outputs;

    /*! Constant battery properties */
    const battery_properties_params properties;
    const battery_lifetime_params lifetime_batt;
    const battery_losses_params losses;

    /*! Accumulated battery outputs */
    battery_power_outputs power_outputs;

    /*! Models */
    dispatch_t *dispatch_model;
    battery_t *battery_model;
    battery_metrics_t *battery_metrics;
    ChargeController *charge_control;

    void battery_from_data(var_table& vt);

public:
    explicit battery_models();

    ~battery_models() {
        delete battery_model;
        delete battery_metrics;
        delete charge_control;
    };

    void from_data(var_table &vt);

    void initialize_models();

};

/**
 * Front of the meter battery models
 */
class battery_FOM : public storage_interface
{
protected:
    const storage_FOM_params FOM;

    void battery_FOM_from_data(var_table& vt);

public:
    explicit battery_FOM();

    void from_data(var_table &vt) override = 0;
};

class battery_FOM_automated : public storage_interface
{
protected:
    const storage_FOM_params FOM;

    storage_automated_dispatch_params automated_dispatch;

    UtilityRate * utilityRate;

    /* Automated Dispatch */
    size_t look_ahead_hours;
    double dispatch_update_frequency_hours;

    void battery_FOM_automated_from_data(var_table &vt);

    void initialize_models();

public:
    explicit battery_FOM_automated();

    void from_data(var_table &vt) override;
};

class battery_FOM_manual : public storage_interface
{
protected:
    const storage_FOM_params FOM;

    storage_manual_dispatch_params manual_dispatch;

    void battery_FOM_manual_from_data(var_table &vt);

public:
    explicit battery_FOM_manual();

    void from_data(var_table &vt) override;
};

class battery_FOM_custom : public storage_interface
{
protected:
    const storage_FOM_params FOM;

    std::vector<double> custom_dispatch_kw;

    void battery_FOM_custom_from_data(var_table &vt);

public:
    explicit battery_FOM_custom();

    void from_data(var_table &vt) override;
};

/**
 * Behind the meter battery models
 */
class battery_BTM : public storage_interface
{
protected:
    std::vector<double> target_power;

    void battery_BTM_from_data(var_table& vt);

    /*! Models */
public:
    explicit battery_BTM();

    ~battery_BTM() override {
    }

    void from_data(var_table &vt) override = 0;

};

class battery_BTM_automated : public battery_BTM
{
protected:
    storage_automated_dispatch_params automated_dispatch;

    void battery_BTM_automated_from_data(var_table& vt);

public:
    explicit battery_BTM_automated();

    ~battery_BTM_automated() override {}

    void from_data(var_table &vt) override;

};

class battery_BTM_manual : public battery_BTM
{
protected:
    storage_manual_dispatch_params manual_dispatch;

    void battery_BTM_manual_from_data(var_table& vt);

public:
    explicit battery_BTM_manual();
    void from_data(var_table &vt) override;

};

class battery_BTM_custom : public battery_BTM
{
protected:
    std::vector<double> custom_dispatch_kw;

    void battery_BTM_custom_from_data(var_table& vt);

public:
    explicit battery_BTM_custom();

    void from_data(var_table &vt) override;

};

class fuelcell : public storage_interface {
protected:
    const storage_replacement_params replacement;

    fuelcell_power_outputs fuelcell_outputs;

    void fuelcell_from_data(var_table& vt);
public:
    explicit fuelcell();

    void from_data(var_table &vt);

    };
#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_H
