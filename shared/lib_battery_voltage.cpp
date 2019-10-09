#include "lib_battery_capacity.h"
#include "lib_battery_voltage.h"

/*
Define Voltage Model
*/

// Voltage Table
voltage_table::voltage_table(const std::shared_ptr<const battery_voltage_params>& p):
params(p)
{
    cell_voltage_state = params->Vnom_default;
    for (int r = 0; r != (int)params->voltage_matrix.nrows(); r++)
        v_table.emplace_back(table_point(params->voltage_matrix.at(r, 0), params->voltage_matrix.at(r, 1)));

    std::sort(v_table.begin(), v_table.end(), byDOD());
}


voltage_table::voltage_table(const voltage_table& voltage):
params(voltage.params)
{
    cell_voltage_state = voltage.cell_voltage_state;
    v_table = voltage.v_table;
}


void voltage_table::updateVoltage(const capacity_state &capacity, double)
{
    double cell_voltage = cell_voltage_state;
    double DOD = capacity.DOD;
    double I_string = capacity.I / params->num_strings;
    double DOD_lo = -1, DOD_hi = -1, V_lo = -1, V_hi = -1;
    bool voltage_found = exactVoltageFound(DOD, cell_voltage);
    if (!voltage_found)
    {
        prepareInterpolation(DOD_lo, V_lo, DOD_hi, V_hi, DOD);
        cell_voltage = util::interpolate(DOD_lo, V_lo, DOD_hi, V_hi, DOD) - I_string * params->resistance;
    }

    // the cell voltage should not increase when the battery is discharging
    if (I_string <= 0 || (I_string > 0 && cell_voltage <= cell_voltage))
        cell_voltage_state = cell_voltage;

}

bool voltage_table::exactVoltageFound(double DOD, double & V)
{
    bool contained = false;
    for (size_t r = 0; r != v_table.size(); r++)
    {
        if (v_table[r].DOD() == DOD)
        {
            V = v_table[r].V();
            contained = true;
            break;
        }
    }
    return contained;
}

void voltage_table::prepareInterpolation(double & DOD_lo, double & V_lo, double & DOD_hi, double & V_hi, double DOD)
{
    size_t nrows = v_table.size();
    DOD_lo = v_table[0].DOD();
    DOD_hi = v_table[nrows - 1].DOD();
    V_lo = v_table[0].V();
    V_hi = v_table[nrows - 1].V();

    for (size_t r = 0; r != nrows; r++)
    {
        double DOD_r = v_table[r].DOD();
        double V_r = v_table[r].V();

        if (DOD_r <= DOD)
        {
            DOD_lo = DOD_r;
            V_lo = V_r;
        }

        if (DOD_r >= DOD)
        {
            DOD_hi = DOD_r;
            V_hi = V_r;
            break;
        }
    }
}


voltage_dynamic::voltage_dynamic(const std::shared_ptr<const battery_voltage_params>& p):
params(p)
{
    // assume fully charged, not the nominal value
    cell_voltage_state = params->Vfull;

    parameter_compute();
}


voltage_dynamic::voltage_dynamic(const voltage_dynamic& voltage):
params(voltage.get_params())
{
    cell_voltage_state = voltage.cell_voltage_state;
    A = voltage.A;
    B0 = voltage.B0;
    E0 = voltage.E0;
    K = voltage.K;
}
void voltage_dynamic::parameter_compute()
{
    // Determines parameters according to page 2 of:
    // Tremblay 2009 "A Generic Bettery Model for the Dynamic Simulation of Hybrid Electric Vehicles"
//	double eta = 0.995;
    double I = params->Qfull*params->C_rate; // [A]
    //R = _Vnom*(1. - eta) / (_C_rate*_Qnom); // [Ohm]
    A = params->Vfull - params->Vexp; // [V]
    B0 = 3. / params->Qexp;     // [1/Ah]
    K = ((params->Vfull - params->Vnom + A*(std::exp(-B0*params->Qnom) - 1))*(params->Qfull - params->Qnom)) / (params->Qnom); // [V] - polarization voltage
    E0 = params->Vfull + K + params->resistance * I - A;
}

void voltage_dynamic::updateVoltage(const capacity_state &capacity, double )
{
    double Q = capacity.qmax;
    double I = capacity.I;
    double q0 = capacity.q0;

    // is on a per-cell basis.
    // I, Q, q0 are on a per-string basis since adding cells in series does not change current or charge
    double cell_voltage = voltage_model_tremblay_hybrid(Q / params->num_strings, I / params->num_strings , q0 / params->num_strings);

    // the cell voltage should not increase when the battery is discharging
    if (I <= 0 || (I > 0 && cell_voltage <= cell_voltage_state) )
        cell_voltage_state = cell_voltage;
}

double voltage_dynamic::voltage_model_tremblay_hybrid(double Q, double I, double q0)
{
    // everything in here is on a per-cell basis
    // Tremblay Dynamic Model
    double it = Q - q0;
    double E = E0 - K*(Q / (Q - it)) + A*exp(-B0*it);
    double V = E - params->resistance * I;

    // Discharged lower than model can handle ( < 1% SOC)
    if (V < 0 || !std::isfinite(V))
        V = 0.5*params->Vnom;
    else if (V > params->Vfull*1.25)
        V = params->Vfull;
    return V;
}

// Vanadium redox flow model
voltage_vanadium_redox::voltage_vanadium_redox(const std::shared_ptr<const battery_voltage_params>& p):
params(p)
{
    cell_voltage_state = params->Vnom_default;
}

voltage_vanadium_redox::voltage_vanadium_redox(const voltage_vanadium_redox& voltage):
params(voltage.get_params())
{
        cell_voltage_state = voltage.cell_voltage_state;
}

void voltage_vanadium_redox::updateVoltage(const capacity_state &capacity, double T_battery_K )
{
    double Q = capacity.qmax;
    double I = capacity.I;
    double q0 = capacity.q0;

    double T = T_battery_K;

    // is on a per-cell basis.
    // I, Q, q0 are on a per-string basis since adding cells in series does not change current or charge
    double cell_voltage = voltage_model(Q / params->num_strings, q0 / params->num_strings, I/ params->num_strings, T);

    // the cell voltage should not increase when the battery is discharging
    if (I <= 0 || (I > 0 && cell_voltage <= cell_voltage_state))
        cell_voltage_state = cell_voltage;
}

double voltage_vanadium_redox::voltage_model(double qmax, double q0, double I_string, double T)
{
    double SOC = q0 / qmax;
    double SOC_use = SOC;
    if (SOC > 1 - tolerance)
        SOC_use = 1 - tolerance;

    double A = std::log(std::pow(SOC_use, 2) / std::pow(1 - SOC_use, 2));


    double V_cell = 0.;

    if (std::isfinite(A))
    {
        double V_stack_cell = params->Vnom_default + (R_molar * T / F) * A * C0;
        V_cell = V_stack_cell - I_string * params->resistance;
    }
    return V_cell;
}
