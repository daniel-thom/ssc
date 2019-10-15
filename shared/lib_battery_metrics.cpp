#include "lib_battery_metrics.h"


battery_metrics::battery_metrics(double dt_hour)
{
    _dt_hour = dt_hour;

    // single value metrics
    _e_charge_accumulated = 0;
    _e_charge_from_pv = 0.;
    _e_charge_from_grid = _e_charge_accumulated; // assumes initial charge from grid
    _e_discharge_accumulated = 0.;
    _e_loss_system = 0.;
    _average_efficiency = 100.;
    _average_roundtrip_efficiency = 100.;
    _pv_charge_percent = 0.;

    // annual metrics
    _e_charge_from_pv_annual = 0.;
    _e_charge_from_grid_annual = _e_charge_from_grid;
    _e_charge_annual = _e_charge_accumulated;
    _e_discharge_annual = 0.;
    _e_loss_system_annual = _e_loss_system;
    _e_grid_import_annual = 0.;
    _e_grid_export_annual = 0.;
    _e_loss_annual = 0.;
}
double battery_metrics::average_battery_conversion_efficiency(){ return _average_efficiency; }
double battery_metrics::average_battery_roundtrip_efficiency(){ return _average_roundtrip_efficiency; }
double battery_metrics::pv_charge_percent(){ return _pv_charge_percent; }
double battery_metrics::energy_pv_charge_annual(){ return _e_charge_from_pv_annual; }
double battery_metrics::energy_grid_charge_annual(){ return _e_charge_from_grid_annual; }
double battery_metrics::energy_charge_annual(){ return _e_charge_annual; }
double battery_metrics::energy_discharge_annual(){ return _e_discharge_annual; }
double battery_metrics::energy_grid_import_annual(){ return _e_grid_import_annual; }
double battery_metrics::energy_grid_export_annual(){ return _e_grid_export_annual; }
double battery_metrics::energy_loss_annual(){ return _e_loss_annual; }
double battery_metrics::energy_system_loss_annual(){ return _e_loss_system_annual; };

void battery_metrics::compute_metrics_ac(const BatteryPower * batteryPower)
{
    accumulate_grid_annual(batteryPower->powerGrid);
    accumulate_battery_charge_components(batteryPower->powerBatteryAC, batteryPower->powerPVToBattery, batteryPower->powerGridToBattery);
    accumulate_energy_charge(batteryPower->powerBatteryAC);
    accumulate_energy_discharge(batteryPower->powerBatteryAC);
    accumulate_energy_system_loss(batteryPower->powerSystemLoss);
    compute_annual_loss();
}
void battery_metrics::compute_annual_loss()
{
    double e_conversion_loss = 0.;
    if (_e_charge_annual > _e_discharge_annual)
        e_conversion_loss = _e_charge_annual - _e_discharge_annual;
    _e_loss_annual = e_conversion_loss + _e_loss_system_annual;
}
void battery_metrics::accumulate_energy_charge(double P_tofrom_batt)
{
    if (P_tofrom_batt < 0.)
    {
        _e_charge_accumulated += (-P_tofrom_batt)*_dt_hour;
        _e_charge_annual += (-P_tofrom_batt)*_dt_hour;
    }
}
void battery_metrics::accumulate_energy_discharge(double P_tofrom_batt)
{
    if (P_tofrom_batt > 0.)
    {
        _e_discharge_accumulated += P_tofrom_batt*_dt_hour;
        _e_discharge_annual += P_tofrom_batt*_dt_hour;
    }
}
void battery_metrics::accumulate_energy_system_loss(double P_system_loss)
{
    _e_loss_system += P_system_loss * _dt_hour;
    _e_loss_system_annual += P_system_loss * _dt_hour;
}
void battery_metrics::accumulate_battery_charge_components(double P_tofrom_batt, double P_pv_to_batt, double P_grid_to_batt)
{
    if (P_tofrom_batt < 0.)
    {
        _e_charge_from_pv += P_pv_to_batt * _dt_hour;
        _e_charge_from_pv_annual += P_pv_to_batt * _dt_hour;
        _e_charge_from_grid += P_grid_to_batt * _dt_hour;
        _e_charge_from_grid_annual += P_grid_to_batt * _dt_hour;
    }
    if (_e_charge_accumulated == 0){
        _average_efficiency = 0;
        _average_roundtrip_efficiency = 0;
    }
    else{
        _average_efficiency = 100.*(_e_discharge_accumulated / _e_charge_accumulated);
        _average_roundtrip_efficiency = 100.*(_e_discharge_accumulated / (_e_charge_accumulated + _e_loss_system));
    }
    _pv_charge_percent = 100.*(_e_charge_from_pv / _e_charge_accumulated);
}
void battery_metrics::accumulate_grid_annual(double P_tofrom_grid)
{
    // e_grid > 0 (export to grid) 
    // e_grid < 0 (import from grid)

    if (P_tofrom_grid > 0)
        _e_grid_export_annual += P_tofrom_grid*_dt_hour;
    else
        _e_grid_import_annual += (-P_tofrom_grid)*_dt_hour;
}

void battery_metrics::new_year()
{
    _e_charge_from_pv_annual = 0.;
    _e_charge_from_grid_annual = 0;
    _e_charge_annual = 0.;
    _e_discharge_annual = 0.;
    _e_grid_import_annual = 0.;
    _e_grid_export_annual = 0.;
    _e_loss_system_annual = 0.;
}
