#include "lib_dispatch_powerflow.h"

#include "lib_battery_dispatch.h"
#include "lib_battery_powerflow.h"
#include "lib_power_electronics.h"
#include "lib_shared_inverter.h"

dispatch_powerflow_state::dispatch_powerflow_state() :
powerPV(0),
powerPVThroughSharedInverter(0),
powerLoad(0),
powerBatteryDC(0),
powerBatteryAC(0),
powerBatteryTarget(0),
powerGrid(0),
powerGeneratedBySystem(0),
powerPVToLoad(0),
powerPVToBattery(0),
powerPVToGrid(0),
powerPVClipped(0),
powerClippedToBattery(0),
powerGridToBattery(0),
powerGridToLoad(0),
powerBatteryToLoad(0),
powerBatteryToGrid(0),
powerFuelCell(0),
powerFuelCellToGrid(0),
powerFuelCellToLoad(0),
powerFuelCellToBattery(0),
powerPVInverterDraw(0),
powerSystemLoss(0),
powerConversionLoss(0),
canPVCharge(false),
canClipCharge(false),
canGridCharge(false),
canDischarge(false),
canFuelCellCharge(false),
time_at_charging_mode(0){

}

void dispatch_powerflow_state::reset()
{
    powerFuelCell = 0;
    powerFuelCellToGrid = 0;
    powerFuelCellToLoad = 0;
    powerFuelCellToBattery = 0;
    powerBatteryDC = 0;
    powerBatteryAC = 0;
    powerBatteryTarget = 0;
    powerBatteryToGrid = 0;
    powerBatteryToLoad = 0;
    powerClippedToBattery = 0;
    powerConversionLoss = 0;
    powerGeneratedBySystem = 0;
    powerGrid = 0;
    powerGridToBattery = 0;
    powerGridToLoad = 0;
    powerLoad = 0;
    powerPV = 0;
    powerPVThroughSharedInverter = 0;
    powerPVClipped = 0;
    powerPVInverterDraw = 0;
    powerPVToBattery = 0;
    powerPVToGrid = 0;
    powerPVToLoad = 0;
    voltageSystem = 0;
}

battery_powerflow::battery_powerflow(battery_power_params p, const battery_properties_params &b, SharedInverter* s) :
params(std::move(p)),
battery_model(new battery(b)),
shared_inverter(s){
    if (params.connection == dispatch_params::CONNECTION::DC_CONNECTED && !shared_inverter)
        throw general_error("Creating a DC-connected battery_powerflow requires a SharedInverter");
}

battery_powerflow::battery_powerflow(const battery_powerflow& f):
params(f.params),
battery_model(f.battery_model),
shared_inverter(f.shared_inverter){
    state = f.state;
}

void battery_powerflow::calculate_powerflow()
{
    if (params.connection == dispatch_params::CONNECTION::AC_CONNECTED) {
        calculateACConnected();
    }
    else if (params.connection == dispatch_params::CONNECTION::DC_CONNECTED) {
        calculateDCConnected();
    }
}

void battery_powerflow::apply_dispatch(const storage_time_state& s, double &target_power) {
    // Ensure the battery operates within the state-of-charge limits
    run_SOC_controller(target_power);

    // Ensure the battery isn't switching rapidly between charging and dischaging
    run_switch_controller(target_power);

    if (target_power == 0){
        return;
    }

    // Calculate current, and ensure the battery falls within the current limits
    double I = get_target_current(target_power);

    // Setup battery iteration
    bool iterate = true;
    size_t count = 0;
//    size_t lifetimeIndex = util::lifetimeIndex(s.year, s.hour, s.step, static_cast<size_t>(1 / params.capacity->time->dt_hour));

    do {

        // Run Battery Model to update current based on charge/discharge
        battery_model->run(s, I);

        // Update how much power was actually used to/from battery
        double battery_voltage_new = battery_model->battery_voltage();
        target_power = I * battery_voltage_new * util::watt_to_kilowatt;

        // Update power flow calculations, calculate AC power, and check the constraints
        calculate_powerflow();
//        iterate = apply_constraints(count, battery_model->get_state());

        // If current changed during last iteration of constraints checker, recalculate internal battery state
        if (!iterate) {
//            finalize(lifetimeIndex, I);
        }

        // Recalculate the DC battery power
        target_power = I * battery_model->battery_voltage() * util::watt_to_kilowatt;
        count++;

    } while (iterate);

    // finalize AC power flow calculation and update for next step
    calculate_powerflow();
//    _prev_charging = _charging;
}

