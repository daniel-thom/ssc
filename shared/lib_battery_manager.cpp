#include "lib_battery_manager.h"


battery_manager::battery_manager(var_table& vt, bool setup_model, size_t nrec, double dt_hr, batt_variables *batt_vars_in)
{
    make_vars = false;

    // time quantities
    nyears = 1;
    _dt_hour = dt_hr;
    step_per_hour = static_cast<size_t>(1. / _dt_hour);
    initialize_time(0, 0, 0);

    bool has_fuelcell = false;
    if (auto vd = vt.lookup("fuelcell_power")) {
        fuelcellPower = vd->arr_vector();
        has_fuelcell = true;
    }

    // battery variables
    if (batt_vars_in == 0)
    {
        make_vars = true;
        batt_vars = new batt_variables();


        // fuel cell variables
        batt_vars->en_fuelcell = false;
        if (has_fuelcell) {
            batt_vars->en_fuelcell = true;
            batt_vars->batt_can_fuelcellcharge = vt.as_vector_bool("dispatch_manual_fuelcellcharge");
        }

        batt_vars->en_batt = vt.as_boolean("en_batt");
        if (batt_vars->en_batt)
        {
            // Financial Parameters
            batt_vars->analysis_period = vt.as_integer("analysis_period");

            // Lifetime simulation
            batt_vars->system_use_lifetime_output = vt.as_boolean("system_use_lifetime_output");

            if (batt_vars->system_use_lifetime_output) {
                nyears = batt_vars->analysis_period;
            }

            // Current and capacity
            batt_vars->batt_current_choice = vt.as_integer("batt_current_choice");
            batt_vars->batt_current_charge_max = vt.as_double("batt_current_charge_max");
            batt_vars->batt_current_discharge_max = vt.as_double("batt_current_discharge_max");
            batt_vars->batt_power_charge_max_kwdc = vt.as_double("batt_power_charge_max_kwdc");
            batt_vars->batt_power_discharge_max_kwdc = vt.as_double("batt_power_discharge_max_kwdc");
            batt_vars->batt_power_charge_max_kwac = vt.as_double("batt_power_charge_max_kwac");
            batt_vars->batt_power_discharge_max_kwac = vt.as_double("batt_power_discharge_max_kwac");

            // Power converters and topology
            batt_vars->batt_topology = vt.as_integer("batt_ac_or_dc");
            batt_vars->batt_ac_dc_efficiency = vt.as_double("batt_ac_dc_efficiency");
            batt_vars->batt_dc_ac_efficiency = vt.as_double("batt_dc_ac_efficiency");
            batt_vars->batt_dc_dc_bms_efficiency = vt.as_double("batt_dc_dc_efficiency");

            if (vt.is_assigned("dcoptimizer_loss")) {
                batt_vars->pv_dc_dc_mppt_efficiency = 100. - vt.as_double("dcoptimizer_loss");
            }
            else {
                batt_vars->pv_dc_dc_mppt_efficiency = 100;
            }


            // Charge limits and priority
            batt_vars->batt_initial_SOC = vt.as_double("batt_initial_SOC");
            batt_vars->batt_maximum_SOC = vt.as_double("batt_maximum_soc");
            batt_vars->batt_minimum_SOC = vt.as_double("batt_minimum_soc");
            batt_vars->batt_minimum_modetime = vt.as_double("batt_minimum_modetime");

            // Storage dispatch controllers
            batt_vars->batt_dispatch = vt.as_integer("batt_dispatch_choice");
            batt_vars->batt_meter_position = vt.as_integer("batt_meter_position");

            // Front of meter
            if (batt_vars->batt_meter_position == dispatch_interface::FRONT)
            {

                batt_vars->pv_clipping_forecast = vt.as_vector_double("batt_pv_clipping_forecast");
                batt_vars->pv_dc_power_forecast = vt.as_vector_double("batt_pv_dc_forecast");
                size_t count_ppa_price_input;
                ssc_number_t* ppa_price = vt.as_array("ppa_price_input", &count_ppa_price_input);

//				double ppa_price = cm.as_double("ppa_price_input");
                int ppa_multiplier_mode = vt.as_integer("ppa_multiplier_model");

                if (ppa_multiplier_mode == 0) {
                    batt_vars->ppa_price_series_dollar_per_kwh = flatten_diurnal(
                            vt.as_matrix_unsigned_long("dispatch_sched_weekday"),
                            vt.as_matrix_unsigned_long("dispatch_sched_weekend"),
                            step_per_hour,
                            vt.as_vector_double("dispatch_tod_factors"), ppa_price[0]);
                }
                else {
                    batt_vars->ppa_price_series_dollar_per_kwh = vt.as_vector_double("dispatch_factors_ts");
                    for (size_t i = 0; i < batt_vars->ppa_price_series_dollar_per_kwh.size(); i++) {
                        batt_vars->ppa_price_series_dollar_per_kwh[i] *= ppa_price[0];
                    }
                }
                outMarketPrice = vt.allocate("market_sell_rate_series_yr1",batt_vars->ppa_price_series_dollar_per_kwh.size());
                for (size_t i = 0; i < batt_vars->ppa_price_series_dollar_per_kwh.size(); i++) {
                    outMarketPrice[i] = (ssc_number_t)(batt_vars->ppa_price_series_dollar_per_kwh[i] * 1000.0);
                }

                // For automated front of meter with electricity rates
                batt_vars->ec_rate_defined = false;
                if (vt.is_assigned("en_electricity_rates")) {
                    if (vt.as_integer("en_electricity_rates"))
                    {
                        batt_vars->ec_use_realtime = vt.as_boolean("ur_en_ts_sell_rate");
                        if (!batt_vars->ec_use_realtime) {
                            batt_vars->ec_weekday_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekday");
                            batt_vars->ec_weekend_schedule = vt.as_matrix_unsigned_long("ur_ec_sched_weekend");
                            batt_vars->ec_tou_matrix = vt.as_matrix("ur_ec_tou_mat");
                        }
                        else {
                            batt_vars->ec_realtime_buy = vt.as_vector_double("ur_ts_buy_rate");
                        }
                        batt_vars->ec_rate_defined = true;
                    }
                    else {
                        batt_vars->ec_use_realtime = true;
                        batt_vars->ec_realtime_buy = batt_vars->ppa_price_series_dollar_per_kwh;
                        vt.assign("en_electricity_rates", 1);
                        batt_vars->ec_rate_defined = true;
                    }
                }

                batt_vars->batt_cycle_cost_choice = vt.as_integer("batt_cycle_cost_choice");
                batt_vars->batt_cycle_cost = vt.as_double("batt_cycle_cost");

                if (batt_vars->batt_dispatch == dispatch_interface::FOM_LOOK_AHEAD ||
                    batt_vars->batt_dispatch == dispatch_interface::FOM_FORECAST ||
                    batt_vars->batt_dispatch == dispatch_interface::FOM_LOOK_BEHIND)
                {
                    batt_vars->batt_look_ahead_hours = vt.as_unsigned_long("batt_look_ahead_hours");
                    batt_vars->batt_dispatch_update_frequency_hours = vt.as_double("batt_dispatch_update_frequency_hours");
                }
                else if (batt_vars->batt_dispatch == dispatch_interface::FOM_CUSTOM_DISPATCH)
                {
                    batt_vars->batt_custom_dispatch = vt.as_vector_double("batt_custom_dispatch");
                }
            }
                // Automated behind-the-meter
            else
            {
                if (batt_vars->batt_dispatch == dispatch_interface::MAINTAIN_TARGET)
                {
                    batt_vars->batt_target_choice = vt.as_integer("batt_target_choice");
                    batt_vars->target_power_monthly = vt.as_vector_double("batt_target_power_monthly");
                    batt_vars->target_power = vt.as_vector_double("batt_target_power");

                    if (batt_vars->batt_target_choice == dispatch_automatic_behind_the_meter::TARGET_SINGLE_MONTHLY)
                    {
                        target_power_monthly = batt_vars->target_power_monthly;
                        target_power.clear();
                        target_power.reserve(8760 * step_per_hour);
                        for (size_t month = 0; month != 12; month++)
                        {
                            double target = target_power_monthly[month];
                            for (size_t h = 0; h != util::hours_in_month(month + 1); h++)
                            {
                                for (size_t s = 0; s != step_per_hour; s++)
                                    target_power.push_back(target);
                            }
                        }

                    }
                    else
                        target_power = batt_vars->target_power;

                    if (target_power.size() != nrec)
                        throw exec_error("battery", "invalid number of target powers, must be equal to number of records in weather file");

                    // extend target power to lifetime internally
                    for (size_t y = 1; y < nyears; y++) {
                        for (size_t i = 0; i < nrec; i++) {
                            target_power.push_back(target_power[i]);
                        }
                    }
                    batt_vars->target_power = target_power;

                }
                else if (batt_vars->batt_dispatch == dispatch_interface::CUSTOM_DISPATCH)
                {
                    batt_vars->batt_custom_dispatch = vt.as_vector_double("batt_custom_dispatch");
                }
            }

            // Manual dispatch
            if ((batt_vars->batt_meter_position == dispatch_interface::FRONT && batt_vars->batt_dispatch == dispatch_interface::FOM_MANUAL) ||
                (batt_vars->batt_meter_position == dispatch_interface::BEHIND && batt_vars->batt_dispatch == dispatch_interface::MANUAL))
            {
                batt_vars->batt_can_charge = vt.as_vector_bool("dispatch_manual_charge");
                batt_vars->batt_can_discharge = vt.as_vector_bool("dispatch_manual_discharge");
                batt_vars->batt_can_gridcharge = vt.as_vector_bool("dispatch_manual_gridcharge");
                batt_vars->batt_discharge_percent = vt.as_vector_double("dispatch_manual_percent_discharge");
                batt_vars->batt_gridcharge_percent = vt.as_vector_double("dispatch_manual_percent_gridcharge");
                batt_vars->batt_discharge_schedule_weekday = vt.as_matrix_unsigned_long("dispatch_manual_sched");
                batt_vars->batt_discharge_schedule_weekend = vt.as_matrix_unsigned_long("dispatch_manual_sched_weekend");
            }

            // Common to automated methods
            batt_vars->batt_dispatch_auto_can_charge = true;
            batt_vars->batt_dispatch_auto_can_clipcharge = true;
            batt_vars->batt_dispatch_auto_can_gridcharge = false;
            batt_vars->batt_dispatch_auto_can_fuelcellcharge = true;

            if (vt.is_assigned("batt_dispatch_auto_can_gridcharge")) {
                batt_vars->batt_dispatch_auto_can_gridcharge = vt.as_boolean("batt_dispatch_auto_can_gridcharge");
            }
            if (vt.is_assigned("batt_dispatch_auto_can_charge")) {
                batt_vars->batt_dispatch_auto_can_charge = vt.as_boolean("batt_dispatch_auto_can_charge");
            }
            if (vt.is_assigned("batt_dispatch_auto_can_clipcharge")) {
                batt_vars->batt_dispatch_auto_can_clipcharge = vt.as_boolean("batt_dispatch_auto_can_clipcharge");
            }
            if (vt.is_assigned("batt_dispatch_auto_can_fuelcellcharge")) {
                batt_vars->batt_dispatch_auto_can_fuelcellcharge = vt.as_boolean("batt_dispatch_auto_can_fuelcellcharge");
            }

            // Inverter settings
            if (vt.is_assigned("inverter_model"))
            {
                batt_vars->inverter_model = vt.as_integer("inverter_model");
                batt_vars->inverter_count = vt.as_integer("inverter_count");
                batt_vars->batt_inverter_efficiency_cutoff = vt.as_double("batt_inverter_efficiency_cutoff");

                if (batt_vars->inverter_model == SharedInverter::SANDIA_INVERTER)
                {
                    batt_vars->inverter_efficiency = vt.as_double("inv_snl_eff_cec");
                    batt_vars->inverter_paco = batt_vars->inverter_count * vt.as_double("inv_snl_paco") * util::watt_to_kilowatt;
                }
                else if (batt_vars->inverter_model == SharedInverter::DATASHEET_INVERTER)
                {
                    batt_vars->inverter_efficiency = vt.as_double("inv_ds_eff");
                    batt_vars->inverter_paco = batt_vars->inverter_count * vt.as_double("inv_ds_paco") * util::watt_to_kilowatt;

                }
                else if (batt_vars->inverter_model == SharedInverter::PARTLOAD_INVERTER)
                {
                    batt_vars->inverter_efficiency = vt.as_double("inv_pd_eff");
                    batt_vars->inverter_paco = batt_vars->inverter_count * vt.as_double("inv_pd_paco") * util::watt_to_kilowatt;
                }
                else if (batt_vars->inverter_model == SharedInverter::COEFFICIENT_GENERATOR)
                {
                    batt_vars->inverter_efficiency = vt.as_double("inv_cec_cg_eff_cec");
                    batt_vars->inverter_paco = batt_vars->inverter_count * vt.as_double("inv_cec_cg_paco") * util::watt_to_kilowatt;
                }
            }
            else
            {
                batt_vars->inverter_model = SharedInverter::NONE;
                batt_vars->inverter_count = 1;
                batt_vars->inverter_efficiency = batt_vars->batt_ac_dc_efficiency;
                batt_vars->inverter_paco = batt_vars->batt_kw;
            }
        }
    }
    else
        batt_vars = batt_vars_in;


    battery_model = 0;
    dispatch_model = 0;
    charge_control = 0;
    batt_metrics = 0;

    // outputs
    outTotalCharge = 0;
    outAvailableCharge = 0;
    outBoundCharge = 0;
    outMaxChargeAtCurrent = 0;
    outMaxChargeThermal = 0;
    outMaxCharge = 0;
    outSOC = 0;
    outDOD = 0;
    outDODCycleAverage = 0;
    outCurrent = 0;
    outCellVoltage = 0;
    outBatteryVoltage = 0;
    outCapacityPercent = 0;
    outCycles = 0;
    outBatteryBankReplacement = 0;
    outBatteryTemperature = 0;
    outCapacityThermalPercent = 0;
    outBatteryPower = 0;
    outGridPower = 0;
    outPVToLoad = 0;
    outBatteryToLoad = 0;
    outGridToLoad = 0;
    outFuelCellToLoad = 0;
    outGridPowerTarget = 0;
    outPVToBatt = 0;
    outGridToBatt = 0;
    outFuelCellToBatt = 0;
    outPVToGrid = 0;
    outBatteryToGrid = 0;
    outFuelCellToGrid = 0;
    outBatteryConversionPowerLoss = 0;
    outBatterySystemLoss = 0;
    outAverageCycleEfficiency = 0;
    outPVChargePercent = 0;
    outAnnualPVChargeEnergy = 0;
    outAnnualGridChargeEnergy = 0;
    outAnnualChargeEnergy = 0;
    outAnnualDischargeEnergy = 0;
    outAnnualGridImportEnergy = 0;
    outAnnualGridExportEnergy = 0;
    outCostToCycle = 0;
    outBenefitCharge = 0;
    outBenefitGridcharge = 0;
    outBenefitClipcharge = 0;
    outBenefitDischarge = 0;


    en = setup_model;
    if (!en) return;

    /* **********************************************************************
    Initialize outputs
    ********************************************************************** */

    // non-lifetime outputs
    if (nyears <= 1)
    {
        // only allocate if lead-acid
        if (chem == 0)
        {
            outAvailableCharge = vt.allocate("batt_q1", nrec*nyears);
            outBoundCharge = vt.allocate("batt_q2", nrec*nyears);
        }
        outCellVoltage = vt.allocate("batt_voltage_cell", nrec*nyears);
        outMaxCharge = vt.allocate("batt_qmax", nrec*nyears);
        outMaxChargeThermal = vt.allocate("batt_qmax_thermal", nrec*nyears);
        outBatteryTemperature = vt.allocate("batt_temperature", nrec*nyears);
        outCapacityThermalPercent = vt.allocate("batt_capacity_thermal_percent", nrec*nyears);
    }
    outCurrent = vt.allocate("batt_I", nrec*nyears);
    outBatteryVoltage = vt.allocate("batt_voltage", nrec*nyears);
    outTotalCharge = vt.allocate("batt_q0", nrec*nyears);
    outCycles = vt.allocate("batt_cycles", nrec*nyears);
    outSOC = vt.allocate("batt_SOC", nrec*nyears);
    outDOD = vt.allocate("batt_DOD", nrec*nyears);
    outDODCycleAverage = vt.allocate("batt_DOD_cycle_average", nrec*nyears);
    outCapacityPercent = vt.allocate("batt_capacity_percent", nrec*nyears);
    outCapacityPercentCycle = vt.allocate("batt_capacity_percent_cycle", nrec*nyears);
    outCapacityPercentCalendar = vt.allocate("batt_capacity_percent_calendar", nrec*nyears);
    outBatteryPower = vt.allocate("batt_power", nrec*nyears);
    outGridPower = vt.allocate("grid_power", nrec*nyears); // Net grid energy required.  Positive indicates putting energy on grid.  Negative indicates pulling off grid
    outGenPower = vt.allocate("pv_batt_gen", nrec*nyears);
    outPVToGrid = vt.allocate("pv_to_grid", nrec*nyears);

    if (batt_vars->batt_meter_position == dispatch_interface::BEHIND)
    {
        outPVToLoad = vt.allocate("pv_to_load", nrec*nyears);
        outBatteryToLoad = vt.allocate("batt_to_load", nrec*nyears);
        outGridToLoad = vt.allocate("grid_to_load", nrec*nyears);

        if (batt_vars->batt_dispatch != dispatch_interface::MANUAL)
        {
            outGridPowerTarget = vt.allocate("grid_power_target", nrec*nyears);
            outBattPowerTarget = vt.allocate("batt_power_target", nrec*nyears);
        }
    }
    else if (batt_vars->batt_meter_position == dispatch_interface::FRONT)
    {
        outBatteryToGrid = vt.allocate("batt_to_grid", nrec*nyears);

        if (batt_vars->batt_dispatch != dispatch_interface::FOM_MANUAL) {
            outCostToCycle = vt.allocate("batt_cost_to_cycle", nrec*nyears);
            outBattPowerTarget = vt.allocate("batt_power_target", nrec*nyears);
            outBenefitCharge = vt.allocate("batt_revenue_charge", nrec*nyears);
            outBenefitGridcharge = vt.allocate("batt_revenue_gridcharge", nrec*nyears);
            outBenefitClipcharge = vt.allocate("batt_revenue_clipcharge", nrec*nyears);
            outBenefitDischarge = vt.allocate("batt_revenue_discharge", nrec*nyears);
        }
    }
    outPVToBatt = vt.allocate("pv_to_batt", nrec*nyears);
    outGridToBatt = vt.allocate("grid_to_batt", nrec*nyears);

    if (batt_vars->en_fuelcell) {
        outFuelCellToBatt = vt.allocate("fuelcell_to_batt", nrec*nyears);
        outFuelCellToGrid = vt.allocate("fuelcell_to_grid", nrec*nyears);
        outFuelCellToLoad = vt.allocate("fuelcell_to_load", nrec*nyears);

    }

    outBatteryConversionPowerLoss = vt.allocate("batt_conversion_loss", nrec*nyears);
    outBatterySystemLoss = vt.allocate("batt_system_loss", nrec*nyears);

    // annual outputs
    size_t annual_size = nyears + 1;
    if (nyears == 1){ annual_size = 1; };

    outBatteryBankReplacement = vt.allocate("batt_bank_replacement", annual_size);
    outAnnualChargeEnergy = vt.allocate("batt_annual_charge_energy", annual_size);
    outAnnualDischargeEnergy = vt.allocate("batt_annual_discharge_energy", annual_size);
    outAnnualGridImportEnergy = vt.allocate("annual_import_to_grid_energy", annual_size);
    outAnnualGridExportEnergy = vt.allocate("annual_export_to_grid_energy", annual_size);
    outAnnualEnergySystemLoss = vt.allocate("batt_annual_energy_system_loss", annual_size);
    outAnnualEnergyLoss = vt.allocate("batt_annual_energy_loss", annual_size);
    outAnnualPVChargeEnergy = vt.allocate("batt_annual_charge_from_pv", annual_size);
    outAnnualGridChargeEnergy = vt.allocate("batt_annual_charge_from_grid", annual_size);

    outBatteryBankReplacement[0] = 0;
    outAnnualChargeEnergy[0] = 0;
    outAnnualDischargeEnergy[0] = 0;
    outAnnualGridImportEnergy[0] = 0;
    outAnnualGridExportEnergy[0] = 0;
    outAnnualEnergyLoss[0] = 0;

    auto params = std::shared_ptr<battery_properties_params>(new battery_properties_params());
    storage_time_params time = storage_time_params(nrec/8760, (size_t)vt.as_integer("analysis_period"));
    params->initialize_from_data(vt, time);

    battery_model = std::make_shared<battery>(params);


    batt_metrics = new battery_metrics(dt_hr);

    /*! Process the dispatch options and create the appropriate model */
    if ((batt_vars->batt_meter_position == dispatch_interface::BEHIND && batt_vars->batt_dispatch == dispatch_interface::MANUAL) ||
        (batt_vars->batt_meter_position == dispatch_interface::FRONT && batt_vars->batt_dispatch == dispatch_interface::FOM_MANUAL))
    {
        /*! Generic manual dispatch model inputs */
        if (batt_vars->batt_can_charge.size() != 6 || batt_vars->batt_can_discharge.size() != 6 || batt_vars->batt_can_gridcharge.size() != 6)
            throw exec_error("battery", "invalid manual dispatch control vector lengths, must be length 6");

        if (batt_vars->batt_discharge_schedule_weekday.nrows() != 12 || batt_vars->batt_discharge_schedule_weekday.ncols() != 24)
            throw exec_error("battery", "invalid manual dispatch schedule matrix dimensions, must be 12 x 24");

        if (batt_vars->batt_discharge_schedule_weekend.nrows() != 12 || batt_vars->batt_discharge_schedule_weekend.ncols() != 24)
            throw exec_error("battery", "invalid weekend manual dispatch schedule matrix dimensions, must be 12 x 24");

        if (batt_vars->en_fuelcell) {
            if (batt_vars->batt_can_fuelcellcharge.size() != 6)
                throw exec_error("fuelcell", "invalid manual dispatch control vector lengths, must be length 6");
        }


        size_t discharge_index = 0;
        size_t gridcharge_index = 0;
        for (size_t i = 0; i < batt_vars->batt_can_discharge.size(); i++)
        {
            if (batt_vars->batt_can_discharge[i])
                dm_percent_discharge[i + 1] = batt_vars->batt_discharge_percent[discharge_index++];

            if (batt_vars->batt_can_gridcharge[i])
                dm_percent_gridcharge[i + 1] = batt_vars->batt_gridcharge_percent[gridcharge_index++];
        }
        // Manual Dispatch Model
        if ((batt_vars->batt_meter_position == dispatch_interface::BEHIND && batt_vars->batt_dispatch == dispatch_interface::MANUAL) ||
            (batt_vars->batt_meter_position == dispatch_interface::FRONT && batt_vars->batt_dispatch == dispatch_interface::FOM_MANUAL))
        {
            dispatch_model = new dispatch_manual(battery_model, dt_hr, batt_vars->batt_minimum_SOC, batt_vars->batt_maximum_SOC,
                                                 batt_vars->batt_current_choice,
                                                 batt_vars->batt_current_charge_max, batt_vars->batt_current_discharge_max,
                                                 batt_vars->batt_power_charge_max_kwdc, batt_vars->batt_power_discharge_max_kwdc,
                                                 batt_vars->batt_power_charge_max_kwac, batt_vars->batt_power_discharge_max_kwac,
                                                 batt_vars->batt_minimum_modetime,
                                                 batt_vars->batt_dispatch, batt_vars->batt_meter_position,
                                                 batt_vars->batt_discharge_schedule_weekday, batt_vars->batt_discharge_schedule_weekend,
                                                 batt_vars->batt_can_charge, batt_vars->batt_can_discharge, batt_vars->batt_can_gridcharge, batt_vars->batt_can_fuelcellcharge,
                                                 dm_percent_discharge, dm_percent_gridcharge);
        }
    }
        /*! Front of meter automated DC-connected dispatch */
    else if (batt_vars->batt_meter_position == dispatch_interface::FRONT)
    {
        double eta_discharge = batt_vars->batt_dc_dc_bms_efficiency * 0.01 * batt_vars->inverter_efficiency;
        double eta_pvcharge = batt_vars->batt_dc_dc_bms_efficiency;
        double eta_gridcharge = batt_vars->batt_dc_dc_bms_efficiency * 0.01 * batt_vars->inverter_efficiency;
        if (batt_vars->batt_topology == ChargeController::AC_CONNECTED) {
            eta_discharge = batt_vars->batt_dc_ac_efficiency;
            eta_pvcharge = batt_vars->batt_ac_dc_efficiency;
            eta_gridcharge = batt_vars->batt_ac_dc_efficiency;
        }

        // Create UtilityRate object only if utility rate is defined
        utilityRate = NULL;
        if (batt_vars->ec_rate_defined) {
            utilityRate = new UtilityRate(batt_vars->ec_use_realtime, batt_vars->ec_weekday_schedule, batt_vars->ec_weekend_schedule, batt_vars->ec_tou_matrix, batt_vars->ec_realtime_buy);
        }
        dispatch_model = new dispatch_automatic_front_of_meter(battery_model, dt_hr, batt_vars->batt_minimum_SOC, batt_vars->batt_maximum_SOC,
                                                               batt_vars->batt_current_choice, batt_vars->batt_current_charge_max, batt_vars->batt_current_discharge_max,
                                                               batt_vars->batt_power_charge_max_kwdc, batt_vars->batt_power_discharge_max_kwdc,
                                                               batt_vars->batt_power_charge_max_kwac, batt_vars->batt_power_discharge_max_kwac,
                                                               batt_vars->batt_minimum_modetime,
                                                               batt_vars->batt_dispatch, batt_vars->batt_meter_position,
                                                               nyears, batt_vars->batt_look_ahead_hours, batt_vars->batt_dispatch_update_frequency_hours,
                                                               batt_vars->batt_dispatch_auto_can_charge, batt_vars->batt_dispatch_auto_can_clipcharge, batt_vars->batt_dispatch_auto_can_gridcharge, batt_vars->batt_dispatch_auto_can_fuelcellcharge,
                                                               batt_vars->inverter_paco, batt_vars->batt_cost_per_kwh,
                                                               batt_vars->batt_cycle_cost_choice, batt_vars->batt_cycle_cost,
                                                               batt_vars->ppa_price_series_dollar_per_kwh, utilityRate,
                                                               eta_pvcharge, eta_gridcharge , eta_discharge);


        if (batt_vars->batt_dispatch == dispatch_interface::CUSTOM_DISPATCH)
        {
            if (auto dispatch_fom = dynamic_cast<dispatch_automatic_front_of_meter*>(dispatch_model))
            {
                if (batt_vars->batt_custom_dispatch.size() != 8760 * step_per_hour) {
                    throw exec_error("battery", "invalid custom dispatch, must be 8760 * steps_per_hour");
                }
                auto dispatch_fom_old = dynamic_cast<dispatch_automatic_front_of_meter_t*>(dispatch_model);
                dispatch_fom_old->set_custom_dispatch(batt_vars->batt_custom_dispatch);

                dispatch_fom->set_custom_dispatch(batt_vars->batt_custom_dispatch);
            }
        }

    }
        /*! Behind-the-meter automated dispatch for peak shaving */
    else
    {
        dispatch_model = new dispatch_automatic_behind_the_meter(battery_model, dt_hr, batt_vars->batt_minimum_SOC, batt_vars->batt_maximum_SOC,
                                                                 batt_vars->batt_current_choice, batt_vars->batt_current_charge_max, batt_vars->batt_current_discharge_max,
                                                                 batt_vars->batt_power_charge_max_kwdc, batt_vars->batt_power_discharge_max_kwdc,
                                                                 batt_vars->batt_power_charge_max_kwac, batt_vars->batt_power_discharge_max_kwac,
                                                                 batt_vars->batt_minimum_modetime,
                                                                 batt_vars->batt_dispatch, batt_vars->batt_meter_position, nyears,
                                                                 batt_vars->batt_look_ahead_hours, batt_vars->batt_dispatch_update_frequency_hours,
                                                                 batt_vars->batt_dispatch_auto_can_charge, batt_vars->batt_dispatch_auto_can_clipcharge, batt_vars->batt_dispatch_auto_can_gridcharge, batt_vars->batt_dispatch_auto_can_fuelcellcharge
        );

        if (batt_vars->batt_dispatch == dispatch_interface::CUSTOM_DISPATCH)
        {
            if (auto dispatch_btm = dynamic_cast<dispatch_automatic_behind_the_meter*>(dispatch_model))
            {
                if (batt_vars->batt_custom_dispatch.size() != 8760 * step_per_hour) {
                    throw exec_error("battery", "invalid custom dispatch, must be 8760 * steps_per_hour");
                }
                dispatch_btm->set_custom_dispatch(batt_vars->batt_custom_dispatch);
            }
        }
    }

    if (batt_vars->batt_topology == ChargeController::AC_CONNECTED) {
        charge_control = new AC_charge_controller(dispatch_model, batt_metrics, batt_vars->batt_ac_dc_efficiency, batt_vars->batt_dc_ac_efficiency);
    }
    else if (batt_vars->batt_topology == ChargeController::DC_CONNECTED) {
        charge_control = new DC_charge_controller(dispatch_model, batt_metrics, batt_vars->batt_dc_dc_bms_efficiency, batt_vars->batt_inverter_efficiency_cutoff);
    }

    parse_configuration();
}

