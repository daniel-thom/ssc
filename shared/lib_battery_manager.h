#ifndef SAM_SIMULATION_CORE_LIB_BATTERY_MANAGER_H
#define SAM_SIMULATION_CORE_LIB_BATTERY_MANAGER_H

#include "vartab.h"

struct battery_manager
{
    /// Pass in the single-year number of records
    battery_manager( var_table &vt, bool setup_model, size_t nrec, double dt_hr, batt_variables *batt_vars=0);

    battery_manager(const battery_manager& orig);

    void parse_configuration();

    /// Initialize automated dispatch with lifetime vectors
    void initialize_automated_dispatch(std::vector<ssc_number_t> pv= std::vector<ssc_number_t>(),
                                       std::vector<ssc_number_t> load= std::vector<ssc_number_t>(),
                                       std::vector<ssc_number_t> cliploss= std::vector<ssc_number_t>());
    ~battery_manager();


    void initialize_time(size_t year, size_t hour_of_year, size_t step);

    /// Run the battery for the current timestep, given the PV power, load, and clipped power
    void advance(var_table *vt, double P_gen, double V_gen=0, double P_load=0, double P_gen_clipped=0);

    /// Given a DC connected battery, set the shared PV and battery invertr
    void setSharedInverter(SharedInverter * sharedInverter);

    void outputs_fixed(var_table *vt);
    void outputs_topology_dependent();
    void metrics();
    void update_grid_power(compute_module &cm, double P_gen_ac, double P_load_ac, size_t index);

    /*! Manual dispatch*/
    bool manual_dispatch = false;

    /*! Automated dispatch look ahead*/
    bool look_ahead = false;

    /*! Automated dispatch look behind*/
    bool look_behind = false;

    /*! Automated dispatch use custom input forecast (look ahead)*/
    bool input_forecast = false;

    /*! Automated dispatch override algorithm grid target calculation*/
    bool input_target = false;

    /*! Use user-input battery dispatch */
    bool input_custom_dispatch = false;

    // for user schedule
    void calculate_monthly_and_annual_outputs( compute_module &cm );

    // time quantities
    size_t step_per_hour;
    size_t step_per_year;
    size_t nyears;
    size_t total_steps;
    double _dt_hour;

    size_t year;
    size_t hour;
    size_t step;
    size_t index; // lifetime_index (0 - nyears * steps_per_hour * 8760)
    size_t year_index; // index for one year (0- steps_per_hour * 8760)

    // member data
    std::shared_ptr<battery_properties_params> params;
    std::shared_ptr<battery> battery_model;
    battery_metrics *batt_metrics;
    dispatch_interface *dispatch_model;
    charge_controller_interface *charge_control;
    UtilityRate * utilityRate;

    bool en;
    int chem;

    batt_variables * batt_vars;
    bool make_vars;

    /*! Map of profile to discharge percent */
    std::map<size_t, double> dm_percent_discharge;

    /*! Map of profile to gridcharge percent*/
    std::map<size_t, double> dm_percent_gridcharge;

    std::vector<double> target_power;
    std::vector<double> target_power_monthly;

    double e_charge;
    double e_discharge;

    /*! Variables to store forecast data */
    std::vector<double> pv_prediction;
    std::vector<double> load_prediction;
    std::vector<double> cliploss_prediction;
    int prediction_index;

    /*! If fuel cell is attached */
    std::vector<double> fuelcellPower;

    // outputs
    double
            *outTotalCharge,
            *outAvailableCharge,
            *outBoundCharge,
            *outMaxChargeAtCurrent,
            *outMaxCharge,
            *outMaxChargeThermal,
            *outSOC,
            *outDOD,
            *outCurrent,
            *outCellVoltage,
            *outBatteryVoltage,
            *outCapacityPercent,
            *outCapacityPercentCycle,
            *outCapacityPercentCalendar,
            *outCycles,
            *outDODCycleAverage,
            *outBatteryBankReplacement,
            *outBatteryTemperature,
            *outCapacityThermalPercent,
            *outDispatchMode,
            *outBatteryPower,
            *outGenPower,
            *outGridPower,
            *outPVToLoad,
            *outBatteryToLoad,
            *outGridToLoad,
            *outFuelCellToLoad,
            *outGridPowerTarget,
            *outBattPowerTarget,
            *outPVToBatt,
            *outGridToBatt,
            *outFuelCellToBatt,
            *outPVToGrid,
            *outBatteryToGrid,
            *outFuelCellToGrid,
            *outBatteryConversionPowerLoss,
            *outBatterySystemLoss,
            *outAnnualPVChargeEnergy,
            *outAnnualGridChargeEnergy,
            *outAnnualChargeEnergy,
            *outAnnualDischargeEnergy,
            *outAnnualGridImportEnergy,
            *outAnnualGridExportEnergy,
            *outAnnualEnergySystemLoss,
            *outAnnualEnergyLoss,
            *outMarketPrice,
            *outCostToCycle,
            *outBenefitCharge,
            *outBenefitGridcharge,
            *outBenefitClipcharge,
            *outBenefitDischarge;

    double outAverageCycleEfficiency;
    double outAverageRoundtripEfficiency;
    double outPVChargePercent;
};

#endif //SAM_SIMULATION_CORE_LIB_BATTERY_MANAGER_H
