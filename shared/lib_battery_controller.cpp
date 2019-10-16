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

bool charge_controller::check_constraints(double &I, size_t count)
{
    bool iterate = true;
    double I_initial = I;
    bool current_iterate = false;
    bool power_iterate = false;

    // don't allow any changes to violate current limits
    const double& SOC_min = battery_model->params->capacity->minimum_SOC;
    const double& SOC_max = battery_model->params->capacity->maximum_SOC;
    const double& dt_hour = battery_model->params->lifetime->time->dt_hour;

    if (restrict_current(I))
    {
        current_iterate = true;
    }
        // don't allow violations of power limits
    else if (restrict_power(I))
    {
        power_iterate = true;
    }
        // decrease the current draw if took too much
    else if (I > 0 && battery_model->get_SOC() < SOC_min - tolerance)
    {
        double dQ = 0.01 * (SOC_min - battery_model->get_SOC()) * battery_model->battery_charge_maximum_thermal();
        I -= dQ / dt_hour;
    }
        // decrease the current charging if charged too much
    else if (I < 0 &&battery_model->get_SOC() > SOC_max + tolerance)
    {
        double dQ = 0.01 * (battery_model->get_SOC() - SOC_max) * battery_model->battery_charge_maximum_thermal();
        I += dQ / dt_hour;
    }

    else
        iterate = false;

    // update constraints for current, power, if they are now violated
    if (!current_iterate) {
        current_iterate = restrict_current(I);


    }
    if (!power_iterate) {
        power_iterate = restrict_power(I);

    }

    // iterate if any of the conditions are met
    if (iterate || current_iterate || power_iterate)
        iterate = true;

    // stop iterating after n tries
    if (count > battery_dispatch::constraintCount)
        iterate = false;




    // don't allow battery to flip from charging to discharging or vice versa
    if (fabs(I) > tolerance && (I_initial / I) < 0) {
        I = 0;
        iterate = false;
    }

    return iterate;
}
void charge_controller::SOC_controller(double& powerDC)
{
    // Implement minimum SOC cut-off
    if (powerDC > 0)
    {
        if (battery_model->get_SOC() <= battery_model->params->capacity->minimum_SOC + tolerance) {
            powerDC = 0;
        }
    }
        // Maximum SOC cut-off
    else if (powerDC < 0)
    {
        if (battery_model->get_SOC() >= battery_model->params->capacity->maximum_SOC- tolerance) {
            powerDC = 0;
        }
    }
}

void charge_controller::switch_controller(double& powerDC)
{
    capacity_state::MODE target_mode;
    if (powerDC < 0)
        target_mode = capacity_state::DISCHARGE;
    else if (powerDC > 0)
        target_mode = capacity_state::CHARGE;
    else
        target_mode = capacity_state::NO_CHARGE;

    // Implement rapid switching check
    if (target_mode != charging_mode)
    {
        if (time_at_mode <= params->minimum_modetime)
        {
            powerDC = 0.;
            charging_mode = target_mode;
        }
        else{
            time_at_mode = 0;
            return;
        }
    }
    time_at_mode += (int)(round(params->capacity->time->dt_hour * util::hour_to_min));
}

double charge_controller::current_controller(double& powerDC)
{
    double P, I = 0.; // [W],[V]
    P = util::kilowatt_to_watt*powerDC;
    I = P / battery_model->get_V_nom();
    restrict_current(I);
    return I;
}

bool charge_controller::restrict_current(double &I)
{
    bool iterate = false;
    if (params->restriction == battery_controller_params::RESTRICT_CURRENT
        || params->restriction == battery_controller_params::RESTRICT_BOTH)
    {
        if (I < 0)
        {
            if (fabs(I) > params->current_charge_max)
            {
                I = -params->current_charge_max;
                iterate = true;
            }
        }
        else
        {
            if (I > params->current_discharge_max)
            {
                I = params->current_discharge_max;
                iterate = true;
            }
        }
    }
    return iterate;
}

