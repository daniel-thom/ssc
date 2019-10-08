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

#include <cmath>
#include <memory>
#include <numeric>

#include "common.h"
#include "core.h"

#include "cmod_grid.h"

var_info vtab_grid_input[] = {
	/*   VARTYPE           DATATYPE         NAME                               LABEL                                    UNITS      META                   GROUP                  REQUIRED_IF                 CONSTRAINTS                      UI_HINTS*/
	// simulation inputs
	{ SSC_INPUT,        SSC_NUMBER,      "system_use_lifetime_output",        "Lifetime simulation",                   "0/1",     "0=SingleYearRepeated,1=RunEveryYear",   "Lifetime",        "?=0",                   "BOOLEAN",                          "" },
	{ SSC_INPUT,        SSC_NUMBER,      "analysis_period",                   "Lifetime analysis period",              "years",   "The number of years in the simulation", "Lifetime",        "system_use_lifetime_output=1","",                           "" },
	{ SSC_INPUT,        SSC_NUMBER,      "enable_interconnection_limit",      "Enable grid interconnection limit",     "0/1",     "Enable a grid interconnection limit",   "Common",        "","",                           "" },
	{ SSC_INPUT,        SSC_NUMBER,      "grid_interconnection_limit_kwac",   "Grid interconnection limit",            "kWac",    "The number of years in the simulation", "Common",        "","",                           "" },

	// external compute module inputs
	{ SSC_INOUT,        SSC_ARRAY,       "gen",								  "System power generated",                "kW",        "Lifetime system generation",          "Common",                  "",                        "",                              "" },
	{ SSC_INPUT,		SSC_ARRAY,	     "load",			                  "Electricity load (year 1)",             "kW",	    "",                                    "Common",	                       "",	                      "",	                           "" },

var_info_invalid };