void battery_manager::parse_configuration()
{
    int batt_dispatch = batt_vars->batt_dispatch;
    int batt_meter_position = batt_vars->batt_meter_position;

    // parse configuration
    if (dynamic_cast<dispatch_automatic*>(dispatch_model))
    {
        prediction_index = 0;
        if (batt_meter_position == dispatch_interface::BEHIND)
        {
            if (batt_dispatch == dispatch_interface::LOOK_AHEAD || batt_dispatch == dispatch_interface::MAINTAIN_TARGET)
            {
                look_ahead = true;
                if (batt_dispatch == dispatch_interface::MAINTAIN_TARGET)
                    input_target = true;
            }
            else if (batt_dispatch == dispatch_interface::CUSTOM_DISPATCH)
            {
                input_custom_dispatch = true;
            }
            else
                look_behind = true;
        }
        else if (batt_meter_position == dispatch_interface::FRONT)
        {
            if (batt_dispatch == dispatch_interface::FOM_LOOK_AHEAD) {
                look_ahead = true;
            }
            else if (batt_dispatch == dispatch_interface::FOM_LOOK_BEHIND) {
                look_behind = true;
            }
            else if (batt_dispatch == dispatch_interface::FOM_FORECAST) {
                input_forecast = true;
            }
            else if (batt_dispatch == dispatch_interface::FOM_CUSTOM_DISPATCH) {
                input_custom_dispatch = true;
            }
        }
    }
    else
        manual_dispatch = true;
}

