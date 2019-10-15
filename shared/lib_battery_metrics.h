#ifndef SAM_SIMULATION_CORE_LIB_BATTERY_METRICS_H
#define SAM_SIMULATION_CORE_LIB_BATTERY_METRICS_H

#include "lib_battery_powerflow.h"

/*! Battery metrics class */
class battery_metrics
{
    /**
    Class which computes ac or dc energy metrics such as:
    1. Annual energy sent to charge battery or discharged from battery
    2. Annual energy charged from the grid or PV
    3. Annual energy imported or exported to the grid annually
    4. Average roundtrip and conversion efficiency
    5. Percentage of energy charged from PV
    */
public:
    battery_metrics(double dt_hour);
    ~battery_metrics(){};

    void compute_metrics_ac(const BatteryPower * batteryPower);
    //void compute_metrics_dc(const BatteryPower * batteryPower);
    void compute_annual_loss();

    void accumulate_energy_charge(double P_tofrom_batt);
    void accumulate_energy_discharge(double P_tofrom_batt);
    void accumulate_energy_system_loss(double P_system_loss);
    void accumulate_battery_charge_components(double P_tofrom_batt, double P_pv_to_batt, double P_grid_to_batt);
    void accumulate_grid_annual(double P_tofrom_grid);
    void new_year();


    // outputs
    double energy_pv_charge_annual();
    double energy_grid_charge_annual();
    double energy_charge_annual();
    double energy_discharge_annual();
    double energy_grid_import_annual();
    double energy_grid_export_annual();
    double energy_system_loss_annual();
    double energy_loss_annual();
    double average_battery_conversion_efficiency();
    double average_battery_roundtrip_efficiency();
    double pv_charge_percent();

protected:

    // single value metrics
    double _e_charge_accumulated;	 // [Kwh]
    double _e_discharge_accumulated; // [Kwh]
    double _e_charge_from_pv;		 // [Kwh]
    double _e_charge_from_grid;		 // [Kwh]
    double _e_loss_system;			 // [Kwh]

    /*! Efficiency includes the battery internal efficiency and conversion efficiencies [%] */
    double _average_efficiency;

    /*! Efficiency includes auxilliary system losses [%] */
    double _average_roundtrip_efficiency;

    /*! This is the percentage of energy charge from the PV system [%] */
    double _pv_charge_percent;

    // annual metrics
    double _e_charge_from_pv_annual;   // [Kwh]
    double _e_charge_from_grid_annual; // [Kwh]
    double _e_loss_system_annual;	   // [Kwh]
    double _e_charge_annual;		   // [Kwh]
    double _e_discharge_annual;		   // [Kwh]
    double _e_grid_import_annual;	   // [Kwh]
    double _e_grid_export_annual;	   // [Kwh]
    double _e_loss_annual;			   // [kWh]

    double _dt_hour;
};

#endif //SAM_SIMULATION_CORE_LIB_BATTERY_METRICS_H
