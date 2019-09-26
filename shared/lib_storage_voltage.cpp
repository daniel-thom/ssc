#include "lib_storage_capacity.h"
#include "lib_storage_voltage.h"


/*
Define Voltage Model
*/

// Voltage Table
voltage_table_t::voltage_table_t(int n_cell_series, int n_strings, double voltage, double resistance, util::matrix_t<double> voltage_matrix) :
        batt_voltage_matrix(std::move(voltage_matrix))
{
    num_cells_series = n_cell_series;
    num_strings = n_strings;
    cell_voltage_nominal = voltage;
    cell_voltage_state = voltage;
    R = resistance;
    for (int r = 0; r != (int)batt_voltage_matrix.nrows(); r++)
        voltage_table.emplace_back(table_point(batt_voltage_matrix.at(r, 0), batt_voltage_matrix.at(r, 1)));

    std::sort(voltage_table.begin(), voltage_table.end(), byDOD());
}

voltage_table_t * voltage_table_t::clone(){ return new voltage_table_t(*this); }

void voltage_table_t::copy(voltage_interface * voltage)
{
    if (auto * tmp = dynamic_cast<voltage_table_t*>(voltage))
    {
        num_strings = tmp->num_strings;
        cell_voltage_state = tmp->cell_voltage_state;
        R = tmp->R;
        voltage_table = tmp->voltage_table;
    }
}


void voltage_table_t::updateVoltage(const capacity_state &capacity, ...)
{
    double cell_voltage = cell_voltage_state;
    double DOD = capacity.DOD;
    double I_string = capacity.I / num_strings;
    double DOD_lo = -1, DOD_hi = -1, V_lo = -1, V_hi = -1;
    bool voltage_found = exactVoltageFound(DOD, cell_voltage);
    if (!voltage_found)
    {
        prepareInterpolation(DOD_lo, V_lo, DOD_hi, V_hi, DOD);
        cell_voltage = util::interpolate(DOD_lo, V_lo, DOD_hi, V_hi, DOD) - I_string * R;
    }

    // the cell voltage should not increase when the battery is discharging
    if (I_string <= 0 || (I_string > 0 && cell_voltage <= cell_voltage))
        cell_voltage_state = cell_voltage;

}

bool voltage_table_t::exactVoltageFound(double DOD, double & V)
{
    bool contained = false;
    for (size_t r = 0; r != voltage_table.size(); r++)
    {
        if (voltage_table[r].DOD() == DOD)
        {
            V = voltage_table[r].V();
            contained = true;
            break;
        }
    }
    return contained;
}