void battery_manager::initialize_automated_dispatch(std::vector<ssc_number_t> pv, std::vector<ssc_number_t> load, std::vector<ssc_number_t> cliploss)
{
    if (dynamic_cast<dispatch_automatic*>(dispatch_model))
    {
        // automatic look ahead or behind
        size_t nrec = nyears * 8760 * step_per_hour;
        if (!input_custom_dispatch)
        {
            // look ahead
            if (look_ahead)
            {
                if (pv.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec; idx++) {
                        pv_prediction.push_back(pv[idx]);
                    }

                }
                if (load.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec; idx++) {
                        load_prediction.push_back(load[idx]);
                    }
                }
                if (cliploss.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec; idx++) {
                        cliploss_prediction.push_back(cliploss[idx]);
                    }
                }
            }
            else if (look_behind)
            {
                // day one is zeros
                for (size_t idx = 0; idx != 24 * step_per_hour; idx++)
                {
                    pv_prediction.push_back(0);
                    load_prediction.push_back(0);
                    cliploss_prediction.push_back(0);
                }

                if (pv.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec - 24 * step_per_hour; idx++){
                        pv_prediction.push_back(pv[idx]);
                    }
                }
                if (load.size() !=0 )
                {
                    for (size_t idx = 0; idx != nrec - 24 * step_per_hour; idx++) {
                        load_prediction.push_back(load[idx]);
                    }
                }
                if (cliploss.size() !=0 )
                {
                    for (size_t idx = 0; idx != nrec - 24 * step_per_hour; idx++) {
                        cliploss_prediction.push_back(cliploss[idx]);
                    }
                }
            }
            else if (input_forecast)
            {
                if (pv.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec; idx++) {
                        pv_prediction.push_back(pv[idx]);
                    }
                }
                if (cliploss.size() != 0)
                {
                    for (size_t idx = 0; idx != nrec; idx++) {
                        cliploss_prediction.push_back(cliploss[idx]);
                    }
                }
            }
            // Input checking
            if (pv.size() == 0)
            {

                for (size_t idx = 0; idx != nrec; idx++)				{
                    pv_prediction.push_back(0.);
                }
            }
            if (load.size() == 0)
            {
                for (size_t idx = 0; idx != nrec; idx++) {
                    load_prediction.push_back(0.);
                }
            }
            if (cliploss.size() == 0)
            {
                for (size_t idx = 0; idx != nrec; idx++) {
                    cliploss_prediction.push_back(0.);
                }
            }

            if (dispatch_automatic_behind_the_meter * automatic_dispatch_btm = dynamic_cast<dispatch_automatic_behind_the_meter*>(dispatch_model))
            {
                automatic_dispatch_btm->update_pv_data(pv_prediction);
                automatic_dispatch_btm->update_load_data(load_prediction);

                if (input_target){
                    automatic_dispatch_btm->set_target_power(target_power);
                }
            }
            else if (dispatch_automatic_front_of_meter * automatic_dispatch_fom = dynamic_cast<dispatch_automatic_front_of_meter*>(dispatch_model))
            {
                automatic_dispatch_fom->update_pv_data(pv_prediction);
                automatic_dispatch_fom->update_cliploss_data(cliploss_prediction);
            }
        }
    }

}
battery_manager::~battery_manager()
{
    if (batt_metrics) delete batt_metrics;
    if( dispatch_model ) delete dispatch_model;
    if (charge_control) delete charge_control;
    if (make_vars) delete batt_vars;
}