bool battery_powerflow::apply_constraints(size_t& count, battery_state& batt_state)
{
    const double dt_hour = params.capacity->time->dt_hour;

    bool iterate = true;
    bool current_iterate = false;
    bool power_iterate = false;

    double& I = batt_state.capacity.I;
    double& V = batt_state.batt_voltage;
    double I_initial = I;
    double& SOC = batt_state.capacity.SOC;
    double& grid_to_batt = state.powerGridToBattery;

    // don't allow any changes to violate current limits
    if (apply_current_restrictions(I))
    {
        current_iterate = true;
    }
        // don't allow violations of power limits
    else if (apply_power_restrictions(I, V))
    {
        power_iterate = true;
    }
        // decrease the current draw if took too much
    else if (I > 0 && SOC < params.capacity->minimum_SOC - tolerance)
    {
        double dQ = 0.01 * (params.capacity->minimum_SOC - SOC) * batt_state.capacity.qmax_thermal;
        I -= dQ / dt_hour;
    }
        // decrease the current charging if charged too much
    else if (I < 0 && SOC > params.capacity->maximum_SOC + tolerance)
    {
        double dQ = 0.01 * (SOC - params.capacity->maximum_SOC) * batt_state.capacity.qmax_thermal;
        I += dQ / dt_hour;
    }
        // Don't allow grid charging unless explicitly allowed (reduce charging)
    else if (I < 0 && grid_to_batt > tolerance && !state.canGridCharge)
    {
        if (fabs(state.powerBatteryAC) < tolerance)
            I += (grid_to_batt * util::kilowatt_to_watt / batt_state.batt_voltage);
        else
            I += (grid_to_batt / fabs(state.powerBatteryAC)) *fabs(I);
    }
        // Don't allow grid charging if producing PV
    else if (params.connection == dispatch_params::CONNECTION::DC_CONNECTED &&
             grid_to_batt &&
             (state.powerPVToGrid > 0 || state.powerPVToLoad > 0))
    {
        if (fabs(state.powerBatteryAC) < tolerance)
            I += (grid_to_batt * util::kilowatt_to_watt / batt_state.batt_voltage);
        else
            I += (grid_to_batt / fabs(state.powerBatteryAC)) *fabs(I);
    }
        // Don't allow battery to discharge if it gets wasted due to inverter efficiency limitations
        // Typically, this would be due to low power flow, so just cut off battery.
    else if (params.connection == dispatch_params::CONNECTION::DC_CONNECTED &&
             shared_inverter->efficiencyAC < params.inverter_efficiency_cutoff)
    {
        // The requested DC power
        double powerBatterykWdc = I * V * util::watt_to_kilowatt;

        // if battery discharging, see if can back off to get higher efficiency
        if (state.powerBatteryDC > 0) {
            if (powerBatterykWdc + state.powerPV > shared_inverter->getACNameplateCapacitykW()) {
                powerBatterykWdc = shared_inverter->getACNameplateCapacitykW() - state.powerPV;
                powerBatterykWdc = fmax(powerBatterykWdc, 0);
            }
            I = powerBatterykWdc * util::kilowatt_to_watt / V;
        }
            // if charging, this will also be due to low powerflow from grid-charging, just cut off that component
        else if (state.powerBatteryDC < 0 && grid_to_batt > 0){
            I *= fmax(1.0 - fabs(grid_to_batt * shared_inverter->efficiencyAC * 0.01 / state.powerBatteryDC), 0);
        }
        else {
            iterate = false;
        }
    }
    else
        iterate = false;

    // update constraints for current, power, if they are now violated
    if (!current_iterate) {
        current_iterate = apply_current_restrictions(I);
    }
    if (!power_iterate) {
        power_iterate = apply_power_restrictions(I, V);
    }

    // iterate if any of the conditions are met
    if (iterate || current_iterate || power_iterate)
        iterate = true;

    // stop iterating after n tries
    if (count > dispatch_params::constraintCount)
        iterate = false;

    // don't allow battery to flip from charging to discharging or vice versa
    if (fabs(I) > tolerance && (I_initial / I) < 0) {
        I = 0;
        iterate = false;
    }

    // reset
    if (iterate)
    {
//        _Battery->copy(_Battery_initial);
        state.powerBatteryDC = 0;
        state.powerBatteryAC = 0;
        state.powerGridToBattery = 0;
        state.powerBatteryToGrid = 0;
        state.powerPVToGrid = 0;
    }

    return iterate;
}

