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

#ifndef _battery_controllers_h_
#define _battery_controllers_h_

// Required due to need for complete type in std::unique_ptr<>
#include "lib_shared_inverter.h"
#include "lib_dispatch.h"
#include "lib_battery_metrics.h"
#include "lib_dispatch_params.h"

class bidir_inverter
{
public:

    bidir_inverter(double ac_dc_efficiency, double dc_ac_efficiency)
    {
        _dc_ac_efficiency = 0.01*dc_ac_efficiency;
        _ac_dc_efficiency = 0.01*ac_dc_efficiency;
    }

    double dc_ac_efficiency() { return _dc_ac_efficiency; }
    double ac_dc_efficiency() { return _ac_dc_efficiency; }


    // return power loss [kW]
    double convert_to_dc(double P_ac, double * P_dc);
    double convert_to_ac(double P_dc, double * P_ac);

    // return increased power required, i.e 9 kWac may require 10 kWdc
    double compute_dc_from_ac(double P_ac);


protected:
    double _dc_ac_efficiency;
    double _ac_dc_efficiency;

    double _loss_dc_ac;
    double _loss_ac_dc;
};

class DC_DC_charge_controller
{
public:

    DC_DC_charge_controller(double batt_dc_dc_bms_efficiency, double pv_dc_dc_mppt_efficiency)
    {
        _batt_dc_dc_bms_efficiency = 0.01*batt_dc_dc_bms_efficiency;
        _pv_dc_dc_mppt_efficiency = 0.01*pv_dc_dc_mppt_efficiency;

    }

    double batt_dc_dc_bms_efficiency() { return _batt_dc_dc_bms_efficiency; };
    double pv_dc_dc_mppt_efficiency() { return _pv_dc_dc_mppt_efficiency; };


protected:
    double _batt_dc_dc_bms_efficiency;
    double _pv_dc_dc_mppt_efficiency;
    double _loss_dc_dc;
};

class battery_rectifier
{
public:
    // don't know if I need this component in AC or DC charge controllers
    battery_rectifier(double ac_dc_efficiency) { _ac_dc_efficiency = 0.01 * ac_dc_efficiency; }
    double ac_dc_efficiency() { return _ac_dc_efficiency; }

    // return power loss [kW]
    double convert_to_dc(double P_ac, double * P_dc);

protected:
    double _ac_dc_efficiency;
    double _loss_dc_ac;
};

/**
*
* \class charge_controller_interface
*
*  A charge_controller is a mostly abstract base class which defines the structure of battery charge controllers
*  The charge_controller requires information about the battery dispatch.  At every time step, the charge controller
*  runs the dispatch model and returns to the calling function.  While the charge_controller may seem to be an unnecessary
*  step in the calculation, it provides a location to store the initial dispatch in a timestep, and iterate upon that dispatch
*  if necessary.  It also houses the framework of battery power electronics components, which currently have single-point
*  efficiencies, but could be expanded to have more detailed models.
*/
class charge_controller
{
public:
    /// Construct an charge_controller with a dispatch object, battery metrics object
    charge_controller();

    /// Virtual destructor for charge_controller
    virtual ~charge_controller() {};

    /// Virtual method to run the charge controller given the current timestep, PV production, and load
    virtual void run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t index) = 0;

    double get_V(){return battery_model->get_V();}

    double get_time_at_mode(){return time_at_mode;}

    const std::shared_ptr<const battery_controller_params> params;
protected:

    std::shared_ptr<battery> battery_model;

    std::shared_ptr<SharedInverter> inverter;

    double charging_mode;
    int time_at_mode;

    /// Helper function to run common dispatch tasks.  Requires that m_batteryPower->powerBattery is previously defined
    double runDispatch(const storage_time_state &time, double power_DC);

    /// Method to check any operational constraints and modify the battery current if needed
    bool check_constraints(double &I, size_t count);

    // Controllers
    void SOC_controller(double& power_DC);
    void switch_controller(double& power_DC);
    double current_controller(double& power_DC);
    bool restrict_current(double &I);
    bool restrict_power(double &I);

    double power_DC_to_AC(double power_DC){
        if (power_DC == 0)
            return 0.;
        if (params->connection == battery_controller_params::AC){
            if (power_DC < 0)
                return power_DC / (params->pvcharge_eff / 100.);
            else if (power_DC > 0)
                return power_DC * (params->discharge_eff / 100.);
        }
        else{
            if (power_DC < 0)
                return power_DC * (inverter->efficiencyAC / 100.);
            else
                return power_DC / (inverter->efficiencyAC / 100.);
        }
    }
};
//
///**
//*
//* \class AC_charge_controller
//*
//*  A AC_charge_controller is derived from the charge_controller, and contains information specific to a battery connected on the AC-side
//*  of a power generating source.  It requires information about the efficiency to convert power from AC to DC and back to AC.
//*/
//class AC_charge_controller : public charge_controller
//{
//public:
//    /// Construct an AC_charge_controller with a dispatch object, battery metrics object, and single-point efficiencies for the battery bidirectional inverter
//    AC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyACToDC, double efficiencyDCToAC);
//
//    /// Destroy the AC_charge_controller
//    ~AC_charge_controller() {};
//
//    /// Runs the battery dispatch model with the current PV and Load information
//    void run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t index);
//
//private:
//    // allocated and managed internally
//    std::unique_ptr<bidir_inverter> m_bidirectionalInverter;  /// Model for the battery bi-directional inverter
//
//    // allocated and managed internally
//    std::unique_ptr<dispatch_interface> m_dispatchInitial;	/// An internally managed copy of the initial dispatch of the timestep
//
//    // memory managed elsewhere
//    BatteryPower * m_batteryPower;
//    battery_metrics *m_batteryMetrics;    /// An object that tracks battery metrics for later analysis
//    dispatch_interface *m_dispatch;		/// An object containing the framework to run a battery and check operational constraints
//};
//
///**
//*
//* \class DC_charge_controller
//*
//*  A DC_charge_controller is derived from the charge_controller, and contains information specific to a battery connected on the DC-side
//*  of a power generating source.  It requires information about the efficiency to convert power from DC to DC between the battery and DC source
//*/
//class DC_charge_controller : public charge_controller
//{
//public:
//    /// Construct a DC_charge_controller with a dispatch object, battery metrics object, and single-point efficiency for the battery charge controller
//    DC_charge_controller(dispatch_interface *dispatch, battery_metrics * battery_metrics, double efficiencyDCToDC, double inverterEfficiencyCutoff);
//
//    /// Destroy the DC_charge_controller object
//    ~DC_charge_controller() {};
//
//    /// Sets the shared inverter used by the PV and battery system
//    void setSharedInverter(SharedInverter * sharedInverter);
//
//    /// Runs the battery dispatch model with the current PV and Load information
//    void run(size_t year, size_t hour_of_year, size_t step_of_hour, size_t index);
//
//private:
//
//    // allocated and managed internally
//    std::unique_ptr<DC_DC_charge_controller> m_DCDCcharge_controller;  /// Model for the battery DC/DC charge controller with Battery Management System
//
//    // allocated and managed internally
//    std::unique_ptr<dispatch_interface> m_dispatchInitial;	/// An internally managed copy of the initial dispatch of the timestep
//
//    // memory managed elsewhere
//    BatteryPower * m_batteryPower;
//    battery_metrics *m_batteryMetrics;    /// An object that tracks battery metrics for later analysis
//    dispatch_interface *m_dispatch;		/// An object containing the framework to run a battery and check operational constraints
//};
//

#endif
