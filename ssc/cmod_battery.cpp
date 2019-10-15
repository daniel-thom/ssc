/**
BSD-3-Clause
Copyright 2019 Alliance for Sustainable Energy, LLC
Redistribution and use in source and binary forms, with or without modification, are permitted provided 
that the following conditions are met :
1.	Redistributions of source code must retain the above copyright notice, this list of conditions 
and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.	Neither the name of the copyright holder nor the names of its contributors may be used to endorse 
or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER, CONTRIBUTORS, UNITED STATES GOVERNMENT OR UNITED STATES 
DEPARTMENT OF ENERGY, NOR ANY OF THEIR EMPLOYEES, BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>

#include "cmod_battery.h"
#include "common.h"
#include "core.h"
#include "lib_storage_params.h"
#include "lib_battery_model.h"
#include "lib_dispatch.h"
#include "lib_battery_powerflow.h"
#include "lib_battery_controller.h"
#include "lib_power_electronics.h"
#include "lib_battery_controller.h"
#include "lib_shared_inverter.h"
#include "lib_time.h"
#include "lib_util.h"
#include "lib_utility_rate.h"

var_info vtab_battery_inputs[] = {
	/*   VARTYPE           DATATYPE         NAME                                            LABEL                                                   UNITS      META                   GROUP           REQUIRED_IF                 CONSTRAINTS                      UI_HINTS*/

	// simulation inputs - required only if lifetime analysis
	{ SSC_INPUT,        SSC_NUMBER,      "system_use_lifetime_output",                 "PV lifetime simulation",                                  "0/1",     "0=SingleYearRepeated,1=RunEveryYear",                     "Simulation",             "?=0",                        "BOOLEAN",                        "" },
	{ SSC_INPUT,        SSC_NUMBER,      "analysis_period",                            "Lifetime analysis period",                                "years",   "The number of years in the simulation",                   "Simulation",             "system_use_lifetime_output =1",   "",                               "" },

	// configuration inputs
	{ SSC_INPUT,        SSC_NUMBER,      "batt_chem",                                  "Battery chemistry",                                       "",        "0=LeadAcid,1=LiIon,2=VanadiumRedox,3=IronFlow",   "Battery",       "",                           "MIN=0,MAX=3",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inverter_model",                             "Inverter model specifier",                                "",        "0=cec,1=datasheet,2=partload,3=coefficientgenerator,4=generic","","", "INTEGER,MIN=0,MAX=4",           "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inverter_count",                             "Number of inverters",                                     "",        "",                     "Inverter"        "",                            "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_snl_eff_cec",                            "Inverter Sandia CEC Efficiency",                          "%",       "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_snl_paco",                               "Inverter Sandia Maximum AC Power",                        "Wac",     "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_ds_eff",                                 "Inverter Datasheet Efficiency",                           "%",       "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_ds_paco",                                "Inverter Datasheet Maximum AC Power",                     "Wac",     "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_pd_eff",                                 "Inverter Partload Efficiency",                            "%",       "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_pd_paco",                                "Inverter Partload Maximum AC Power",                      "Wac",     "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_cec_cg_eff_cec",                         "Inverter Coefficient Generator CEC Efficiency",           "%",       "",                     "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "inv_cec_cg_paco",                            "Inverter Coefficient Generator Max AC Power",             "Wac",       "",                   "Inverter",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_ac_or_dc",                              "Battery interconnection (AC or DC)",                      "",        "0=DC_Connected,1=AC_Connected",                  "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_dc_dc_efficiency",                      "PV DC to battery DC efficiency",                          "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "dcoptimizer_loss",                           "PV loss in DC/DC w/MPPT conversion",                      "",        "",                     "PV",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_dc_ac_efficiency",                      "Battery DC to AC efficiency",                             "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_ac_dc_efficiency",                      "Inverter AC to battery DC efficiency",                    "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_meter_position",                        "Position of battery relative to electric meter",          "",        "0=BehindTheMeter,1=FrontOfMeter",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_inverter_efficiency_cutoff",            "Inverter efficiency at which to cut battery charge or discharge off",          "%",        "","Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_ARRAY,       "batt_losses",                                "Battery system losses at each timestep",                  "kW",       "",                     "Battery",       "?=0",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,       "batt_losses_charging",                       "Battery system losses when charging",                     "kW",       "",                     "Battery",       "?=0",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,       "batt_losses_discharging",                    "Battery system losses when discharging",                  "kW",       "",                     "Battery",       "?=0",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,       "batt_losses_idle",                           "Battery system losses when idle",                         "kW",       "",                     "Battery",       "?=0",                        "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_loss_choice",                           "Loss power input option",                                 "0/1",      "0=Monthly,1=TimeSeries",                     "Battery",       "?=0",                        "MIN=0,MAX=1",                             "" },

	// Current and capacity battery inputs
	{ SSC_INPUT,        SSC_NUMBER,      "batt_current_choice",                        "Limit cells by current or power",                         "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_computed_strings",                      "Number of strings of cells",                              "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_computed_series",                       "Number of cells in series",                               "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_computed_bank_capacity",                "Computed bank capacity",                                  "kWh",     "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_current_charge_max",                    "Maximum charge current",                                  "A",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_current_discharge_max",                 "Maximum discharge current",                               "A",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_power_charge_max_kwdc",                 "Maximum charge power (DC)",                               "kWdc",    "",                    "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_power_discharge_max_kwdc",              "Maximum discharge power (DC)",                            "kWdc",    "",                    "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_power_charge_max_kwac",                 "Maximum charge power (AC)",                               "kWac",    "",                    "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_power_discharge_max_kwac",              "Maximum discharge power (AC)",                            "kWac",    "",                    "Battery",       "",                           "",                              "" },


	// Voltage discharge curve
	{ SSC_INPUT,        SSC_NUMBER,      "batt_voltage_choice",                        "Battery voltage input option",                            "0/1",      "0=UseVoltageModel,1=InputVoltageTable",                    "Battery",       "?=0",                        "MIN=0,MAX=1",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Vfull",                                 "Fully charged cell voltage",                              "V",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Vexp",                                  "Cell voltage at end of exponential zone",                 "V",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Vnom",                                  "Cell voltage at end of nominal zone",                     "V",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Vnom_default",                          "Default nominal cell voltage",                            "V",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Qfull",                                 "Fully charged cell capacity",                             "Ah",      "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Qfull_flow",                            "Fully charged flow battery capacity",                     "Ah",      "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Qexp",                                  "Cell capacity at end of exponential zone",                "Ah",      "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_Qnom",                                  "Cell capacity at end of nominal zone",                    "Ah",      "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_C_rate",                                "Rate at which voltage vs. capacity curve input",          "",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_resistance",                            "Internal resistance",                                     "Ohm",     "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,		SSC_MATRIX,      "batt_voltage_matrix",                        "Battery voltage vs. depth-of-discharge",                 "",         "",                     "Battery",       "",                           "",                             "" },

	// lead-acid inputs
	{ SSC_INPUT,		SSC_NUMBER,		"LeadAcid_q20_computed",	                   "Capacity at 20-hour discharge rate",                     "Ah",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,		SSC_NUMBER,		"LeadAcid_q10_computed",	                   "Capacity at 10-hour discharge rate",                     "Ah",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,		SSC_NUMBER,		"LeadAcid_qn_computed",	                       "Capacity at discharge rate for n-hour rate",             "Ah",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,		SSC_NUMBER,		"LeadAcid_tn",	                               "Time to discharge",                                      "h",        "",                     "Battery",       "",                           "",                             "" },

	// charge limits and priority inputs
	{ SSC_INPUT,        SSC_NUMBER,      "batt_initial_SOC",		                   "Initial state-of-charge",                                 "%",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_minimum_SOC",		                   "Minimum allowed state-of-charge",                         "%",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_maximum_SOC",                           "Maximum allowed state-of-charge",                         "%",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_minimum_modetime",                      "Minimum time at charge state",                            "min",     "",                     "Battery",       "",                           "",                              "" },

	// lifetime inputs
	{ SSC_INPUT,		SSC_MATRIX,     "batt_lifetime_matrix",                        "Cycles vs capacity at different depths-of-discharge",    "",         "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_calendar_choice",                        "Calendar life degradation input option",                 "0/1/2",    "0=NoCalendarDegradation,1=LithiomIonModel,2=InputLossTable",                     "Battery",       "",                           "MIN=0,MAX=2",                             "" },
	{ SSC_INPUT,        SSC_MATRIX,     "batt_calendar_lifetime_matrix",               "Days vs capacity",                                       "",         "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_calendar_q0",                            "Calendar life model initial capacity cofficient",        "",         "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_calendar_a",                             "Calendar life model coefficient",                        "1/sqrt(day)","",                   "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_calendar_b",                             "Calendar life model coefficient",                        "K",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_calendar_c",                             "Calendar life model coefficient",                        "K",        "",                     "Battery",       "",                           "",                             "" },

	// replacement inputs
	{ SSC_INPUT,        SSC_NUMBER,     "batt_replacement_capacity",                   "Capacity degradation at which to replace battery",       "%",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_replacement_option",                     "Enable battery replacement?",                             "0=none,1=capacity based,2=user schedule", "", "Battery", "?=0",                  "INTEGER,MIN=0,MAX=2",          "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_replacement_schedule",                   "Number of Battery bank replacements per year (user specified)", "number/year","",                  "Battery",      "batt_replacement_option=2",   "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_replacement_schedule_percent",           "Max percentage of nominal battery capacity to replace in year",      "%","",                  "Battery",      "batt_replacement_option=2",   "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "om_replacement_cost1",                        "Cost to replace battery per kWh",                        "$/kWh",    "",                     "Battery",       "",                           "",                             "" },

	// thermal inputs
	{ SSC_INPUT,        SSC_NUMBER,     "batt_mass",                                   "Battery mass",                                           "kg",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_length",                                 "Battery length",                                         "m",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_width",                                  "Battery width",                                          "m",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_height",                                 "Battery height",                                         "m",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_Cp",                                     "Battery specific heat capacity",                         "J/KgK",    "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_h_to_ambient",                           "Heat transfer between battery and environment",          "W/m2K",    "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_room_temperature_celsius",               "Temperature of storage room",                            "C",        "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_MATRIX,     "cap_vs_temp",                                 "Effective capacity as function of temperature",          "C,%",      "",                     "Battery",       "",                           "",                             "" },

	// storage dispatch
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_charge",                      "Periods 1-6 charging from system allowed?",              "",         "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_fuelcellcharge",			     "Periods 1-6 charging from fuel cell allowed?",           "",         "",                     "FuelCell",     "",                        "",                              "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_discharge",                   "Periods 1-6 discharging allowed?",                       "",         "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_gridcharge",                  "Periods 1-6 grid charging allowed?",                     "",         "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_percent_discharge",           "Periods 1-6 discharge percent",                          "%",        "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_manual_percent_gridcharge",          "Periods 1-6 gridcharge percent",                         "%",        "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_MATRIX,     "dispatch_manual_sched",                       "Battery dispatch schedule for weekday",                  "",         "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_MATRIX,     "dispatch_manual_sched_weekend",               "Battery dispatch schedule for weekend",                  "",         "",                     "Battery",       "en_batt=1&batt_dispatch_choice=4",                           "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_target_power",                           "Grid target power for every time step",                  "kW",       "",                     "Battery",       "en_batt=1&batt_meter_position=0&batt_dispatch_choice=2",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_target_power_monthly",                   "Grid target power on monthly basis",                     "kW",       "",                     "Battery",       "en_batt=1&batt_meter_position=0&batt_dispatch_choice=2",                        "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_target_choice",                          "Target power input option",                              "0/1",      "0=InputMonthlyTarget,1=InputFullTimeSeries", "Battery", "en_batt=1&batt_meter_position=0&batt_dispatch_choice=2",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_custom_dispatch",                        "Custom battery power for every time step",               "kW",       "",                     "Battery",       "en_batt=1&batt_dispatch_choice=3","",                         "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_choice",                        "Battery dispatch algorithm",                             "0/1/2/3/4", "If behind the meter: 0=PeakShavingLookAhead,1=PeakShavingLookBehind,2=InputGridTarget,3=InputBatteryPower,4=ManualDispatch, if front of meter: 0=AutomatedLookAhead,1=AutomatedLookBehind,2=AutomatedInputForecast,3=InputBatteryPower,4=ManualDispatch",                    "Battery",       "en_batt=1",                        "",                             "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_pv_clipping_forecast",                   "PV clipping forecast",                                   "kW",       "",                     "Battery",       "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",          "" },
	{ SSC_INPUT,        SSC_ARRAY,      "batt_pv_dc_forecast",                         "PV dc power forecast",                                   "kW",       "",                     "Battery",       "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",          "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_auto_can_fuelcellcharge",       "Charging from fuel cell allowed for automated dispatch?",          "kW",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_auto_can_gridcharge",           "Grid charging allowed for automated dispatch?",          "kW",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_auto_can_charge",               "PV charging allowed for automated dispatch?",            "kW",       "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_auto_can_clipcharge",           "Battery can charge from clipped PV for automated dispatch?", "kW",   "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_auto_gridcharge_max_daily",              "Allowed grid charging percent per day for automated dispatch","kW",  "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_look_ahead_hours",                       "Hours to look ahead in automated dispatch",              "hours",    "",                     "Battery",       "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_dispatch_update_frequency_hours",        "Frequency to update the look-ahead dispatch",            "hours",    "",                     "Battery",       "",                           "",                             "" },

	//  cycle cost inputs
	{ SSC_INPUT,        SSC_NUMBER,     "batt_cycle_cost_choice",                      "Use SAM model for cycle costs or input custom",           "0/1",     "0=UseCostModel,1=InputCost", "Battery", "",                           "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "batt_cycle_cost",                             "Input battery cycle costs",                               "$/cycle-kWh","",                  "Battery",       "",                           "",                             "" },

	// Utility rate inputs
	{ SSC_INOUT,        SSC_NUMBER,     "en_electricity_rates",                        "Enable Electricity Rates",                                "0/1",     "0=EnableElectricityRates,1=NoRates",    "ElectricityRate",   "",                                   "",                             "" },
	{ SSC_INPUT,        SSC_NUMBER,     "ur_en_ts_sell_rate",                          "Enable time step sell rates",                             "0/1",     "",                       "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "BOOLEAN",                           "" },
	//{ SSC_INPUT,        SSC_ARRAY,      "ur_ts_sell_rate",                             "Time step sell rates",                                    "0/1",     "",                       "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",                                  "" },
	{ SSC_INPUT,        SSC_ARRAY,      "ur_ts_buy_rate",                              "Time step buy rates",                                     "0/1",     "",                       "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",                                  "" },
	{ SSC_INPUT,        SSC_MATRIX,     "ur_ec_sched_weekday",                         "Energy charge weekday schedule",                          "",        "12 x 24 matrix",         "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",          "" },
	{ SSC_INPUT,        SSC_MATRIX,     "ur_ec_sched_weekend",                         "Energy charge weekend schedule",                          "",        "12 x 24 matrix",         "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",          "" },
	{ SSC_INPUT,        SSC_MATRIX,     "ur_ec_tou_mat",                               "Energy rates table",                                      "",        "",                       "ElectricityRate",              "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2",  "",          "" },



	// PPA financial inputs
	{ SSC_INPUT,        SSC_ARRAY,      "ppa_price_input",		                        "PPA Price Input",	                                        "",      "",                  "Time of Delivery", "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2"   "",          "" },
	{ SSC_INPUT,        SSC_NUMBER,     "ppa_multiplier_model",                         "PPA multiplier model",                                    "0/1",    "0=diurnal,1=timestep","Time of Delivery", "?=0",                                                  "INTEGER,MIN=0", "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_factors_ts",                          "Dispatch payment factor time step",                        "",      "",                  "Time of Delivery", "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2&ppa_multiplier_model=1", "", "" },
	{ SSC_INPUT,        SSC_ARRAY,      "dispatch_tod_factors",		                    "TOD factors for periods 1-9",	                            "",      "",                  "Time of Delivery", "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2&ppa_multiplier_model=0"   "",          "" },
	{ SSC_INPUT,        SSC_MATRIX,     "dispatch_sched_weekday",                       "Diurnal weekday TOD periods",                              "1..9",  "12 x 24 matrix",    "Time of Delivery", "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2&ppa_multiplier_model=0",  "",          "" },
	{ SSC_INPUT,        SSC_MATRIX,     "dispatch_sched_weekend",                       "Diurnal weekend TOD periods",                              "1..9",  "12 x 24 matrix",    "Time of Delivery", "en_batt=1&batt_meter_position=1&batt_dispatch_choice=2&ppa_multiplier_model=0",  "",          "" },

	// Powerflow calculation inputs
	{ SSC_INPUT,       SSC_ARRAY,       "fuelcell_power",                               "Electricity from fuel cell",                            "kW",       "",                     "FuelCell",     "",                           "",                         "" },


	var_info_invalid
};

var_info vtab_battery_outputs[] = {
	// Capacity, Voltage, Charge outputs
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_q0",                                    "Battery total charge",                                   "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_q1",                                    "Battery available charge",                               "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_q2",                                    "Battery bound charge",                                   "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_SOC",                                   "Battery state of charge",                                "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_DOD",                                   "Battery cycle depth of discharge",                       "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_qmaxI",                                 "Battery maximum capacity at current",                    "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_qmax",                                  "Battery maximum charge with degradation",                "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_qmax_thermal",                          "Battery maximum charge at temperature",                  "Ah",       "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_I",                                     "Battery current",                                        "A",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_voltage_cell",                          "Battery cell voltage",                                   "V",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_voltage",                               "Battery voltage",	                                     "V",        "",                     "Battery",       "",                           "",                              "" },
																		               
	// Lifecycle related outputs											             
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_DOD_cycle_average",                     "Battery average cycle DOD",                              "",         "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_cycles",                                "Battery number of cycles",                               "",         "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_temperature",                           "Battery temperature",                                    "C",        "",                     "Battery",       "",                           "",                              "" }, 
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_capacity_percent",                      "Battery relative capacity to nameplate",                 "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_capacity_percent_cycle",                "Battery relative capacity to nameplate (cycling)",       "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_capacity_percent_calendar",             "Battery relative capacity to nameplate (calendar)",      "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_capacity_thermal_percent",              "Battery capacity percent for temperature",               "%",        "",                     "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_bank_replacement",                      "Battery bank replacements per year",                     "number/year", "",                  "Battery",       "",                           "",                              "" },
																			          
	// Power outputs at native timestep												        
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_power",                                 "Electricity to/from battery",                           "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "grid_power",                                 "Electricity to/from grid",                              "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "pv_to_load",                                 "Electricity to load from PV",                           "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_to_load",                               "Electricity to load from battery",                      "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "grid_to_load",                               "Electricity to load from grid",                         "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "pv_to_batt",                                 "Electricity to battery from PV",                        "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "fuelcell_to_batt",                           "Electricity to battery from fuel cell",                 "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "grid_to_batt",                               "Electricity to battery from grid",                      "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "pv_to_grid",                                 "Electricity to grid from PV",                           "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_to_grid",                               "Electricity to grid from battery",                      "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_conversion_loss",                       "Electricity loss in battery power electronics",         "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_system_loss",                           "Electricity loss from battery ancillary equipment",     "kW",      "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "grid_power_target",                          "Electricity grid power target for automated dispatch","kW","",                               "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_power_target",                          "Electricity battery power target for automated dispatch","kW","",                            "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_cost_to_cycle",                         "Battery computed cost to cycle",                                "$/cycle", "",                       "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "market_sell_rate_series_yr1",                "Market sell rate (Year 1)",                             "$/MWh", "",                         "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_revenue_gridcharge",					   "Revenue to charge from grid",                           "$/kWh", "",                         "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_revenue_charge",                        "Revenue to charge from system",                         "$/kWh", "",                         "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_revenue_clipcharge",                    "Revenue to charge from clipped",                        "$/kWh", "",                         "Battery",       "",                           "",                              "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_revenue_discharge",                     "Revenue to discharge",                                  "$/kWh", "",                         "Battery",       "",                           "",                              "" },

	// monthly outputs
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_pv_to_load",                         "Energy to load from PV",                                "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_batt_to_load",                       "Energy to load from battery",                           "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_grid_to_load",                       "Energy to load from grid",                              "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_pv_to_grid",                         "Energy to grid from PV",                                "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_batt_to_grid",                       "Energy to grid from battery",                           "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_pv_to_batt",                         "Energy to battery from PV",                             "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "monthly_grid_to_batt",                       "Energy to battery from grid",                           "kWh",      "",                      "Battery",       "",                          "LENGTH=12",                     "" },

	// annual metrics
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_charge_from_pv",                 "Battery annual energy charged from PV",                 "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_charge_from_grid",               "Battery annual energy charged from grid",               "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_charge_energy",                  "Battery annual energy charged",                         "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_discharge_energy",               "Battery annual energy discharged",                      "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_energy_loss",                    "Battery annual energy loss",                            "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "batt_annual_energy_system_loss",             "Battery annual system energy loss",                     "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "annual_export_to_grid_energy",               "Annual energy exported to grid",                        "kWh",      "",                      "Battery",       "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_ARRAY,      "annual_import_to_grid_energy",               "Annual energy imported from grid",                      "kWh",      "",                      "Battery",       "",                           "",                               "" },
	
	// single value metrics
	{ SSC_OUTPUT,        SSC_NUMBER,     "average_battery_conversion_efficiency",      "Battery average cycle conversion efficiency",           "%",        "",                      "Annual",        "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_NUMBER,     "average_battery_roundtrip_efficiency",       "Battery average roundtrip efficiency",                  "%",        "",                      "Annual",        "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_NUMBER,     "batt_pv_charge_percent",                     "Battery percent energy charged from PV",                "%",        "",                      "Annual",        "",                           "",                               "" },
	{ SSC_OUTPUT,        SSC_NUMBER,     "batt_bank_installed_capacity",               "Battery bank installed capacity",                       "kWh",      "",                      "Annual",        "",                           "",                               "" },

	// test matrix output
	{ SSC_OUTPUT,        SSC_MATRIX,     "batt_dispatch_sched",                        "Battery dispatch schedule",                              "",        "",                     "Battery",       "",                           "",                               "ROW_LABEL=MONTHS,COL_LABEL=HOURS_OF_DAY"  },
	

var_info_invalid };

///////////////////////////////////////////////////
static var_info _cm_vtab_battery[] = {
	/*   VARTYPE           DATATYPE         NAME                                             LABEL                                                   UNITS      META                           GROUP                  REQUIRED_IF                 CONSTRAINTS                      UI_HINTS*/
	{ SSC_INOUT,        SSC_NUMBER,      "percent_complete",                           "Estimated simulation status",                             "%",          "",                     "Simulation",                        "",                            "",                               "" },
	{ SSC_INPUT,        SSC_NUMBER,      "system_use_lifetime_output",                 "Lifetime simulation",                                     "0/1",       "0=SingleYearRepeated,1=RunEveryYear",   "Simulation",        "?=0",                   "BOOLEAN",                              "" },
	{ SSC_INPUT,        SSC_NUMBER,      "analysis_period",                            "Lifetime analysis period",                                "years",     "The number of years in the simulation", "Simulation",        "system_use_lifetime_output=1","",                               "" },
	{ SSC_INPUT,        SSC_NUMBER,      "en_batt",                                    "Enable battery storage model",                            "0/1",        "",                     "Battery",                      "?=0",                    "",                               "" },
	{ SSC_INOUT,        SSC_ARRAY,       "gen",										   "System power generated",                                  "kW",         "",                     "System",                             "",                       "",                               "" },
	{ SSC_INPUT,		SSC_ARRAY,	     "load",			                           "Electricity load (year 1)",                               "kW",	        "",				        "ElectricLoad",                             "",	                      "",	                            "" },
	{ SSC_INPUT,        SSC_NUMBER,      "batt_replacement_option",                    "Enable battery replacement?",                              "0=none,1=capacity based,2=user schedule", "", "Battery",             "?=0",                    "INTEGER,MIN=0,MAX=2",           "" },
	{ SSC_INOUT,        SSC_NUMBER,      "capacity_factor",                            "Capacity factor",                                         "%",          "",                     "System",                             "?=0",                    "",                               "" },
	{ SSC_INOUT,        SSC_NUMBER,      "annual_energy",                              "Annual Energy",                                           "kWh",        "",                     "Battery",                      "?=0",                    "",                               "" },
																																																											      
	// other variables come from battstor common table
	var_info_invalid };

void process_messages(battstor* batt, compute_module* cm)
{
    message dispatch_messages = batt->dispatch_model->get_messages();

    for (int i = 0; i != (int)dispatch_messages.total_message_count(); i++)
        cm->log(dispatch_messages.construct_log_count_string(i), SSC_NOTICE);
}

class cm_battery : public compute_module
{
public:

	cm_battery()
	{
		add_var_info(_cm_vtab_battery);
		add_var_info( vtab_battery_inputs);
		add_var_info(vtab_battery_outputs);
	}

    void exec() override
	{
		if (as_boolean("en_batt"))
		{
			// System generation output, which is lifetime (if system_lifetime_output == true);
			std::vector<ssc_number_t> power_input_lifetime = as_vector_ssc_number_t("gen");
			std::vector<ssc_number_t> load_lifetime, load_year_one;
			size_t n_rec_lifetime = power_input_lifetime.size();
			size_t n_rec_single_year;
			double dt_hour_gen;
			if (is_assigned("load")) {
				load_year_one = as_vector_ssc_number_t("load");
			}

			 single_year_to_lifetime_interpolated<ssc_number_t>(
				(bool)as_integer("system_use_lifetime_output"),
				(size_t)as_integer("analysis_period"),
				n_rec_lifetime, 
				load_year_one,
				load_lifetime,
				n_rec_single_year,
				dt_hour_gen);

			// Create battery structure and initialize
			battstor batt(*m_vartab, true, n_rec_single_year, dt_hour_gen);
			batt.initialize_automated_dispatch(power_input_lifetime, load_lifetime);

			if (load_lifetime.size() != n_rec_lifetime) {
				throw exec_error("battery", "Load length does not match system generation length");
			}
			if (batt.batt_vars->batt_topology == ChargeController::DC_CONNECTED) {
				batt.batt_vars->batt_topology = ChargeController::AC_CONNECTED;
				throw exec_error("battery", "Generic System must be AC connected to battery");
			}
			
			// Prepare outputs
			ssc_number_t * p_gen = allocate("gen", n_rec_lifetime);
			double capacity_factor_in, annual_energy_in, nameplate_in;
			capacity_factor_in = annual_energy_in = nameplate_in = 0;

			if (is_assigned("capacity_factor") && is_assigned("annual_energy")) {
				capacity_factor_in = as_double("capacity_factor");
				annual_energy_in = as_double("annual_energy");
				nameplate_in = (annual_energy_in / (capacity_factor_in * 0.01)) / util::hours_per_year;
			}
	
			
			/* *********************************************************************************************
			Run Simulation
			*********************************************************************************************** */
			double annual_energy = 0;
			float percent_complete = 0.0;
			float percent = 0.0;
			size_t nStatusUpdates = 50;

			if (is_assigned("percent_complete")) {
				percent_complete = as_float("percent_complete");
			}

			size_t lifetime_idx = 0;
			for (size_t year = 0; year != batt.nyears; year++)
			{
				for (size_t hour = 0; hour < 8760; hour++)
				{
					// status bar
					if (hour % (8760 / nStatusUpdates) == 0)
					{
						// assume that anyone using this module is chaining with two techs
						float techs = 3;
						percent = percent_complete + 100.0f * ((float)lifetime_idx + 1) / ((float)n_rec_lifetime) / techs;
						if (!update("", percent, (float)hour)) {
							throw exec_error("battery", "simulation canceled at hour " + util::to_string(hour + 1.0));
						}
					}
					for (size_t jj = 0; jj < batt.step_per_hour; jj++)
					{
	
						batt.initialize_time(year, hour, jj);
						batt.advance(m_vartab, power_input_lifetime[lifetime_idx], 0, load_lifetime[lifetime_idx], 0);
						p_gen[lifetime_idx] = batt.outGenPower[lifetime_idx];
						if (year == 0) {
							annual_energy += p_gen[lifetime_idx] * batt._dt_hour;
						}
						lifetime_idx++;
					}
				}
			}
            process_messages(&batt, this);
            batt.calculate_monthly_and_annual_outputs(*this);

			// update capacity factor and annual energy
			assign("capacity_factor", var_data(static_cast<ssc_number_t>(annual_energy * 100.0 / (nameplate_in * util::hours_per_year))));
			assign("annual_energy", var_data(static_cast<ssc_number_t>(annual_energy)));
			assign("percent_complete", var_data((ssc_number_t)percent));
		}
		else
			assign("average_battery_roundtrip_efficiency", var_data((ssc_number_t)0.));
	}
};

DEFINE_MODULE_ENTRY(battery, "Battery storage standalone model .", 10)