bool charge_controller::restrict_power(double &I)
{
    bool iterate = false;
    if (params->restriction == battery_controller_params::RESTRICT_POWER
        || params->restriction == battery_controller_params::RESTRICT_BOTH)
    {
        double powerBattery = I * battery_model->get_V() * util::watt_to_kilowatt;
        double dP = 0;

        double power_AC = power_DC_to_AC(powerBattery);

        // charging
        if (powerBattery < 0)
        {
            if (fabs(powerBattery) > params->power_charge_max_kwdc * (1 + low_tolerance))
            {
                dP = fabs(params->power_charge_max_kwdc - fabs(powerBattery));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
            else if (params->connection == battery_controller_params::AC &&
                     fabs(power_AC) > params->power_charge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params->power_charge_max_kwac - fabs(power_AC));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
                // This could just be grid power since that's technically the only AC component.  But, limit all to this
            else if (params->connection == battery_controller_params::DC &&
                     fabs(power_AC) > params->power_charge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params->power_charge_max_kwac - fabs(power_AC));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
        }
        else
        {
            if (fabs(powerBattery) > params->power_discharge_max_kwdc * (1 + low_tolerance))
            {
                dP = fabs(params->power_discharge_max_kwdc - powerBattery);

                // decrease discharging magnitude
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
            else if (fabs(power_AC) > params->power_discharge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params->power_discharge_max_kwac - power_AC);

                // decrease discharging magnitude
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
        }
    }
    return iterate;
}

double charge_controller::runDispatch(const storage_time_state &time, double power_DC)
{
    // Ensure the battery operates within the state-of-charge limits
    SOC_controller(power_DC);

    // Ensure the battery isn't switching rapidly between charging and dischaging
    switch_controller(power_DC);

    // Calculate current, and ensure the battery falls within the current limits
    double I = current_controller(power_DC);


    // Setup battery iteration
    auto battery_model_initial = battery_model->get_state();
    bool iterate = true;
    size_t count = 0;

    do {
        // Run Battery Model to update charge based on charge/discharge
        battery_model->run(time, I);


        // Update how much power was actually used to/from battery
        I = battery_model->get_I();
        double battery_voltage_new = battery_model->get_V();
        power_DC = I * battery_voltage_new * util::watt_to_kilowatt;

        // Update power flow calculations, calculate AC power, and check the constraints
//        mbattery_modelPowerFlow->calculate();
        iterate = check_constraints(I, count);

        // Recalculate the DC battery power
        power_DC = I * battery_model->get_V() * util::watt_to_kilowatt;

        count++;

    } while (iterate);

    battery_model->set_state(battery_model_initial);
    battery_model->run(time, I);

    // finalize AC power flow calculation and update for next step
//    mbattery_modelPowerFlow->calculate();
}
//
//AC_charge_controller::AC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyACToDC, double efficiencyDCToAC):
//m_dispatch(dispatch),
//mbattery_modelMetrics(battery_metrics)
//{
//    if (dynamic_cast<dispatch_manual*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp2(new dispatch_manual(*dispatch));
//        m_dispatchInitial = std::move(tmp2);
//    }
//    if (dynamic_cast<dispatch_automatic_behind_the_meter*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp3(new dispatch_automatic_behind_the_meter(*dispatch));
//        m_dispatchInitial = std::move(tmp3);
//    }
//    if (dynamic_cast<dispatch_automatic_front_of_meter*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp4(new dispatch_automatic_front_of_meter(*dispatch));
//        m_dispatchInitial = std::move(tmp4);
//    }
//
//    std::unique_ptr<bidir_inverter> tmp(new bidir_inverter(efficiencyACToDC, efficiencyDCToAC));
//    m_bidirectionalInverter = std::move(tmp);
//    mbattery_modelPower = dispatch->getBatteryPower();
//    mbattery_modelPower->connectionMode = charge_controller_interface::AC_CONNECTED;
//    mbattery_modelPower->singlePointEfficiencyACToDC = m_bidirectionalInverter->ac_dc_efficiency();
//    mbattery_modelPower->singlePointEfficiencyDCToAC = m_bidirectionalInverter->dc_ac_efficiency();
//}
//
//void AC_charge_controller::run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t)
//{
//    if (mbattery_modelPower->powerPV < 0)
//    {
//        mbattery_modelPower->powerPVInverterDraw = mbattery_modelPower->powerPV;
//        mbattery_modelPower->powerPV = 0;
//    }
//
//    // For AC connected system, there is no power going through shared inverter
//    mbattery_modelPower->powerPVThroughSharedInverter = 0;
//    mbattery_modelPower->powerPVClipped = 0;
//
//    // Dispatch the battery
//    m_dispatch->dispatch(year, hour_of_year, step_of_hour);
//
//    // Compute annual metrics
//    mbattery_modelMetrics->compute_metrics_ac(m_dispatch->getBatteryPower());
//}
//
//DC_charge_controller::DC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyDCToDC, double inverterEfficiencyCutoff):
//        m_dispatch(dispatch),
//        mbattery_modelMetrics(battery_metrics)
//{
//    if (dynamic_cast<dispatch_manual*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp2(new dispatch_manual(*dispatch));
//        m_dispatchInitial = std::move(tmp2);
//    }
//    if (dynamic_cast<dispatch_automatic_behind_the_meter*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp3(new dispatch_automatic_behind_the_meter(*dispatch));
//        m_dispatchInitial = std::move(tmp3);
//    }
//    if (dynamic_cast<dispatch_automatic_front_of_meter*>(m_dispatch)) {
//        std::unique_ptr<dispatch_interface> tmp4(new dispatch_automatic_front_of_meter(*dispatch));
//        m_dispatchInitial = std::move(tmp4);
//    }
//
//    std::unique_ptr<DC_DC_charge_controller> tmp(new DC_DC_charge_controller(efficiencyDCToDC, 100));
//    m_DCDCcharge_controller = std::move(tmp);
//    mbattery_modelPower = dispatch->getBatteryPower();
//    mbattery_modelPower->connectionMode = charge_controller_interface::DC_CONNECTED;
//    mbattery_modelPower->singlePointEfficiencyDCToDC = m_DCDCcharge_controller->batt_dc_dc_bms_efficiency();
//    mbattery_modelPower->inverterEfficiencyCutoff = inverterEfficiencyCutoff;
//}
//
//void DC_charge_controller::setSharedInverter(SharedInverter * sharedInverter)
//{
//    mbattery_modelPower->setSharedInverter(sharedInverter);
//}
//
//void DC_charge_controller::run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t)
//{
//    if (mbattery_modelPower->powerPV < 0){
//        mbattery_modelPower->powerPV = 0;
//    }
//
//    // For DC connected system, there is potentially full PV power going through shared inverter
//    mbattery_modelPower->powerPVThroughSharedInverter = mbattery_modelPower->powerPV;
//
//    // Dispatch the battery
//    m_dispatch->dispatch(year, hour_of_year, step_of_hour);
//
//    // Compute annual metrics
//    mbattery_modelMetrics->compute_metrics_ac(m_dispatch->getBatteryPower());
//}