var_info vtab_grid_output[] = {

	{ SSC_OUTPUT,        SSC_ARRAY,       "system_pre_interconnect_kwac",     "System power before grid interconnect",  "kW",       "Lifetime system generation" "",                 "",                        "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "capacity_factor_interconnect_ac",  "Capacity factor of the interconnection (year 1)",  "%",          "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_energy_pre_interconnect_ac", "Annual Energy AC pre-interconnection (year 1)",   "kWh",        "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_energy",                    "Annual Energy AC (year 1)",                        "kWh",        "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_ac_interconnect_loss_percent","Annual Energy loss from interconnection limit (year 1)", "%", "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_ac_interconnect_loss_kwh",   "Annual Energy loss from interconnection limit (year 1)", "kWh", "",                "",                           "?=0",                     "",                              "" },

	{ SSC_OUTPUT,        SSC_ARRAY,       "system_pre_curtailment_kwac",     "System power before grid curtailment",  "kW",       "Lifetime system generation" "",                 "",                        "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "capacity_factor_curtailment_ac",  "Capacity factor of the curtailment (year 1)",  "%",          "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_energy_pre_curtailment_ac", "Annual Energy AC pre-curtailment (year 1)",   "kWh",        "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_energy",                    "Annual Energy AC  (year 1)",                        "kWh",        "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_ac_curtailment_loss_percent","Annual Energy loss from curtailment (year 1)", "%", "",                "",                           "?=0",                     "",                              "" },
	{ SSC_OUTPUT,        SSC_NUMBER,      "annual_ac_curtailment_loss_kwh",   "Annual Energy loss from curtailment (year 1)", "kWh", "",                "",                           "?=0",                     "",
			  "" },

var_info_invalid };


cm_grid::cm_grid()
{
	add_var_info(vtab_grid_input);
	add_var_info(vtab_grid_output);
	add_var_info(vtab_technology_outputs);
	add_var_info(vtab_grid_curtailment);
}

// Have to add this since compute module isn't actually fully constructed until compute is called with
// a vartable.
void cm_grid::construct()
{
	std::unique_ptr<gridVariables> tmp(new gridVariables(*this));
	gridVars = std::move(tmp);
	allocateOutputs();
}

void cm_grid::exec() throw (general_error)
{
	construct();

	// interconnection  calculations
	double capacity_factor_interconnect, annual_energy_pre_interconnect, annual_energy_pre_curtailment, annual_energy;
	capacity_factor_interconnect = annual_energy_pre_interconnect = annual_energy_pre_curtailment = annual_energy = 0;

//	annual_energy_pre_interconnect = std::accumulate(gridVars->systemGenerationLifetime_kW.begin(), gridVars->systemGenerationLifetime_kW.begin() + gridVars->numberOfSingleYearRecords, (double)0.0)*gridVars->dt_hour_gen;

	size_t hour = 0;
	size_t num_steps_per_hour = size_t(1.0 / gridVars->dt_hour_gen);
	// compute grid export, apply interconnection limit
	for (size_t i = 0; i < gridVars->numberOfLifetimeRecords; i++) 
	{
		double gridNet = gridVars->systemGenerationLifetime_kW[i] - gridVars->loadLifetime_kW[i];
		double gridNetInterconnectionLimited = gridNet;

		p_genPreInterconnect_kW[i] = static_cast<ssc_number_t>(gridVars->systemGenerationLifetime_kW[i]);

		if (gridVars->enable_interconnection_limit)
			gridNetInterconnectionLimited = fmin(gridNet, gridVars->grid_interconnection_limit_kW);
		double interconnectionLimited = gridNet - gridNetInterconnectionLimited;
		gridVars->systemGenerationLifetime_kW[i] -= interconnectionLimited;

		p_genPreCurtailment_kW[i] = static_cast<ssc_number_t>(gridVars->systemGenerationLifetime_kW[i]);

		// compute curtailment
		gridVars->systemGenerationLifetime_kW[i] *= (1.0 - gridVars->gridCurtailmentLifetime_percent[i]/100.0);

		p_genGrid_kW[i] = static_cast<ssc_number_t>(gridVars->systemGenerationLifetime_kW[i]);

		if (i < gridVars->numberOfSingleYearRecords)
		{
			annual_energy_pre_interconnect += p_genPreInterconnect_kW[i];
			annual_energy_pre_curtailment += p_genPreCurtailment_kW[i];
			annual_energy += p_genGrid_kW[i];
		}


		if (((i + 1) % gridVars->numberOfSingleYearRecords) == 0)
			hour = 0;
		else if (((i + 1) % num_steps_per_hour) == 0)
			hour++;

	}


	annual_energy_pre_interconnect *= gridVars->dt_hour_gen;
	annual_energy_pre_curtailment *= gridVars->dt_hour_gen;
	annual_energy *= gridVars->dt_hour_gen;


//	annual_energy_interconnect = std::accumulate(gridVars->systemGenerationLifetime_kW.begin(), gridVars->systemGenerationLifetime_kW.begin() + gridVars->numberOfSingleYearRecords, (double)0.0)*gridVars->dt_hour_gen;
	capacity_factor_interconnect = annual_energy_pre_curtailment * util::fraction_to_percent / (gridVars->grid_interconnection_limit_kW * 8760.);




	assign("capacity_factor_interconnect_ac", var_data(capacity_factor_interconnect));
	assign("annual_energy_pre_interconnect_ac", var_data(annual_energy_pre_interconnect));
	assign("annual_energy_pre_curtailment_ac", var_data(annual_energy_pre_curtailment));
	assign("annual_energy", var_data(annual_energy)); 
	assign("annual_ac_interconnect_loss_kwh", var_data(std::roundf(annual_energy_pre_interconnect - annual_energy_pre_curtailment)));
	assign("annual_ac_interconnect_loss_percent", var_data(100.0*(annual_energy_pre_interconnect - annual_energy_pre_curtailment)/ annual_energy_pre_interconnect));

}

void cm_grid::allocateOutputs()
{
	p_genGrid_kW = allocate("gen", gridVars->systemGenerationLifetime_kW.size());
	p_genPreCurtailment_kW = allocate("system_pre_curtailment_kwac", gridVars->systemGenerationLifetime_kW.size());
	p_genPreInterconnect_kW = allocate("system_pre_interconnect_kwac", gridVars->systemGenerationLifetime_kW.size());
}

DEFINE_MODULE_ENTRY(grid, "Grid model", 1)