void voltage_table_t::prepareInterpolation(double & DOD_lo, double & V_lo, double & DOD_hi, double & V_hi, double DOD)
{
    size_t nrows = voltage_table.size();
    DOD_lo = voltage_table[0].DOD();
    DOD_hi = voltage_table[nrows - 1].DOD();
    V_lo = voltage_table[0].V();
    V_hi = voltage_table[nrows - 1].V();

    for (size_t r = 0; r != nrows; r++)
    {
        double DOD_r = voltage_table[r].DOD();
        double V_r = voltage_table[r].V();

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


voltage_dynamic_t::voltage_dynamic_t(int n_cell_series, int n_strings, const battery_voltage_params* p)
{
    num_cells_series = n_cell_series;
    num_strings = n_strings;
    R = p->resistance;
    cell_voltage_nominal = p->Vnom_default;

    Vfull = p->Vfull;
    Vexp = p->Vexp;
    Vnom = p->Vnom;
    Qfull = p->Qfull;
    Qexp = p->Qexp;
    Qnom = p->Qnom;
    C_rate = p->C_rate;

    // assume fully charged, not the nominal value
    cell_voltage_state = Vfull;

    parameter_compute();
};
voltage_dynamic_t * voltage_dynamic_t::clone(){ return new voltage_dynamic_t(*this); }

void voltage_dynamic_t::copy(voltage_interface * voltage)
{
    if (auto * tmp = dynamic_cast<voltage_dynamic_t*>(voltage)){
        num_strings = tmp->num_strings;
        cell_voltage_state = tmp->cell_voltage_state;
        R = tmp->R;

        Vfull = tmp->Vfull;
        Vexp = tmp->Vexp;
        Vnom = tmp->Vnom;
        Qfull = tmp->Qfull;
        Qexp = tmp->Qexp;
        Qnom = tmp->Qnom;
        C_rate = tmp->C_rate;

        A = tmp->A;
        B0 = tmp->B0;
        E0 = tmp->E0;
        K = tmp->K;
    }

}
void voltage_dynamic_t::parameter_compute()
{
    // Determines parameters according to page 2 of:
    // Tremblay 2009 "A Generic Bettery Model for the Dynamic Simulation of Hybrid Electric Vehicles"
//	double eta = 0.995;
    double I = Qfull*C_rate; // [A]
    //R = _Vnom*(1. - eta) / (_C_rate*_Qnom); // [Ohm]
    A = Vfull - Vexp; // [V]
    B0 = 3. / Qexp;     // [1/Ah]
    K = ((Vfull - Vnom + A*(std::exp(-B0*Qnom) - 1))*(Qfull - Qnom)) / (Qnom); // [V] - polarization voltage
    E0 = Vfull + K + R * I - A;
}

void voltage_dynamic_t::updateVoltage(const capacity_state &capacity, ... )
{
    double Q = capacity.qmax;
    double I = capacity.I;
    double q0 = capacity.q0;

    // is on a per-cell basis.
    // I, Q, q0 are on a per-string basis since adding cells in series does not change current or charge
    double cell_voltage = voltage_model_tremblay_hybrid(Q / num_strings, I / num_strings , q0 / num_strings);

    // the cell voltage should not increase when the battery is discharging
    if (I <= 0 || (I > 0 && cell_voltage <= cell_voltage_state) )
        cell_voltage_state = cell_voltage;
}

double voltage_dynamic_t::voltage_model_tremblay_hybrid(double Q, double I, double q0)
{
    // everything in here is on a per-cell basis
    // Tremblay Dynamic Model
    double it = Q - q0;
    double E = E0 - K*(Q / (Q - it)) + A*exp(-B0*it);
    double V = E - R * I;

    // Discharged lower than model can handle ( < 1% SOC)
    if (V < 0 || !std::isfinite(V))
        V = 0.5*Vnom;
    else if (V > Vfull*1.25)
        V = Vfull;
    return V;
}

// Vanadium redox flow model
voltage_vanadium_redox_t::voltage_vanadium_redox_t(int n_cell_series, int n_strings, double V_ref_50_in,  double resistance)
{
    num_cells_series = n_cell_series;
    num_strings = n_strings;
    R = resistance;
    cell_voltage_nominal = V_ref_50_in;
    cell_voltage_state = V_ref_50_in;

    V_ref_50 = V_ref_50_in;
    I = 0;
    R_molar = 8.314;            // Molar gas constant [J/mol/K]^M
    F = 26.801 * 3600;          // Faraday constant [As/mol]^M
    C0 = 1.38;                  // model correction factor^M
}

voltage_vanadium_redox_t * voltage_vanadium_redox_t::clone(){ return new voltage_vanadium_redox_t(*this); }

void voltage_vanadium_redox_t::copy(voltage_interface * voltage)
{
    if (auto * tmp = dynamic_cast<voltage_vanadium_redox_t*>(voltage)){
        num_strings = tmp->num_strings;
        cell_voltage_state = tmp->cell_voltage_state;
        R = tmp->R;

        V_ref_50 = tmp->V_ref_50;
        R = tmp->R;
        I = tmp->I;
        R_molar = tmp->R_molar;
        F = tmp->F;
        C0 = tmp->C0;
    }

}

void voltage_vanadium_redox_t::updateVoltage(const capacity_state &capacity, double T_battery_K )
{
    double Q = capacity.qmax;
    I = capacity.I;
    double q0 = capacity.q0;

    double T = T_battery_K;

    // is on a per-cell basis.
    // I, Q, q0 are on a per-string basis since adding cells in series does not change current or charge
    double cell_voltage = voltage_model(Q / num_strings, q0 / num_strings, I/ num_strings, T);

    // the cell voltage should not increase when the battery is discharging
    if (I <= 0 || (I > 0 && cell_voltage <= cell_voltage_state))
        cell_voltage_state = cell_voltage;
}

double voltage_vanadium_redox_t::voltage_model(double qmax, double q0, double I_string, double T)
{
    double SOC = q0 / qmax;
    double SOC_use = SOC;
    if (SOC > 1 - tolerance)
        SOC_use = 1 - tolerance;

    double A = std::log(std::pow(SOC_use, 2) / std::pow(1 - SOC_use, 2));


    double V_cell = 0.;

    if (std::isfinite(A))
    {
        double V_stack_cell = V_ref_50 + (R_molar * T / F) * A * C0;
        V_cell = V_stack_cell - I_string * R;
    }
    return V_cell;
}