battery_manager::battery_manager(const battery_manager& orig){
    throw std::runtime_error("need to fix");
    if (orig.batt_vars) batt_vars = orig.batt_vars;

    if( orig.battery_model )  battery_model = std::shared_ptr<battery>(new battery(*orig.battery_model));
    if (orig.dispatch_model){
        if (auto disp = dynamic_cast<dispatch_manual*>(orig.dispatch_model))
            dispatch_model = new dispatch_manual(*disp);
        else if (auto disp = dynamic_cast<dispatch_automatic_behind_the_meter*>(orig.dispatch_model))
            dispatch_model = new dispatch_automatic_behind_the_meter(*disp);
        else if (auto disp = dynamic_cast<dispatch_automatic_front_of_meter*>(orig.dispatch_model))
            dispatch_model = new dispatch_automatic_front_of_meter(*disp);
        else
            throw general_error("dispatch_model in battery_manager is not of recognized type.");
    }
    batt_metrics = new battery_metrics(orig._dt_hour);
    if (orig.charge_control){
        if (dynamic_cast<ACBatteryController*>(orig.charge_control))
            charge_control = new AC_charge_controller(dispatch_model, batt_metrics, batt_vars->batt_ac_dc_efficiency,
                                                      batt_vars->batt_dc_ac_efficiency);
        else if (dynamic_cast<DCBatteryController*>(orig.charge_control))
            charge_control = new DC_charge_controller(dispatch_model, batt_metrics, batt_vars->batt_dc_ac_efficiency,
                                                      batt_vars->batt_inverter_efficiency_cutoff);
        else
            throw general_error("charge_control in battery_manager is not of recognized type.");
    }
}

