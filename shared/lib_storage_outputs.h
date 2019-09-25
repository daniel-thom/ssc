#ifndef SYSTEM_ADVISOR_MODEL_LIB_STORAGE_OUTPUTS_H
#define SYSTEM_ADVISOR_MODEL_LIB_STORAGE_OUTPUTS_H

struct storage_state_outputs
{
    double  *totalCharge,
            *availableCharge,
            *boundCharge,
            *maxChargeAtCurrent,
            *maxCharge,
            *maxChargeThermal,
            *SOC,
            *DOD,
            *current,
            *cellVoltage,
            *batteryVoltage,
            *capacityPercent;
};

struct storage_annual_outputs
{
  double *PVChargeEnergy,
          *gridChargeEnergy,
          *chargeEnergy,
          *dischargeEnergy,
          *gridImportEnergy,
          *gridExportEnergy,
          *energySystemLoss,
          *energyLoss;
};

struct storage_lifetime_outputs
{
    double  *DODCycleAverage,
            *cycles,
            *temperature,
            *capacityPercentCycle,
            *capacityPercentCalendar,
            *capacityThermalPercent,
            *replacementsPerYear;
};

struct storage_dispatch_outputs
{
    double  *DispatchMode,
    *GenPower,
    *PVToLoad,
    *PVToGrid,
    *GridPower,
    *GridToLoad,
    *GridPowerTarget;
};

struct storage_accumulated_outputs
{
    double  *BatteryConversionPowerLoss,
    *BatterySystemLoss;

    double  *MarketPrice,
    *CostToCycle;

    double averageCycleEfficiency;
    double averageRoundtripEfficiency;

    double PVChargePercent;

    storage_dispatch_outputs dispatch;
    storage_annual_outputs annual;
    storage_lifetime_outputs lifetime;

};

struct fuelcell_power_outputs
{
    double  *power,
            *toLoad,
            *toBatt,
            *toGrid;
};

struct battery_power_outputs
{
    double  *power,
            *powerTarget,
            *toLoad,
            *toGrid,
            *fromPV,
            *fromGrid;
};

#endif //SYSTEM_ADVISOR_MODEL_LIB_STORAGE_OUTPUTS_H
