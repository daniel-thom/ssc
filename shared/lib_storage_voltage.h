#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_VOLTAGE_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_VOLTAGE_H


#include <vector>
#include <map>
#include <string>
#include <stdio.h>
#include <algorithm>

#include "lib_util.h"
#include "lib_storage_params.h"

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
Voltage Base class.
All voltage models are based on one-cell, but return the voltage for one battery
*/
class thermal_t;
struct capacity_state;
class voltage_interface
{
public:
    explicit voltage_interface() {}

    // deep copy
    virtual voltage_interface * clone()=0;

    // copy from voltage to this
    virtual void copy(voltage_interface *) = 0;


    virtual ~voltage_interface(){};

    virtual void updateVoltage(const capacity_state &capacity, ...) = 0;

    virtual double get_battery_voltage() = 0; // voltage of one battery
    virtual double get_cell_voltage() = 0; // voltage of one cell
    virtual double get_battery_voltage_nominal() = 0; // nominal voltage of battery
    virtual double get_R_battery() = 0; // computed battery resistance

    enum VOLTAGE_CHOICE{VOLTAGE_MODEL, VOLTAGE_TABLE};
};

// A row in the table
class table_point
{
public:
    table_point(double DOD = 0., double V = 0.) :
            _DOD(DOD), _V(V){}
    double DOD() const{ return _DOD; }
    double V() const{ return _V; }

private:
    double _DOD;
    double _V;
};

struct byDOD
{
    bool operator()(table_point const &a, table_point const &b){ return a.DOD() < b.DOD(); }
};


class voltage_table_t : public voltage_interface
{
public:
    voltage_table_t(int num_cell_series, int num_strings, double voltage, double resistance, util::matrix_t<double> voltage_table);
    // deep copy
    voltage_table_t * clone();

    // copy from voltage to this
    void copy(voltage_interface *);

    void updateVoltage(const capacity_state &capacity, ...) override;

    double get_battery_voltage() override {return num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() {return num_cells_series * cell_voltage_nominal;};
    double get_R_battery() override { return R * num_cells_series / num_strings; }

private:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    int num_cells_series;
    int num_strings;             // addition number in parallel
    double cell_voltage_nominal;
    double R;
    util::matrix_t<double> batt_voltage_matrix;  // voltage vs depth-of-discharge

    std::vector<table_point> voltage_table;

    bool exactVoltageFound(double DOD, double &V);
    void prepareInterpolation(double & DOD_lo, double & V_lo, double & DOD_hi, double & V_hi, double DOD);

};

// Shepard + Tremblay Model
class voltage_dynamic_t : public voltage_interface
{
public:
    voltage_dynamic_t(int num_cell_series, int num_strings, const battery_voltage_params* p);

    // deep copy
    voltage_dynamic_t * clone();

    // copy from voltage to this
    void copy(voltage_interface *);

    void parameter_compute();
    void updateVoltage(const capacity_state &capacity, ...) override;

    double get_battery_voltage() override {return num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() {return num_cells_series * cell_voltage_nominal;};
    double get_R_battery() override { return R * num_cells_series / num_strings; }

protected:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    int num_cells_series;
    int num_strings;             // addition number in parallel
    double R;                    // internal cell resistance (Ohm)
    double cell_voltage_nominal;
    double Vfull;
    double Vexp;
    double Vnom;
    double Qfull;
    double Qexp;
    double Qnom;
    double C_rate;
    double A;
    double B0;
    double E0;
    double K;

    double voltage_model_tremblay_hybrid(double capacity, double current, double q0);

};

// D'Agostino Vanadium Redox Flow Model
class voltage_vanadium_redox_t : public voltage_interface
{
public:
    voltage_vanadium_redox_t(int num_cell_series, int num_strings, double V_ref_50, double R);

    // deep copy
    voltage_vanadium_redox_t * clone();

    // copy from voltage to this
    void copy(voltage_interface *);

    void updateVoltage(const capacity_state &capacity, double T_battery_K) override;

    double get_battery_voltage() override {return num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() {return num_cells_series * cell_voltage_nominal;};
    double get_R_battery() override { return R * num_cells_series / num_strings; }

private:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    int num_cells_series;
    int num_strings;             // addition number in parallel
    double R;                    // internal cell resistance (Ohm)
    double cell_voltage_nominal;
    double V_ref_50;				// Reference voltage at 50% SOC
    double I;						// Current level [A]
    double R_molar;
    double F;
    double C0;

    double voltage_model(double q0, double qmax, double I_string, double T);
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_VOLTAGE_H