void battery_manager::initialize_time(size_t year_in, size_t hour_of_year, size_t step_of_hour)
{
    step = step_of_hour;
    hour = hour_of_year;
    year = year_in;
    index = (year * 8760 + hour) * step_per_hour + step;
    year_index = (hour * step_per_hour) + step;
    step_per_year = 8760 * step_per_hour;
}
void battery_manager::advance(var_table *vt, double P_gen, double V_gen, double P_load, double P_gen_clipped )
{
    BatteryPower * powerflow = dispatch_model->getBatteryPower();

    powerflow->reset();

    if (index < fuelcellPower.size()) {
        powerflow->powerFuelCell = fuelcellPower[index];
    }

    powerflow->powerGeneratedBySystem = P_gen;
    powerflow->powerPV = P_gen - powerflow->powerFuelCell;
    powerflow->powerLoad = P_load;
    powerflow->voltageSystem = V_gen;
    powerflow->powerPVClipped = P_gen_clipped;

    charge_control->run(year, hour, step, year_index);

    outputs_fixed(vt);
    outputs_topology_dependent();
    metrics();
}
void battery_manager::setSharedInverter(SharedInverter * sharedInverter)
{
    if (DCBatteryController * tmp = dynamic_cast<DCBatteryController *>(charge_control))
        tmp->setSharedInverter(sharedInverter);
}
void battery_manager::outputs_fixed(var_table *vt)
{
    // non-lifetime outputs
    if (nyears <= 1)
    {
        // Capacity Output with Losses Applied
        if (params->chem == battery_properties_params::LEAD_ACID )
        {
            outAvailableCharge[index] = battery_model->get_q0();
            outBoundCharge[index] = battery_model->get_q2();
        }
        outCellVoltage[index] = battery_model->get_V_cell();
        outMaxCharge[index] = battery_model->get_qmax_lifetime();
        outMaxChargeThermal[index] = battery_model->get_qmax_thermal();

        outBatteryTemperature[index] = battery_model->get_T_K() - 273.15;
        outCapacityThermalPercent[index] = battery_model->get_capacity_percent_thermal();
    }

    // Lifetime outputs
    outTotalCharge[index] = battery_model->get_q0();
    outCurrent[index] = battery_model->get_I();
    outBatteryVoltage[index] = battery_model->get_V();

    outCycles[index] = battery_model->get_cycles_elapsed();
    outSOC[index] = battery_model->get_SOC();
    outDOD[index] = battery_model->get_cycle_range();
    outDODCycleAverage[index] = battery_model->get_avg_cycle_range();
    outCapacityPercent[index] = battery_model->get_capacity_percent_lifetime();
    outCapacityPercentCycle[index] = battery_model->get_capacity_percent_cycle();
    outCapacityPercentCalendar[index] = battery_model->get_capacity_percent_calendar();

}