void battery_powerflow::initialize(double stateOfCharge)
{
    // If the battery is allowed to discharge, do so
    if (state.canDischarge && stateOfCharge > params.capacity->minimum_SOC + 1.0 &&
        (state.powerPV < state.powerLoad || params.meter_position == dispatch_params::FRONT))
    {
        // try to discharge full amount.  Will only use what battery can provide
        state.powerBatteryDC = params.charging.power_discharge_max_kwdc;
    }
        // Is there extra power from system
    else if ((state.powerPV > state.powerLoad && state.canPVCharge) || state.canGridCharge)
    {
        if (state.canPVCharge)
        {
            // use all power available, it will only use what it can handle
            state.powerBatteryDC = -(state.powerPV - state.powerLoad);
        }
        // if we want to charge from grid in addition to, or without array, we can always charge at max power
        if (state.canGridCharge) {
            state.powerBatteryDC = -params.charging.power_discharge_max_kwdc;
        }
    }
}

void battery_powerflow::reset()
{
    state.reset();
}

double battery_powerflow::get_target_current(const double target_power_kw)
{
    double I = 0.; // [W],[V]
    I = util::kilowatt_to_watt * target_power_kw / battery_model->battery_voltage_nominal();
    apply_current_restrictions(I);
    return I;
}

void battery_powerflow::run_switch_controller(double &target_power)
{
    int dt_hour = (int)(round(params.capacity->time->dt_hour * util::hour_to_min));
    const capacity_state cap_state = battery_model->get_state().capacity;
    // Implement rapid switching check
    if (cap_state.charge_mode != cap_state.prev_charge_mode)
    {
        if (state.time_at_charging_mode <= params.charging.minimum_modetime)
        {
            target_power = 0.;
            state.time_at_charging_mode += dt_hour;
        }
        else
            state.time_at_charging_mode = 0;
    }
    state.time_at_charging_mode += dt_hour;
}

void battery_powerflow::run_SOC_controller(double &target_power)
{
    const capacity_state cap_state = battery_model->get_state().capacity;
    bool cut_off = false;
    // Implement minimum SOC cut-off
    if (state.powerBatteryDC > 0)
    {
        if (cap_state.SOC <= params.capacity->minimum_SOC + tolerance) {
            target_power = 0;
        }
    }
        // Maximum SOC cut-off
    else if (state.powerBatteryDC < 0)
    {
        if (cap_state.SOC >= params.capacity->maximum_SOC - tolerance) {
            target_power = 0;
        }
    }
}


bool battery_powerflow::apply_current_restrictions(double& I){
    bool iterate = false;
    if (params.charging.current_choice == dispatch_params::CURRENT_CHOICE::RESTRICT_CURRENT
        || params.charging.current_choice == dispatch_params::CURRENT_CHOICE::RESTRICT_BOTH) {
        if (I < 0) {
            if (fabs(I) > params.charging.current_charge_max) {
                I = -params.charging.current_charge_max;
                iterate = true;
            }
        } else {
            if (I > params.charging.current_discharge_max) {
                I = params.charging.current_discharge_max;
                iterate = true;
            }
        }
    }
    return iterate;
}

