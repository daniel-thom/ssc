#include "lib_dispatch.h"
#include "lib_battery_dispatch.h"
#include "lib_battery_powerflow.h"
#include "lib_battery_controller.h"

double bidir_inverter::convert_to_dc(double P_ac, double * P_dc)
{
    double P_loss = P_ac * (1 - _ac_dc_efficiency);
    *P_dc = P_ac * _ac_dc_efficiency;
    return P_loss;
}
double bidir_inverter::convert_to_ac(double P_dc, double * P_ac)
{
    double P_loss = P_dc * (1 - _dc_ac_efficiency);
    *P_ac = P_dc * _dc_ac_efficiency;
    return P_loss;
}
double bidir_inverter::compute_dc_from_ac(double P_ac)
{
    return P_ac / _dc_ac_efficiency;
}

double battery_rectifier::convert_to_dc(double P_ac, double * P_dc)
{
    double P_loss = P_ac * (1 - _ac_dc_efficiency);
    *P_dc = P_ac * _ac_dc_efficiency;
    return P_loss;
}

AC_charge_controller::AC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyACToDC, double efficiencyDCToAC):
m_dispatch(dispatch),
m_batteryMetrics(battery_metrics)
{
    if (dynamic_cast<dispatch_manual*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp2(new dispatch_manual(*dispatch));
        m_dispatchInitial = std::move(tmp2);
    }
    if (dynamic_cast<dispatch_automatic_behind_the_meter*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp3(new dispatch_automatic_behind_the_meter(*dispatch));
        m_dispatchInitial = std::move(tmp3);
    }
    if (dynamic_cast<dispatch_automatic_front_of_meter*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp4(new dispatch_automatic_front_of_meter(*dispatch));
        m_dispatchInitial = std::move(tmp4);
    }

    std::unique_ptr<bidir_inverter> tmp(new bidir_inverter(efficiencyACToDC, efficiencyDCToAC));
    m_bidirectionalInverter = std::move(tmp);
    m_batteryPower = dispatch->getBatteryPower();
    m_batteryPower->connectionMode = charge_controller_interface::AC_CONNECTED;
    m_batteryPower->singlePointEfficiencyACToDC = m_bidirectionalInverter->ac_dc_efficiency();
    m_batteryPower->singlePointEfficiencyDCToAC = m_bidirectionalInverter->dc_ac_efficiency();
}

void AC_charge_controller::run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t)
{
    if (m_batteryPower->powerPV < 0)
    {
        m_batteryPower->powerPVInverterDraw = m_batteryPower->powerPV;
        m_batteryPower->powerPV = 0;
    }

    // For AC connected system, there is no power going through shared inverter
    m_batteryPower->powerPVThroughSharedInverter = 0;
    m_batteryPower->powerPVClipped = 0;

    // Dispatch the battery
    m_dispatch->dispatch(year, hour_of_year, step_of_hour);

    // Compute annual metrics
    m_batteryMetrics->compute_metrics_ac(m_dispatch->getBatteryPower());
}

DC_charge_controller::DC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyDCToDC, double inverterEfficiencyCutoff):
        m_dispatch(dispatch),
        m_batteryMetrics(battery_metrics)
{
    if (dynamic_cast<dispatch_manual*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp2(new dispatch_manual(*dispatch));
        m_dispatchInitial = std::move(tmp2);
    }
    if (dynamic_cast<dispatch_automatic_behind_the_meter*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp3(new dispatch_automatic_behind_the_meter(*dispatch));
        m_dispatchInitial = std::move(tmp3);
    }
    if (dynamic_cast<dispatch_automatic_front_of_meter*>(m_dispatch)) {
        std::unique_ptr<dispatch_interface> tmp4(new dispatch_automatic_front_of_meter(*dispatch));
        m_dispatchInitial = std::move(tmp4);
    }

    std::unique_ptr<DC_DC_charge_controller> tmp(new DC_DC_charge_controller(efficiencyDCToDC, 100));
    m_DCDCcharge_controller = std::move(tmp);
    m_batteryPower = dispatch->getBatteryPower();
    m_batteryPower->connectionMode = charge_controller_interface::DC_CONNECTED;
    m_batteryPower->singlePointEfficiencyDCToDC = m_DCDCcharge_controller->batt_dc_dc_bms_efficiency();
    m_batteryPower->inverterEfficiencyCutoff = inverterEfficiencyCutoff;
}

void DC_charge_controller::setSharedInverter(SharedInverter * sharedInverter)
{
    m_batteryPower->setSharedInverter(sharedInverter);
}

void DC_charge_controller::run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t)
{
    if (m_batteryPower->powerPV < 0){
        m_batteryPower->powerPV = 0;
    }

    // For DC connected system, there is potentially full PV power going through shared inverter
    m_batteryPower->powerPVThroughSharedInverter = m_batteryPower->powerPV;

    // Dispatch the battery
    m_dispatch->dispatch(year, hour_of_year, step_of_hour);

    // Compute annual metrics
    m_batteryMetrics->compute_metrics_ac(m_dispatch->getBatteryPower());
}