void battery_manager::outputs_topology_dependent()
{
    // Power output (all Powers in kWac)
    outBatteryPower[index] = dispatch_model->power_tofrom_battery();
    outGridPower[index] = dispatch_model->power_tofrom_grid();
    outGenPower[index] = dispatch_model->power_gen();
    outPVToBatt[index] = dispatch_model->power_pv_to_batt();
    outGridToBatt[index] = dispatch_model->power_grid_to_batt();

    // Fuel cell updates
    if (batt_vars->en_fuelcell) {
        outFuelCellToLoad[index] = dispatch_model->power_fuelcell_to_load();
        outFuelCellToBatt[index] = dispatch_model->power_fuelcell_to_batt();
        outFuelCellToGrid[index] = dispatch_model->power_fuelcell_to_grid();
    }
    outBatteryConversionPowerLoss[index] = dispatch_model->power_conversion_loss();
    outBatterySystemLoss[index] = dispatch_model->power_system_loss();
    outPVToGrid[index] = dispatch_model->power_pv_to_grid();

    if (batt_vars->batt_meter_position == dispatch_interface::BEHIND)
    {
        outPVToLoad[index] = dispatch_model->power_pv_to_load();
        outBatteryToLoad[index] = dispatch_model->power_battery_to_load();
        outGridToLoad[index] = dispatch_model->power_grid_to_load();

        if (batt_vars->batt_dispatch != dispatch_interface::MANUAL)
        {
            outGridPowerTarget[index] = dispatch_model->power_grid_target();
            outBattPowerTarget[index] = dispatch_model->power_batt_target();
        }

    }
    else if (batt_vars->batt_meter_position == dispatch_interface::FRONT)
    {
        outBatteryToGrid[index] = dispatch_model->power_battery_to_grid();

        if (batt_vars->batt_dispatch != dispatch_interface::FOM_MANUAL) {
            dispatch_automatic_front_of_meter * dispatch_fom = dynamic_cast<dispatch_automatic_front_of_meter *>(dispatch_model);
            outCostToCycle[index] = dispatch_model->cost_to_cycle();
            outBattPowerTarget[index] = dispatch_model->power_batt_target();
            outBenefitCharge[index] = dispatch_fom->benefit_charge();
            outBenefitDischarge[index] = dispatch_fom->benefit_discharge();
            outBenefitClipcharge[index] = dispatch_fom->benefit_clipcharge();
            outBenefitGridcharge[index] = dispatch_fom->benefit_gridcharge();
        }
    }
}