bool battery_powerflow::apply_power_restrictions(double& I, double& V){
    bool iterate = false;
    if (params.charging.current_choice == dispatch_params::CURRENT_CHOICE::RESTRICT_POWER ||
            params.charging.current_choice == dispatch_params::CURRENT_CHOICE::RESTRICT_BOTH)
    {
        double powerBattery = I * V * util::watt_to_kilowatt;
        double dP = 0;

        // charging
        if (powerBattery < 0)
        {
            if (fabs(powerBattery) > params.charging.power_charge_max_kwdc * (1 + low_tolerance))
            {
                dP = fabs(params.charging.power_charge_max_kwdc - fabs(powerBattery));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
            else if (params.connection == dispatch_params::CONNECTION::AC_CONNECTED &&
                     fabs(state.powerBatteryAC) > params.charging.power_charge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params.charging.power_charge_max_kwac - fabs(state.powerBatteryAC));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
                // This could just be grid power since that's technically the only AC component.  But, limit all to this
            else if (params.connection == dispatch_params::CONNECTION::DC_CONNECTED &&
                     fabs(state.powerBatteryAC) > params.charging.power_charge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params.charging.power_charge_max_kwac - fabs(state.powerBatteryAC));

                // increase (reduce) charging magnitude by percentage
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
        }
        else
        {
            if (fabs(powerBattery) > params.charging.power_discharge_max_kwdc * (1 + low_tolerance))
            {
                dP = fabs(params.charging.power_discharge_max_kwdc - powerBattery);

                // decrease discharging magnitude
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
            else if (fabs(state.powerBatteryAC) > params.charging.power_discharge_max_kwac * (1 + low_tolerance))
            {
                dP = fabs(params.charging.power_discharge_max_kwac - state.powerBatteryAC);

                // decrease discharging magnitude
                I -= (dP / fabs(powerBattery)) * I;
                iterate = true;
            }
        }
    }
    return iterate;
}

void battery_powerflow::calculateACConnected()
{
    // The battery power is initially a DC power, which must be converted to AC for powerflow
    double P_battery_dc = state.powerBatteryDC;

    // These quantities are all AC quantities in KW unless otherwise specified
    double P_pv_ac = state.powerPV;
    double P_fuelcell_ac = state.powerFuelCell;
    double P_inverter_draw_ac = state.powerPVInverterDraw;
    double P_load_ac = state.powerLoad;
    double P_system_loss_ac = state.powerSystemLoss;
    double P_pv_to_batt_ac, P_grid_to_batt_ac, P_batt_to_load_ac, P_grid_to_load_ac, P_pv_to_load_ac, P_pv_to_grid_ac, P_batt_to_grid_ac, P_gen_ac, P_grid_ac, P_grid_to_batt_loss_ac, P_batt_to_load_loss_ac, P_batt_to_grid_loss_ac,  P_pv_to_batt_loss_ac, P_fuelcell_to_batt_ac, P_fuelcell_to_load_ac, P_fuelcell_to_grid_ac;
    P_pv_to_batt_ac = P_grid_to_batt_ac = P_batt_to_load_ac = P_grid_to_load_ac = P_pv_to_load_ac = P_pv_to_grid_ac = P_batt_to_grid_ac = P_gen_ac = P_grid_ac = P_grid_to_batt_loss_ac = P_batt_to_load_loss_ac = P_batt_to_grid_loss_ac = P_pv_to_batt_loss_ac = P_fuelcell_to_batt_ac = P_fuelcell_to_load_ac = P_fuelcell_to_grid_ac = 0;

    // convert the calculated DC power to AC, considering the microinverter efficiences
    double P_battery_ac = 0;
    if (P_battery_dc < 0)
        P_battery_ac = P_battery_dc / params.ac_dc_efficiency;
    else if (P_battery_dc > 0)
        P_battery_ac = P_battery_dc * params.dc_ac_efficiency;


    // charging
    if (P_battery_ac <= 0)
    {
        // PV always goes to load first
        P_pv_to_load_ac = P_pv_ac;
        if (P_pv_to_load_ac > P_load_ac) {
            P_pv_to_load_ac = P_load_ac;
        }
        // Fuel cell goes to load next
        P_fuelcell_to_load_ac = std::fmin(P_load_ac - P_pv_to_load_ac, P_fuelcell_ac);

        // Excess PV can go to battery
        if (state.canPVCharge){
            P_pv_to_batt_ac = fabs(P_battery_ac);
            if (P_pv_to_batt_ac > P_pv_ac - P_pv_to_load_ac)
            {
                P_pv_to_batt_ac = P_pv_ac - P_pv_to_load_ac;
            }
        }
        // Fuelcell can also charge battery
        if (state.canFuelCellCharge) {
            P_fuelcell_to_batt_ac = std::fmin(std::fmax(0, fabs(P_battery_ac) - P_pv_to_batt_ac), P_fuelcell_ac - P_fuelcell_to_load_ac);
        }
        // Grid can also charge battery
        if (state.canGridCharge){
            P_grid_to_batt_ac = std::fmax(0, fabs(P_battery_ac) - P_pv_to_batt_ac - P_fuelcell_to_batt_ac);
        }

        P_pv_to_grid_ac = P_pv_ac - P_pv_to_batt_ac - P_pv_to_load_ac;
        P_fuelcell_to_grid_ac = P_fuelcell_ac - P_fuelcell_to_load_ac - P_fuelcell_to_batt_ac;

        // Error checking for battery charging
        if (P_pv_to_batt_ac + P_grid_to_batt_ac + P_fuelcell_to_batt_ac != fabs(P_battery_ac)) {
            P_grid_to_batt_ac = fabs(P_battery_ac) - P_pv_to_batt_ac - P_fuelcell_to_batt_ac;
        }
    }
    else
    {
        // Test if battery is discharging erroneously
        if (!state.canDischarge && P_battery_ac > 0) {
            P_batt_to_grid_ac = P_batt_to_load_ac = 0;
            P_battery_ac = 0;
        }
        P_pv_to_load_ac = P_pv_ac;

        // Excess PV production, no other component meets load
        if (P_pv_ac >= P_load_ac)
        {
            P_pv_to_load_ac = P_load_ac;
            P_fuelcell_to_load_ac = 0;
            P_batt_to_load_ac = 0;

            // discharging to grid
            P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;
            P_fuelcell_to_grid_ac = P_fuelcell_ac;
        }
        else {
            P_fuelcell_to_load_ac = std::fmin(P_fuelcell_ac, P_load_ac - P_pv_to_load_ac);
            P_batt_to_load_ac = std::fmin(P_battery_ac, P_load_ac - P_pv_to_load_ac - P_fuelcell_to_load_ac);
        }
        P_batt_to_grid_ac = P_battery_ac - P_batt_to_load_ac;
        P_fuelcell_to_grid_ac = P_fuelcell_ac - P_fuelcell_to_load_ac;
    }

    // compute losses
    P_pv_to_batt_loss_ac = P_pv_to_batt_ac * (1 - params.ac_dc_efficiency);
    P_grid_to_batt_loss_ac = P_grid_to_batt_ac *(1 - params.ac_dc_efficiency);
    P_batt_to_load_loss_ac = P_batt_to_load_ac * (1 / params.dc_ac_efficiency - 1);
    P_batt_to_grid_loss_ac = P_batt_to_grid_ac * (1 / params.dc_ac_efficiency - 1);

    // Compute total system output and grid power flow
    P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac - P_fuelcell_to_load_ac;
    P_gen_ac = P_pv_ac + P_fuelcell_ac + P_battery_ac + P_inverter_draw_ac - P_system_loss_ac;

    // Grid charging loss accounted for in P_battery_ac
    P_grid_ac = P_gen_ac - P_load_ac;

    // Error checking for power to load
    if (P_pv_to_load_ac + P_grid_to_load_ac + P_batt_to_load_ac + P_fuelcell_to_load_ac != P_load_ac)
        P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac - P_fuelcell_to_load_ac;

    // check tolerances
    if (fabs(P_grid_to_load_ac) < tolerance)
        P_grid_to_load_ac = 0;
    if (fabs(P_grid_to_batt_ac) <  tolerance)
        P_grid_to_batt_ac = 0;
    if (fabs(P_grid_ac) <  tolerance)
        P_grid_ac = 0;

    // assign outputs
    state.powerBatteryAC = P_battery_ac;
    state.powerGrid = P_grid_ac;
    state.powerGeneratedBySystem = P_gen_ac;
    state.powerPVToLoad = P_pv_to_load_ac;
    state.powerPVToBattery = P_pv_to_batt_ac;
    state.powerPVToGrid = P_pv_to_grid_ac;
    state.powerGridToBattery = P_grid_to_batt_ac;
    state.powerGridToLoad = P_grid_to_load_ac;
    state.powerBatteryToLoad = P_batt_to_load_ac;
    state.powerBatteryToGrid = P_batt_to_grid_ac;
    state.powerFuelCellToBattery = P_fuelcell_to_batt_ac;
    state.powerFuelCellToLoad= P_fuelcell_to_load_ac;
    state.powerFuelCellToGrid = P_fuelcell_to_grid_ac;
    state.powerConversionLoss = P_batt_to_load_loss_ac + P_batt_to_grid_loss_ac + P_grid_to_batt_loss_ac + P_pv_to_batt_loss_ac;
}

void battery_powerflow::calculateDCConnected()
{
    // Quantities are AC in KW unless otherwise specified
    double P_load_ac = state.powerLoad;
    double P_system_loss_ac = state.powerSystemLoss;
    double P_battery_ac, P_pv_ac, P_gen_ac, P_pv_to_batt_ac, P_grid_to_batt_ac, P_batt_to_load_ac, P_grid_to_load_ac, P_pv_to_load_ac, P_pv_to_grid_ac, P_batt_to_grid_ac, P_grid_ac, P_conversion_loss_ac;
    P_battery_ac = P_pv_ac = P_pv_to_batt_ac = P_grid_to_batt_ac = P_batt_to_load_ac = P_grid_to_load_ac = P_pv_to_load_ac = P_pv_to_grid_ac = P_batt_to_grid_ac = P_gen_ac = P_grid_ac = P_conversion_loss_ac = 0;

    // Quantities are DC in KW unless otherwise specified
    double P_pv_to_batt_dc, P_grid_to_batt_dc, P_pv_to_inverter_dc;
    P_pv_to_batt_dc = P_grid_to_batt_dc = P_pv_to_inverter_dc = 0;

    // The battery power and PV power are initially DC, which must be converted to AC for powerflow
    double P_battery_dc_pre_bms = state.powerBatteryDC;
    double P_battery_dc = state.powerBatteryDC;
    double P_pv_dc = state.powerPV;

    // convert the calculated DC power to DC at the PV system voltage
    if (P_battery_dc_pre_bms < 0)
        P_battery_dc = P_battery_dc_pre_bms / params.dc_dc_bms_efficiency;
    else if (P_battery_dc > 0)
        P_battery_dc = P_battery_dc_pre_bms * params.dc_dc_bms_efficiency;

    double P_gen_dc = P_pv_dc + P_battery_dc;

    // in the event that PV system isn't operating, assume battery BMS converts battery voltage to nominal inverter input at the weighted efficiency
    double voltage = state.voltageSystem;
    double efficiencyDCAC = shared_inverter->efficiencyAC * 0.01;
    if (voltage <= 0) {
        voltage = shared_inverter->getInverterDCNominalVoltage();
    }
    if (std::isnan(efficiencyDCAC) || shared_inverter->efficiencyAC <= 0) {
        efficiencyDCAC = shared_inverter->getMaxPowerEfficiency() * 0.01;
    }


    // charging
    if (P_battery_dc < 0)
    {
        // First check whether battery charging came from PV.
        // Assumes that if battery is charging and can charge from PV, that it will charge from PV before using the grid
        if (state.canPVCharge || state.canClipCharge) {
            P_pv_to_batt_dc = fabs(P_battery_dc);
            if (P_pv_to_batt_dc > P_pv_dc) {
                P_pv_to_batt_dc = P_pv_dc;
            }
        }
        P_pv_to_inverter_dc = P_pv_dc - P_pv_to_batt_dc;

        // Any remaining charge comes from grid regardless of whether allowed or not.
        P_grid_to_batt_dc = fabs(P_battery_dc) - P_pv_to_batt_dc;

        // Assume inverter only "sees" the net flow in one direction, though practically
        // there should never be case where P_pv_dc - P_pv_to_batt_dc > 0 and P_grid_to_batt_dc > 0 simultaneously
        double P_gen_dc_inverter = P_pv_to_inverter_dc - P_grid_to_batt_dc;

        // convert the DC power to AC
        shared_inverter->calculateACPower(P_gen_dc_inverter, voltage, shared_inverter->Tdry_C);
        efficiencyDCAC = shared_inverter->efficiencyAC * 0.01;


        // Restrict low efficiency so don't get infinites
        if (efficiencyDCAC <= 0.05 && (P_grid_to_batt_dc > 0 || P_pv_to_inverter_dc > 0)) {
            efficiencyDCAC = 0.05;
        }
        // This is a traditional DC/AC efficiency loss
        if (P_gen_dc_inverter > 0) {
            shared_inverter->powerAC_kW = P_gen_dc_inverter * efficiencyDCAC;
        }
            // if we are charging from grid, then we actually care about the amount of grid power it took to achieve the DC value
        else {
            shared_inverter->powerAC_kW = P_gen_dc_inverter / efficiencyDCAC;
        }
        shared_inverter->efficiencyAC = efficiencyDCAC * 100;

        // Compute the AC quantities
        P_gen_ac = shared_inverter->powerAC_kW;
        P_grid_to_batt_ac = P_grid_to_batt_dc / efficiencyDCAC;
        P_pv_ac = P_pv_to_inverter_dc * efficiencyDCAC;
        P_pv_to_load_ac = P_load_ac;
        if (P_pv_to_load_ac > P_pv_ac) {
            P_pv_to_load_ac = P_pv_ac;
        }
        P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac;
        P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;

        // In this case, we have a combo of Battery DC power from the PV array, and potentially AC power from the grid
        if (P_pv_to_batt_dc + P_grid_to_batt_ac > 0) {
            P_battery_ac = -(P_pv_to_batt_dc + P_grid_to_batt_ac);
        }

        // Assign this as AC values, even though they are fully DC
        P_pv_to_batt_ac = P_pv_to_batt_dc;
    }
    else
    {
        // convert the DC power to AC
        shared_inverter->calculateACPower(P_gen_dc, voltage, shared_inverter->Tdry_C);
        efficiencyDCAC = shared_inverter->efficiencyAC * 0.01;
        P_gen_ac = shared_inverter->powerAC_kW;

        P_battery_ac = P_battery_dc * efficiencyDCAC;
        P_pv_ac = P_pv_dc * efficiencyDCAC;

        // Test if battery is discharging erroneously
        if (!state.canDischarge && P_battery_ac > 0) {
            P_batt_to_grid_ac = P_batt_to_load_ac = 0;
            P_battery_ac = 0;
        }

        P_pv_to_load_ac = P_pv_ac;
        if (P_pv_ac >= P_load_ac)
        {
            P_pv_to_load_ac = P_load_ac;
            P_batt_to_load_ac = 0;

            // discharging to grid
            P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;
        }
        else {
            P_batt_to_load_ac = std::fmin(P_battery_ac, P_load_ac - P_pv_to_load_ac);
        }
        P_batt_to_grid_ac = P_battery_ac - P_batt_to_load_ac;
    }

    // compute losses
    P_conversion_loss_ac = P_gen_dc - P_gen_ac + P_battery_dc_pre_bms - P_battery_dc;

    // Compute total system output and grid power flow, inverter draw is built into P_pv_ac
    P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac;
    P_gen_ac -= P_system_loss_ac;

    // Grid charging loss accounted for in P_battery_ac
    P_grid_ac = P_gen_ac - P_load_ac;

    // Error checking for power to load
    if (P_pv_to_load_ac + P_grid_to_load_ac + P_batt_to_load_ac != P_load_ac)
        P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac;

    // check tolerances
    if (fabs(P_grid_to_load_ac) < tolerance)
        P_grid_to_load_ac = 0;
    if (fabs(P_grid_to_batt_ac) <  tolerance)
        P_grid_to_batt_ac = 0;
    if (fabs(P_grid_ac) <  tolerance)
        P_grid_ac = 0;

    // assign outputs
    state.powerBatteryAC = P_battery_ac;
    state.powerGrid = P_grid_ac;
    state.powerGeneratedBySystem = P_gen_ac;
    state.powerPVToLoad = P_pv_to_load_ac;
    state.powerPVToBattery = P_pv_to_batt_ac;
    state.powerPVToGrid = P_pv_to_grid_ac;
    state.powerGridToBattery = P_grid_to_batt_ac;
    state.powerGridToLoad = P_grid_to_load_ac;
    state.powerBatteryToLoad = P_batt_to_load_ac;
    state.powerBatteryToGrid = P_batt_to_grid_ac;
    state.powerConversionLoss = P_conversion_loss_ac;
}