void battery_manager::metrics()
{
    size_t annual_index;
    nyears > 1 ? annual_index = year + 1 : annual_index = 0;
    outBatteryBankReplacement[annual_index] = battery_model->get_n_replacements();

    if ((hour == 8759) && (step == step_per_hour - 1))
    {
        battery_model->reset_replacements();
        outAnnualGridImportEnergy[annual_index] = batt_metrics->energy_grid_import_annual();
        outAnnualGridExportEnergy[annual_index] = batt_metrics->energy_grid_export_annual();
        outAnnualPVChargeEnergy[annual_index] = batt_metrics->energy_pv_charge_annual();
        outAnnualGridChargeEnergy[annual_index] = batt_metrics->energy_grid_charge_annual();
        outAnnualChargeEnergy[annual_index] = batt_metrics->energy_charge_annual();
        outAnnualDischargeEnergy[annual_index] = batt_metrics->energy_discharge_annual();
        outAnnualEnergyLoss[annual_index] = batt_metrics->energy_loss_annual();
        outAnnualEnergySystemLoss[annual_index] = batt_metrics->energy_system_loss_annual();
        batt_metrics->new_year();
    }

    // Average battery conversion efficiency
    outAverageCycleEfficiency = batt_metrics->average_battery_conversion_efficiency();
    if (outAverageCycleEfficiency > 100)
        outAverageCycleEfficiency = 100;
    else if (outAverageCycleEfficiency < 0)
        outAverageCycleEfficiency = 0;

    // Average battery roundtrip efficiency
    outAverageRoundtripEfficiency = batt_metrics->average_battery_roundtrip_efficiency();
    if (outAverageRoundtripEfficiency > 100)
        outAverageRoundtripEfficiency = 100;
    else if (outAverageRoundtripEfficiency < 0)
        outAverageRoundtripEfficiency = 0;

    // PV charge ratio
    outPVChargePercent = batt_metrics->pv_charge_percent();
    if (outPVChargePercent > 100)
        outPVChargePercent = 100;
    else if (outPVChargePercent < 0)
        outPVChargePercent = 0;
}

// function needed to correctly calculate P_grid to to additional losses in P_gen post battery like wiring, curtailment, availability
void battery_manager::update_grid_power(compute_module &, double P_gen_ac, double P_load_ac, size_t index_replace)
{
    double P_grid = P_gen_ac - P_load_ac;
    outGridPower[index_replace] = P_grid;
}

void battery_manager::calculate_monthly_and_annual_outputs( compute_module &cm )
{
    // single value metrics
    cm.assign("average_battery_conversion_efficiency", var_data( (ssc_number_t) outAverageCycleEfficiency ));
    cm.assign("average_battery_roundtrip_efficiency", var_data((ssc_number_t)outAverageRoundtripEfficiency));
    cm.assign("batt_pv_charge_percent", var_data((ssc_number_t)outPVChargePercent));
    cm.assign("batt_bank_installed_capacity", (ssc_number_t)batt_vars->batt_kwh);

    // monthly outputs
    cm.accumulate_monthly_for_year("pv_to_batt", "monthly_pv_to_batt", _dt_hour, step_per_hour);
    cm.accumulate_monthly_for_year("grid_to_batt", "monthly_grid_to_batt", _dt_hour, step_per_hour);
    cm.accumulate_monthly_for_year("pv_to_grid", "monthly_pv_to_grid", _dt_hour, step_per_hour);

    if (batt_vars->batt_meter_position == dispatch_interface::BEHIND)
    {
        cm.accumulate_monthly_for_year("pv_to_load", "monthly_pv_to_load", _dt_hour, step_per_hour);
        cm.accumulate_monthly_for_year("batt_to_load", "monthly_batt_to_load", _dt_hour, step_per_hour);
        cm.accumulate_monthly_for_year("grid_to_load", "monthly_grid_to_load", _dt_hour, step_per_hour);
    }
    else if (batt_vars->batt_meter_position == dispatch_interface::FRONT)
    {
        cm.accumulate_monthly_for_year("batt_to_grid", "monthly_batt_to_grid", _dt_hour, step_per_hour);
    }
}
